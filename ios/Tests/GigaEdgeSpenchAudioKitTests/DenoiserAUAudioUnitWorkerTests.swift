import AudioToolbox
import AVFoundation
import XCTest
@testable import GigaEdgeSpenchAudioKit

final class DenoiserAUAudioUnitWorkerTests: XCTestCase {
    func testProcessingChunkResolutionMatchesWebSemantics() {
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkSeconds(settings: .default), 0.016, accuracy: 0.000_001)
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkFrameCount(sampleRate: 48_000, settings: .default), 768)

        let latencyBounded = Settings(minLatency: 0.05, maxProcessingChunkSeconds: 0.064)
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkSeconds(settings: latencyBounded), 0.048, accuracy: 0.000_001)
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkFrameCount(sampleRate: 44_100, settings: latencyBounded), 2_117)

        let belowMinimum = Settings(minLatency: 0.008, maxProcessingChunkSeconds: 0.008)
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkSeconds(settings: belowMinimum), 0.016, accuracy: 0.000_001)

        let capped = Settings(minLatency: 1, maxProcessingChunkSeconds: 1)
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkSeconds(settings: capped), 0.256, accuracy: 0.000_001)
        XCTAssertEqual(DenoiserAUAudioUnit.processingChunkFrameCount(sampleRate: 48_000, settings: capped), 12_288)
    }

    func testWorkerWaitsForConfiguredFullChunk() throws {
        let component = DenoiserAVAudioUnit.componentDescription
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: component, options: [])
        try audioUnit.configureInitialSettings(Settings(minLatency: 0.05,
                                                              maxProcessingChunkSeconds: 0.064))
        try audioUnit.setFeatures(.standard)
        try audioUnit.allocateRenderResources()
        defer {
            audioUnit.deallocateRenderResources()
        }

        let frameCount = 256
        let render = audioUnit.internalRenderBlock
        let ablPointer = UnsafeMutablePointer<AudioBufferList>.allocate(capacity: 1)
        defer { ablPointer.deallocate() }
        ablPointer.pointee.mNumberBuffers = 1

        let abl = UnsafeMutableAudioBufferListPointer(ablPointer)
        abl[0].mNumberChannels = 1
        abl[0].mDataByteSize = UInt32(frameCount * MemoryLayout<Float>.size)

        let pullInput: AURenderPullInputBlock = { _, _, pulledFrames, _, ioData in
            let inputBuffers = UnsafeMutableAudioBufferListPointer(ioData)
            guard let inputData = inputBuffers[0].mData else { return kAudio_ParamError }
            inputData.assumingMemoryBound(to: Float.self).update(repeating: 1.0, count: Int(pulledFrames))
            return noErr
        }

        var timestamp = AudioTimeStamp()
        timestamp.mFlags = .sampleTimeValid
        var actionFlags: AudioUnitRenderActionFlags = []
        var output = [Float](repeating: 0, count: frameCount)

        func renderOnce() {
            output.withUnsafeMutableBufferPointer { pointer in
                abl[0].mData = pointer.baseAddress.map { UnsafeMutableRawPointer($0) }
                XCTAssertEqual(render(&actionFlags,
                                      &timestamp,
                                      UInt32(frameCount),
                                      0,
                                      ablPointer,
                                      nil,
                                      pullInput), noErr)
            }
        }

        // The effective chunk is 48 ms / 2,304 frames at 48 kHz.
        for _ in 0..<8 {
            renderOnce()
        }
        usleep(20_000)
        XCTAssertEqual(audioUnit.telemetry.audioProcessingCounters.values.reduce(0, +), 0)

        renderOnce()

        var processingInvocationCount = 0
        for _ in 0..<50 where processingInvocationCount == 0 {
            usleep(2_000)
            processingInvocationCount = audioUnit.telemetry.audioProcessingCounters.values.reduce(0, +)
        }
        XCTAssertEqual(processingInvocationCount, 1)
    }

    func testWorkerPipelinePrimesThenProducesAudio() throws {
        let component = DenoiserAVAudioUnit.componentDescription
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: component, options: [])
        try audioUnit.setFeatures(.standard)
        try audioUnit.allocateRenderResources()
        defer {
            audioUnit.deallocateRenderResources()
        }

        let frameCount = 256
        let render = audioUnit.internalRenderBlock
        let ablPointer = UnsafeMutablePointer<AudioBufferList>.allocate(capacity: 1)
        defer { ablPointer.deallocate() }
        ablPointer.pointee.mNumberBuffers = 1

        let abl = UnsafeMutableAudioBufferListPointer(ablPointer)
        abl[0].mNumberChannels = 1
        abl[0].mDataByteSize = UInt32(frameCount * MemoryLayout<Float>.size)

        let pullInput: AURenderPullInputBlock = { _, _, pulledFrames, _, ioData in
            let inputBuffers = UnsafeMutableAudioBufferListPointer(ioData)
            guard let inputData = inputBuffers[0].mData else { return kAudio_ParamError }
            let inputSamples = inputData.assumingMemoryBound(to: Float.self)
            inputSamples.update(repeating: 1.0, count: Int(pulledFrames))
            return noErr
        }

        var timestamp = AudioTimeStamp()
        timestamp.mFlags = .sampleTimeValid
        var actionFlags: AudioUnitRenderActionFlags = []
        var output = [Float](repeating: 0, count: frameCount)

        for _ in 0..<6 {
            output.withUnsafeMutableBufferPointer { pointer in
                abl[0].mData = pointer.baseAddress.map { UnsafeMutableRawPointer($0) }
                let status = render(&actionFlags,
                                    &timestamp,
                                    UInt32(frameCount),
                                    0,
                                    ablPointer,
                                    nil,
                                    pullInput)
                XCTAssertEqual(status, noErr)
            }
            XCTAssertTrue(output.allSatisfy { $0 == 0 })
            usleep(2_000)
        }

        var gotProcessedAudio = false
        for _ in 0..<80 where !gotProcessedAudio {
            output.withUnsafeMutableBufferPointer { pointer in
                abl[0].mData = pointer.baseAddress.map { UnsafeMutableRawPointer($0) }
                let status = render(&actionFlags,
                                    &timestamp,
                                    UInt32(frameCount),
                                    0,
                                    ablPointer,
                                    nil,
                                    pullInput)
                XCTAssertEqual(status, noErr)
            }

            if output.contains(where: { abs($0) > 0.0001 }) {
                gotProcessedAudio = true
            }
            usleep(2_000)
        }

        XCTAssertTrue(gotProcessedAudio)
    }

    func testAudioUnitUsesConfiguredBusSampleRate() throws {
        let component = DenoiserAVAudioUnit.componentDescription
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: component, options: [])
        let format = AVAudioFormat(standardFormatWithSampleRate: 44_100, channels: 1)!

        try audioUnit.inputBusses[0].setFormat(format)
        try audioUnit.outputBusses[0].setFormat(format)
        try audioUnit.allocateRenderResources()
        defer {
            audioUnit.deallocateRenderResources()
        }

        XCTAssertEqual(audioUnit.sampleRate, 44_100, accuracy: 0.5)
    }

    func testAudioUnitFallsBackToBootstrapSampleRateBeforeNegotiation() throws {
        let component = DenoiserAVAudioUnit.componentDescription
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: component, options: [])

        XCTAssertEqual(audioUnit.sampleRate, 48_000, accuracy: 0.5)
    }

    func testLowPerformanceCallbackAndNotificationAreEmitted() throws {
        let component = DenoiserAVAudioUnit.componentDescription
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: component, options: [])

        let callbackExpectation = expectation(description: "Low performance callback")
        let notificationExpectation = expectation(description: "Low performance notification")
        let event = LowPerformanceEvent(sampleRate: 48_000,
                                              currentLatency: 0.096,
                                              meanProcessingTimeMilliseconds: 11,
                                              refillCount: 1)
        var receivedEvent: LowPerformanceEvent?
        var callbackReceived = false
        var notificationReceived = false

        audioUnit.onLowPerformance = { event in
            receivedEvent = event
            callbackReceived = true
            callbackExpectation.fulfill()
        }

        let observer = NotificationCenter.default.addObserver(forName: .gigaEdgeSpenchLowPerformance,
                                                              object: audioUnit,
                                                              queue: .main) { notification in
            let event = notification.userInfo?[NotificationUserInfoKey.lowPerformanceEvent] as? LowPerformanceEvent
            XCTAssertNotNil(event)
            notificationReceived = true
            notificationExpectation.fulfill()
        }
        defer {
            NotificationCenter.default.removeObserver(observer)
        }

        audioUnit.emitLowPerformanceEvent(event)
        RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.05))

        wait(for: [callbackExpectation, notificationExpectation], timeout: 2)
        XCTAssertEqual(receivedEvent?.sampleRate, event.sampleRate)
        XCTAssertEqual(receivedEvent?.currentLatency, event.currentLatency)
        XCTAssertEqual(receivedEvent?.meanProcessingTimeMilliseconds, event.meanProcessingTimeMilliseconds)
        XCTAssertEqual(receivedEvent?.refillCount, event.refillCount)
        XCTAssertTrue(callbackReceived)
        XCTAssertTrue(notificationReceived)
    }

    func testProcessingFailureCallbackIsEmitted() throws {
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: DenoiserAVAudioUnit.componentDescription, options: [])
        let callbackExpectation = expectation(description: "Processing failure callback")
        let message = "Denoising failed with native error code -2."
        var receivedMessage: String?

        audioUnit.onProcessingFailure = { failureMessage in
            receivedMessage = failureMessage
            callbackExpectation.fulfill()
        }
        audioUnit.emitProcessingFailure(message)

        wait(for: [callbackExpectation], timeout: 2)
        XCTAssertEqual(receivedMessage, message)
    }

    func testInitialSettingsCannotBeChangedAfterRenderResourcesAllocated() throws {
        let audioUnit = try DenoiserAUAudioUnit(componentDescription: DenoiserAVAudioUnit.componentDescription, options: [])
        try audioUnit.allocateRenderResources()
        defer {
            audioUnit.deallocateRenderResources()
        }

        XCTAssertThrowsError(try audioUnit.configureInitialSettings(.default))
    }

    func testDenoiserAUAudioUnitDeallocatesAndStopsWorkerResources() throws {
        weak var weakAudioUnit: DenoiserAUAudioUnit?

        autoreleasepool {
            var unit = try? DenoiserAUAudioUnit(componentDescription: DenoiserAVAudioUnit.componentDescription, options: [])
            weakAudioUnit = unit
            try? unit?.allocateRenderResources()
            unit?.deallocateRenderResources()
            unit = nil
        }

        XCTAssertTrue(waitUntilReleased({ weakAudioUnit == nil }, timeout: 5.0))
    }

    func testRepeatedRenderResourceCyclesReleaseAudioUnits() throws {
        for _ in 0..<3 {
            weak var weakAudioUnit: DenoiserAUAudioUnit?

            autoreleasepool {
                var unit = try? DenoiserAUAudioUnit(componentDescription: DenoiserAVAudioUnit.componentDescription,
                                                  options: [])
                weakAudioUnit = unit
                try? unit?.allocateRenderResources()
                unit?.deallocateRenderResources()
                unit = nil
            }

            XCTAssertTrue(waitUntilReleased({ weakAudioUnit == nil }, timeout: 5.0))
        }
    }

    func testDenoiserAVAudioUnitDeallocatesAndReleasesAUAudioUnitResources() {
        weak var weakAVAudioUnit: DenoiserAVAudioUnit?
        weak var weakAUAudioUnit: DenoiserAUAudioUnit?

        autoreleasepool {
            var unit: DenoiserAVAudioUnit? = DenoiserAVAudioUnit()
            weakAVAudioUnit = unit
            weakAUAudioUnit = unit?.auAudioUnit as? DenoiserAUAudioUnit
            try? weakAUAudioUnit?.allocateRenderResources()
            weakAUAudioUnit?.deallocateRenderResources()
            unit = nil
        }

        XCTAssertTrue(waitUntilReleased { weakAVAudioUnit == nil })

        if let au = weakAUAudioUnit {
            XCTAssertFalse(au.renderResourcesAllocated)
            XCTAssertTrue(waitUntilReleased({ weakAUAudioUnit == nil }, timeout: 5.0))
        }
    }

    func testLiveGraphStopReleasesAttachedAudioUnits() throws {
        #if targetEnvironment(simulator)
        throw XCTSkip("The iOS Simulator cannot configure the live play-and-record graph reliably.")
        #endif

        weak var weakEngine: AVAudioEngine?
        weak var weakAVAudioUnit: DenoiserAVAudioUnit?
        weak var weakAUAudioUnit: DenoiserAUAudioUnit?
        weak var weakDelayNode: AVAudioUnitDelay?

        autoreleasepool {
            let session = AVAudioSession.sharedInstance()
            try? session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
            try? session.setPreferredSampleRate(44_100)
            try? session.setActive(true)

            var engine: AVAudioEngine? = AVAudioEngine()
            var gigaEdgeSpenchUnit: DenoiserAVAudioUnit? = DenoiserAVAudioUnit()
            var delayNode: AVAudioUnitDelay? = AVAudioUnitDelay()

            weakEngine = engine
            weakAVAudioUnit = gigaEdgeSpenchUnit
            weakAUAudioUnit = gigaEdgeSpenchUnit?.auAudioUnit as? DenoiserAUAudioUnit
            weakDelayNode = delayNode

            if let engine, let gigaEdgeSpenchUnit, let delayNode {
                engine.attach(gigaEdgeSpenchUnit)
                engine.attach(delayNode)

                let inputFormat = engine.inputNode.outputFormat(forBus: 0)
                engine.connect(engine.inputNode, to: gigaEdgeSpenchUnit, format: inputFormat)
                engine.connect(gigaEdgeSpenchUnit, to: delayNode, format: inputFormat)
                engine.connect(delayNode, to: engine.mainMixerNode, format: inputFormat)

                try? engine.start()
                engine.stop()
                engine.pause()
                gigaEdgeSpenchUnit.reset()
                if let audioUnit = gigaEdgeSpenchUnit.auAudioUnit as? DenoiserAUAudioUnit,
                   audioUnit.renderResourcesAllocated {
                    audioUnit.deallocateRenderResources()
                }
                engine.disconnectNodeInput(gigaEdgeSpenchUnit)
                engine.disconnectNodeOutput(gigaEdgeSpenchUnit)
                engine.disconnectNodeInput(delayNode)
                engine.disconnectNodeOutput(delayNode)
                engine.detach(gigaEdgeSpenchUnit)
                engine.detach(delayNode)
                engine.reset()
            }

            engine = nil
            gigaEdgeSpenchUnit = nil
            delayNode = nil

            try? session.setActive(false, options: .notifyOthersOnDeactivation)
        }

        XCTAssertTrue(waitUntilReleased { weakEngine == nil })
        XCTAssertTrue(waitUntilReleased { weakAVAudioUnit == nil })
        XCTAssertTrue(waitUntilReleased { weakDelayNode == nil })

        if let au = weakAUAudioUnit {
            XCTAssertFalse(au.renderResourcesAllocated)
            XCTAssertTrue(waitUntilReleased({ weakAUAudioUnit == nil }, timeout: 5.0))
        }
    }

    private func waitUntilReleased(_ condition: @escaping () -> Bool,
                                   timeout: TimeInterval = 2.0) -> Bool {
        let deadline = Date().addingTimeInterval(timeout)
        while Date() < deadline {
            if condition() {
                return true
            }
            RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.01))
        }
        return condition()
    }
}
