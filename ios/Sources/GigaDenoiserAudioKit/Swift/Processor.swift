import Foundation
import QuartzCore
@_implementationOnly import GigaDenoiserAudioKit_Private

/// High-level Swift wrapper around the underlying GigaDenoiser processing pipeline.
///
/// `Processor` is convenient for direct buffer processing in tests, tools,
/// or offline flows that do not need an `AUAudioUnit` host.
///
/// - Important: A single `Processor` instance is **not** safe for
///   concurrent calls into ``process(input:output:)``, ``setFeatures(_:)``,
///   ``setSampleRate(_:)`` or ``reset()``. Serialize calls from one thread, or
///   wrap the instance with your own synchronization. Internal state updates
///   are protected so that mutating-API calls on different threads will not
///   corrupt the type, but concurrent processing calls would race on the
///   underlying ObjC++ pipeline scratch buffers.
public final class Processor {
    private let stateLock = UnfairLock()
    private var audioProcessElement: AudioProcessPipelineElement?
    private var denoiseElement: DenoisePipelineElement?
    private var currentSampleRate: Float
    private var currentFeatures: Features
    private let currentSettings: Settings
    private let telemetryStore: TelemetryStore

    /// Current sample rate used by the pipeline.
    public var sampleRate: Float {
        stateLock.withLock { currentSampleRate }
    }

    /// Currently enabled GigaDenoiser features.
    public var features: Features {
        stateLock.withLock { currentFeatures }
    }

    /// Validated settings captured at initialization time.
    public var settings: Settings {
        stateLock.withLock { currentSettings }
    }

    /// Cumulative processing telemetry.
    public var telemetry: Telemetry {
        telemetryStore.snapshot()
    }

    /// Creates a GigaDenoiser processor for direct sample-buffer processing.
    ///
    /// - Parameters:
    ///   - sampleRate: Mono input/output sample rate. Must be finite and
    ///     positive; otherwise ``DenoiserError/invalidArgument(_:)`` is thrown.
    ///   - features: Pipeline-stage switches. Pass
    ///     ``Features/standard`` for normalization-only output.
    ///   - settings: Pipeline configuration. Validated via
    ///     ``Settings/validated()``; throws on invalid values.
    public init(sampleRate: Float,
                features: Features = .standard,
                settings: Settings = .default) throws {
        currentSampleRate = try Self.validatedSampleRate(sampleRate)
        currentFeatures = features
        currentSettings = try settings.validated()
        telemetryStore = TelemetryStore(roundingMilliseconds: currentSettings.telemetryRoundingMilliseconds)
        audioProcessElement = try Self.makeAudioProcessElement(sampleRate: currentSampleRate,
                                                               settings: currentSettings,
                                                               isEnabled: features.normalization)
        denoiseElement = try Self.makeDenoiseElement(sampleRate: currentSampleRate,
                                                     modelID: features.noiseSuppression)
    }

    /// Rebuilds only the pipeline stages whose feature configuration changed.
    public func setFeatures(_ features: Features) throws {
        try stateLock.withLock {
            let sampleRate = currentSampleRate
            let settings = currentSettings
            let newAudioProcessElement = features.normalization == currentFeatures.normalization
                ? audioProcessElement
                : try Self.makeAudioProcessElement(sampleRate: sampleRate,
                                                   settings: settings,
                                                   isEnabled: features.normalization)
            let newDenoiseElement = features.noiseSuppression == currentFeatures.noiseSuppression
                ? denoiseElement
                : try Self.makeDenoiseElement(sampleRate: sampleRate, modelID: features.noiseSuppression)

            audioProcessElement = newAudioProcessElement
            denoiseElement = newDenoiseElement
            currentFeatures = features
        }
    }

    /// Rebuilds all configured pipeline stages with fresh internal state.
    public func reset() throws {
        try stateLock.withLock {
            let newAudioProcessElement = try Self.makeAudioProcessElement(sampleRate: currentSampleRate,
                                                                          settings: currentSettings,
                                                                          isEnabled: currentFeatures.normalization)
            let newDenoiseElement = try Self.makeDenoiseElement(sampleRate: currentSampleRate,
                                                                modelID: currentFeatures.noiseSuppression)
            audioProcessElement = newAudioProcessElement
            denoiseElement = newDenoiseElement
        }
    }

    /// Rebuilds the processing pipeline for a new sample rate.
    public func setSampleRate(_ sampleRate: Float) throws {
        let validatedSampleRate = try Self.validatedSampleRate(sampleRate)
        try stateLock.withLock {
            let newAudioProcessElement = try Self.makeAudioProcessElement(sampleRate: validatedSampleRate,
                                                                          settings: currentSettings,
                                                                          isEnabled: currentFeatures.normalization)
            let newDenoiseElement = try Self.makeDenoiseElement(sampleRate: validatedSampleRate,
                                                                modelID: currentFeatures.noiseSuppression)
            audioProcessElement = newAudioProcessElement
            denoiseElement = newDenoiseElement
            currentSampleRate = validatedSampleRate
        }
    }

    /// Processes one input buffer into the supplied output buffer.
    ///
    /// - Parameters:
    ///   - input: Mono `Float` samples to process.
    ///   - output: Pre-allocated buffer of the same length as `input`.
    ///     Existing contents are overwritten.
    /// - Returns: The number of frames written into `output`. May be less
    ///   than `input.count` when the denoiser stage produces a shorter
    ///   output for this chunk.
    /// - Throws: ``DenoiserError/invalidArgument(_:)`` if `input` and
    ///   `output` lengths differ, or ``DenoiserError/unknown(_:)`` if native
    ///   denoising fails.
    public func process(input: [Float], output: inout [Float]) throws -> UInt32 {
        guard input.count == output.count else {
            throw DenoiserError.invalidArgument("Input and output sizes must match")
        }

        return try input.withUnsafeBufferPointer { inputPointer in
            try output.withUnsafeMutableBufferPointer { outputPointer in
                guard let inputAddress = inputPointer.baseAddress,
                      let outputAddress = outputPointer.baseAddress else {
                    return 0
                }

                let state = snapshotState()

                if let audioProcessElement = state.audioProcessElement {
                    if state.settings.unstableReverseProcessingOrder {
                        outputAddress.update(from: inputAddress, count: input.count)
                    } else {
                        _ = measureAudioProcessing {
                            audioProcessElement.processInput(inputAddress,
                                                             output: outputAddress,
                                                             frameCount: UInt32(input.count))
                        }
                    }
                } else {
                    outputAddress.update(from: inputAddress, count: input.count)
                }

                if let denoiseElement = state.denoiseElement {
                    let processed = measureDenoiser {
                        denoiseElement.processInput(outputAddress,
                                                    output: outputAddress,
                                                    frameCount: UInt32(input.count))
                    }
                    if let processingError = denoiseElement.lastProcessingError {
                        throw DenoiserError.unknown(processingError)
                    }

                    if state.settings.unstableReverseProcessingOrder,
                       let audioProcessElement = state.audioProcessElement {
                        _ = measureAudioProcessing {
                            audioProcessElement.processInput(outputAddress,
                                                             output: outputAddress,
                                                             frameCount: processed)
                        }
                    }

                    return processed
                }

                if state.settings.unstableReverseProcessingOrder,
                   let audioProcessElement = state.audioProcessElement {
                    return measureAudioProcessing {
                        audioProcessElement.processInput(outputAddress,
                                                         output: outputAddress,
                                                         frameCount: UInt32(input.count))
                    }
                }

                return UInt32(input.count)
            }
        }
    }

    // Used by the audio unit worker thread to process one mono chunk in place.
    func processInPlace(_ samples: UnsafeMutablePointer<Float>, frameCount: Int) -> String? {
        let state = snapshotState()

        if state.settings.unstableReverseProcessingOrder {
            var processingError: String?
            if let denoiseElement = state.denoiseElement {
                _ = measureDenoiser {
                    denoiseElement.processInput(samples, output: samples, frameCount: UInt32(frameCount))
                }
                processingError = denoiseElement.lastProcessingError
            }
            if let audioProcessElement = state.audioProcessElement {
                _ = measureAudioProcessing {
                    audioProcessElement.processInput(samples, output: samples, frameCount: UInt32(frameCount))
                }
            }
            return processingError
        }

        if let audioProcessElement = state.audioProcessElement {
            _ = measureAudioProcessing {
                audioProcessElement.processInput(samples, output: samples, frameCount: UInt32(frameCount))
            }
        }
        if let denoiseElement = state.denoiseElement {
            _ = measureDenoiser {
                denoiseElement.processInput(samples, output: samples, frameCount: UInt32(frameCount))
            }
            return denoiseElement.lastProcessingError
        }

        return nil
    }

    private func measureAudioProcessing<T>(_ body: () -> T) -> T {
        measure(body) { elapsedMilliseconds in
            telemetryStore.recordAudioProcessing(milliseconds: elapsedMilliseconds)
        }
    }

    private func measureDenoiser<T>(_ body: () -> T) -> T {
        measure(body) { elapsedMilliseconds in
            telemetryStore.recordDenoiser(milliseconds: elapsedMilliseconds)
        }
    }

    private func measure<T>(_ body: () -> T, record: (Double) -> Void) -> T {
        let startTime = CACurrentMediaTime()
        let result = body()
        record((CACurrentMediaTime() - startTime) * 1_000)
        return result
    }

    private func snapshotState() -> (audioProcessElement: AudioProcessPipelineElement?,
                                     denoiseElement: DenoisePipelineElement?,
                                     settings: Settings) {
        stateLock.withLock {
            (audioProcessElement, denoiseElement, currentSettings)
        }
    }

    private static func validatedSampleRate(_ sampleRate: Float) throws -> Float {
        guard sampleRate.isFinite, sampleRate > 0 else {
            throw DenoiserError.invalidArgument("sampleRate must be greater than zero")
        }

        return sampleRate
    }

    private static func makeAudioProcessElement(sampleRate: Float,
                                                settings: Settings,
                                                isEnabled: Bool) throws -> AudioProcessPipelineElement? {
        guard isEnabled else { return nil }
        return try AudioProcessPipelineElement(yamlConfig: normalizationConfig(sampleRate: sampleRate,
                                                                               settings: settings),
                                               sampleRate: sampleRate)
    }

    private static func makeDenoiseElement(sampleRate: Float,
                                           modelID: ModelDescriptor?) throws -> DenoisePipelineElement? {
        guard let modelID else { return nil }
        let modelURL = try ModelCatalog.modelURL(forID: modelID)
        return try DenoisePipelineElement(modelPath: modelURL.path, sampleRate: sampleRate)
    }

    static func normalizationConfig(sampleRate: Float, settings: Settings) -> String {
        if let customConfig = settings.unstableCustomNormalizationConfig {
            return customConfig
        }

        let roundedSampleRate = Int(sampleRate.rounded())
        let vadConfig = settings.useVadNormalization
            ? "\n    use_vad: true\n    target_level_db: -18.0"
            : ""

        return """
        sound_processing:
          loudness_ctrl:
            sample_rate: \(roundedSampleRate)
        \(vadConfig)
          dynamic_range_compressor_ctrl:
            sample_rate: \(roundedSampleRate)
          noise_gate_ctrl:
            sample_rate: \(roundedSampleRate)
          limiter_ctrl:
            sample_rate: \(roundedSampleRate)
        """
    }
}
