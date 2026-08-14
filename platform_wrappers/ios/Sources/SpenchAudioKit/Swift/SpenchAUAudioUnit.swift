import AudioToolbox
import AVFoundation
import QuartzCore
@_implementationOnly import SpenchAudioKit_Private

/// `AUAudioUnit` implementation that runs the Spench processing pipeline on a
/// dedicated worker thread.
///
/// The render callback only moves samples between two SPSC ring buffers and
/// tracks render-owned underrun state. DSP work happens on the worker.
///
/// ## Lifecycle
///
/// 1. The host calls ``allocateRenderResources()``. The unit rebuilds the
///    pipeline for the negotiated bus sample rate and starts the worker
///    thread. Settings cannot be changed past this point.
/// 2. The host renders. ``onLowPerformance`` may fire on the main queue if
///    the worker falls behind.
/// 3. The host calls ``deallocateRenderResources()``. The worker stops, ring
///    buffers reset, and the unit is ready to be reallocated.
///
/// See <doc:ThreadingAndLifecycle> for full threading guarantees.
public final class SpenchAUAudioUnit: AUAudioUnit {
    // The buses need a valid format before the host negotiates the real one.
    // This value is only a bootstrap and is replaced during allocation.
    private static let bootstrapSampleRate: Double = 48_000
    private static let processingChunkStepSeconds: Double = 0.016
    private static let maxOutputBufferDuration: Double = 0.256
    private static let meanProcessTimeCount = 60

    private let inputBus: AUAudioUnitBus
    private let outputBus: AUAudioUnitBus
    private var inputBusArray: AUAudioUnitBusArray!
    private var outputBusArray: AUAudioUnitBusArray!

    private var processor: SpenchProcessor
    private var currentSettings: SpenchSettings
    private var inputRing: RingBuffer
    private var outputRing: RingBuffer
    private var outputPrimed = false
    private var processingFailureReported = false
    private var lowPerformanceMonitor: SpenchLowPerformanceMonitor
    private let meanProcessingTimeStore = AtomicOptionalDouble()

    private var workerEvent: WorkerThreadEvent?
    private var workerFinishedEvent: DispatchSemaphore?
    private var workerThread: Thread?

    private var internalBlock: AUInternalRenderBlock!

    /// Features currently applied to the processor.
    public var features: SpenchFeatures {
        processor.features
    }

    /// Settings captured when the audio unit was created.
    public var settings: SpenchSettings {
        currentSettings
    }

    /// Current mono sample rate used by the processor.
    public var sampleRate: Double {
        Double(processor.sampleRate)
    }

    /// Cumulative processing telemetry.
    public var telemetry: SpenchTelemetry {
        processor.telemetry
    }

    /// Low-performance callback delivered on the main queue.
    public var onLowPerformance: ((SpenchLowPerformanceEvent) -> Void)?

    /// Processing-failure callback delivered on the main queue. A failure is
    /// reported once per render-resource cycle.
    public var onProcessingFailure: ((String) -> Void)?

    /// Rebuilds pipeline stages whose feature configuration changed.
    public func setFeatures(_ features: SpenchFeatures) throws {
        try processor.setFeatures(features)
    }

    /// Clears pipeline and ring-buffer state.
    ///
    /// - Important: Call this only when the host audio engine is stopped or
    ///   paused. The render thread is owned by the host and is not paused by
    ///   this method, so concurrent renders during `reset()` would race on the
    ///   ring buffers and congestion-control state.
    public override func reset() {
        let shouldRestartWorker = renderResourcesAllocated
        if shouldRestartWorker {
            // Reset mutates ring-buffer and congestion-control state that the worker also uses.
            stopWorkerThread()
        }

        super.reset()
        processor.reset()
        inputRing.reset()
        outputRing.reset()
        resetCongestionControlState(sampleRate: resolvedSampleRate())

        if shouldRestartWorker {
            startWorkerThread()
        }
    }

    public override var inputBusses: AUAudioUnitBusArray { inputBusArray }
    public override var outputBusses: AUAudioUnitBusArray { outputBusArray }

    public override init(componentDescription: AudioComponentDescription,
                         options: AudioComponentInstantiationOptions = []) throws {
        let format = Self.bootstrapFormat()
        let settings = try SpenchSettings.default.validated()
        let ringFrames = Self.ringCapacityFrames(sampleRate: format.sampleRate, settings: settings)

        currentSettings = settings
        processor = try SpenchProcessor(sampleRate: Float(format.sampleRate), settings: settings)
        inputRing = RingBuffer(capacity: ringFrames)
        outputRing = RingBuffer(capacity: ringFrames)
        lowPerformanceMonitor = SpenchLowPerformanceMonitor(settings: settings,
                                                            sampleRate: format.sampleRate,
                                                            maxLatencySeconds: Self.maxOutputBufferDuration)

        inputBus = try AUAudioUnitBus(format: format)
        outputBus = try AUAudioUnitBus(format: format)
        try super.init(componentDescription: componentDescription, options: options)

        inputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .input, busses: [inputBus])
        outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outputBus])

        maximumFramesToRender = 4_096
        internalBlock = buildRenderBlock()
        resetCongestionControlState(sampleRate: format.sampleRate)
    }

    deinit {
        stopWorkerThread()
    }

    public override func allocateRenderResources() throws {
        try synchronizeBusFormatsIfNeeded()

        let sampleRate = resolvedSampleRate()
        let previousProcessor = processor
        let previousInputRing = inputRing
        let previousOutputRing = outputRing
        let previousMonitor = lowPerformanceMonitor

        do {
            try rebuildProcessor(sampleRate: Float(sampleRate))
            configureRuntimeState(sampleRate: sampleRate)
            try super.allocateRenderResources()
        } catch {
            // Roll back partial state so a failed allocation leaves the unit in
            // its prior consistent shape rather than half-rebuilt.
            processor = previousProcessor
            inputRing = previousInputRing
            outputRing = previousOutputRing
            lowPerformanceMonitor = previousMonitor
            throw error
        }

        startWorkerThread()
    }

    public override func deallocateRenderResources() {
        stopWorkerThread()
        inputRing.reset()
        outputRing.reset()
        resetCongestionControlState(sampleRate: resolvedSampleRate())
        super.deallocateRenderResources()
    }

    public override var internalRenderBlock: AUInternalRenderBlock { internalBlock }

    public override func shouldChange(to format: AVAudioFormat, for bus: AUAudioUnitBus) -> Bool {
        guard !renderResourcesAllocated,
              bus === inputBus || bus === outputBus,
              format.channelCount == 1,
              format.commonFormat == .pcmFormatFloat32,
              !format.isInterleaved,
              format.sampleRate > 0,
              format.sampleRate.isFinite else {
            return false
        }

        return super.shouldChange(to: format, for: bus)
    }

    internal func configureInitialSettings(_ settings: SpenchSettings) throws {
        guard !renderResourcesAllocated, workerThread == nil else {
            throw SpenchError.invalidArgument(
                "Settings can only be configured before render resources are allocated"
            )
        }

        currentSettings = try settings.validated()
        let sampleRate = resolvedSampleRate()
        processor = try SpenchProcessor(sampleRate: Float(sampleRate),
                                        features: processor.features,
                                        settings: currentSettings)
        configureRuntimeState(sampleRate: sampleRate)
    }

    private func buildRenderBlock() -> AUInternalRenderBlock {
        return { [weak self] actionFlags, timestamp, frameCount, _, outputData, _, pullInputBlock in
            guard let self else { return kAudioUnitErr_Uninitialized }
            guard let pullInputBlock else { return kAudioUnitErr_NoConnection }

            let pullStatus = pullInputBlock(actionFlags, timestamp, frameCount, 0, outputData)
            guard pullStatus == noErr else { return pullStatus }

            let buffers = UnsafeMutableAudioBufferListPointer(outputData)
            guard let data = buffers.first?.mData else { return kAudioUnitErr_NoConnection }

            let samples = data.bindMemory(to: Float.self, capacity: Int(frameCount))
            let now = CACurrentMediaTime()

            let pushed = self.inputRing.push(samples, count: frameCount)
            if pushed > 0 {
                self.workerEvent?.notify()
            }

            guard self.shouldRenderOutput(now: now, availableFrames: self.outputRing.availableToRead) else {
                // Hold silence until enough processed audio has accumulated to avoid
                // immediately underrunning after startup or after a refill cycle.
                samples.update(repeating: 0, count: Int(frameCount))
                return noErr
            }

            let popped = self.outputRing.pop(samples, count: frameCount)
            if popped < frameCount {
                let remaining = Int(frameCount - popped)
                samples.advanced(by: Int(popped)).update(repeating: 0, count: remaining)

                if let event = self.handleOutputUnderrun(now: now,
                                                         poppedFrames: popped,
                                                         requestedFrames: frameCount) {
                    self.emitLowPerformanceEvent(event)
                }
            } else {
                self.lowPerformanceMonitor.markRefilled()
            }

            return noErr
        }
    }

    private func startWorkerThread() {
        guard workerThread == nil else { return }

        let workerEvent = WorkerThreadEvent()
        let workerFinishedEvent = DispatchSemaphore(value: 0)
        let thread = Thread { [weak self, workerEvent] in
            self?.runWorkerLoop(workerEvent: workerEvent)
            workerFinishedEvent.signal()
        }
        thread.name = "SpenchAUAudioUnitWorker"
        // The worker feeds the realtime render block; user-interactive QoS keeps
        // it scheduled aggressively enough to avoid output underruns.
        thread.qualityOfService = .userInteractive
        self.workerEvent = workerEvent
        self.workerFinishedEvent = workerFinishedEvent
        workerThread = thread
        thread.start()
    }

    private func stopWorkerThread() {
        guard workerThread != nil else { return }

        workerEvent?.stop()
        workerFinishedEvent?.wait()

        workerThread = nil
        workerEvent = nil
        workerFinishedEvent = nil
    }

    private func runWorkerLoop(workerEvent: WorkerThreadEvent) {
        // Sample rate is fixed for the lifetime of one render-resource cycle,
        // so we cache it once instead of paying for the locked accessor on
        // every iteration.
        let workerSampleRate = Double(processor.sampleRate)
        let chunkFrameCount = Self.processingChunkFrameCount(sampleRate: workerSampleRate,
                                                              settings: currentSettings)
        let chunkFramesU32 = UInt32(chunkFrameCount)
        var processingBuffer = [Float](repeating: 0, count: chunkFrameCount)

        var meanProcessTimeAggregator = MeanProcessTimeAggregator(window: Self.meanProcessTimeCount)

        while true {
            if inputRing.availableToRead < chunkFramesU32 {
                if !workerEvent.waitForNotification() {
                    break
                }
                continue
            }

            var readFrames: UInt32 = 0
            processingBuffer.withUnsafeMutableBufferPointer { pointer in
                guard let baseAddress = pointer.baseAddress else { return }
                readFrames = inputRing.pop(baseAddress, count: chunkFramesU32)
            }

            if readFrames != chunkFramesU32 {
                if !workerEvent.waitForNotification() {
                    break
                }
                continue
            }

            processingBuffer.withUnsafeMutableBufferPointer { pointer in
                guard let baseAddress = pointer.baseAddress else { return }

                let startTime = CACurrentMediaTime()
                let processingFailure = processor.processInPlace(baseAddress, frameCount: Int(readFrames))
                let elapsedMilliseconds = (CACurrentMediaTime() - startTime) * 1_000
                meanProcessingTimeStore.storeValue(meanProcessTimeAggregator.append(elapsedMilliseconds))
                _ = outputRing.push(baseAddress, count: readFrames)

                if let processingFailure, !processingFailureReported {
                    processingFailureReported = true
                    emitProcessingFailure(processingFailure)
                }
            }
        }
    }

    private func rebuildProcessor(sampleRate: Float) throws {
        processor = try SpenchProcessor(sampleRate: sampleRate,
                                        features: processor.features,
                                        settings: currentSettings)
    }

    private func configureRuntimeState(sampleRate: Double) {
        let ringFrames = Self.ringCapacityFrames(sampleRate: sampleRate, settings: currentSettings)
        inputRing = RingBuffer(capacity: ringFrames)
        outputRing = RingBuffer(capacity: ringFrames)
        inputRing.reset()
        outputRing.reset()
        resetCongestionControlState(sampleRate: sampleRate)
    }

    private func resolvedSampleRate() -> Double {
        let inputSampleRate = inputBus.format.sampleRate
        if inputSampleRate > 0, inputSampleRate.isFinite {
            return inputSampleRate
        }

        let outputSampleRate = outputBus.format.sampleRate
        if outputSampleRate > 0, outputSampleRate.isFinite {
            return outputSampleRate
        }

        return Self.bootstrapSampleRate
    }

    private func synchronizeBusFormatsIfNeeded() throws {
        let targetFormat = resolvedFormat()

        if inputBus.format != targetFormat {
            try inputBus.setFormat(targetFormat)
        }
        if outputBus.format != targetFormat {
            try outputBus.setFormat(targetFormat)
        }
    }

    private func resolvedFormat() -> AVAudioFormat {
        let inputFormat = inputBus.format
        if inputFormat.sampleRate > 0, inputFormat.sampleRate.isFinite {
            return inputFormat
        }

        return outputBus.format
    }

    private func shouldRenderOutput(now: Double, availableFrames: UInt32) -> Bool {
        if lowPerformanceMonitor.shouldRelaxRefillRequirement(now: now) {
            // A slow refill can continue with a smaller buffer instead of stalling.
            outputPrimed = true
        }

        if outputPrimed {
            return true
        }

        if availableFrames >= lowPerformanceMonitor.primeThresholdFrames() {
            // Once enough audio is buffered, stay in streaming mode until the next underrun.
            outputPrimed = true
            lowPerformanceMonitor.markRefilled()
            return true
        }

        return false
    }

    private func handleOutputUnderrun(now: Double,
                                      poppedFrames: UInt32,
                                      requestedFrames: UInt32) -> SpenchLowPerformanceEvent? {
        guard poppedFrames < requestedFrames else {
            lowPerformanceMonitor.markRefilled()
            return nil
        }

        outputPrimed = false
        return lowPerformanceMonitor.handleUnderrun(now: now,
                                                    meanProcessingTimeMilliseconds: meanProcessingTimeMilliseconds())
    }

    private func resetCongestionControlState(sampleRate: Double) {
        outputPrimed = false
        processingFailureReported = false
        meanProcessingTimeStore.storeNil()
        lowPerformanceMonitor.reset(sampleRate: sampleRate)
    }

    private func meanProcessingTimeMilliseconds() -> Double? {
        meanProcessingTimeStore.loadValue()?.doubleValue
    }

    internal func emitLowPerformanceEvent(_ event: SpenchLowPerformanceEvent) {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }

            self.onLowPerformance?(event)
            NotificationCenter.default.post(name: .spenchLowPerformance,
                                            object: self,
                                            userInfo: [SpenchNotificationUserInfoKey.lowPerformanceEvent: event])
        }
    }

    internal func emitProcessingFailure(_ message: String) {
        DispatchQueue.main.async { [weak self] in
            self?.onProcessingFailure?(message)
        }
    }

    private static func bootstrapFormat() -> AVAudioFormat {
        let sessionSampleRate = AVAudioSession.sharedInstance().sampleRate
        let sampleRate = sessionSampleRate > 0 && sessionSampleRate.isFinite ? sessionSampleRate : bootstrapSampleRate
        return AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 1)!
    }

    internal static func processingChunkSeconds(settings: SpenchSettings) -> Double {
        let requestedSeconds = min(settings.maxProcessingChunkSeconds,
                                   settings.minLatency,
                                   maxOutputBufferDuration)
        guard requestedSeconds > processingChunkStepSeconds else {
            return processingChunkStepSeconds
        }

        return max((requestedSeconds / processingChunkStepSeconds).rounded(.down) * processingChunkStepSeconds,
                   processingChunkStepSeconds)
    }

    internal static func processingChunkFrameCount(sampleRate: Double, settings: SpenchSettings) -> Int {
        Int((sampleRate * processingChunkSeconds(settings: settings)).rounded(.up))
    }

    private static func ringCapacityFrames(sampleRate: Double, settings: SpenchSettings) -> UInt32 {
        let processingChunkFrames = processingChunkFrameCount(sampleRate: sampleRate, settings: settings)
        let minimumRingFrames = Int((sampleRate * maxOutputBufferDuration).rounded(.up))
        return UInt32(max(minimumRingFrames, processingChunkFrames * 2))
    }
}
