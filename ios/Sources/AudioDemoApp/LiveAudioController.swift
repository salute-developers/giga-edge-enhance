import AVFoundation
import EnhanceAudioKit

final class LiveAudioController {
    private var engine: AVAudioEngine?
    private var gigaEdgeEnhanceUnit: EnhanceAVAudioUnit?
    private var delayNode: AVAudioUnitDelay?

    var telemetry: EnhanceTelemetry {
        gigaEdgeEnhanceUnit?.telemetry ?? .empty
    }
    var onProcessingFailure: ((String) -> Void)?

    func start(features: EnhanceFeatures,
               settings: EnhanceSettings,
               onLowPerformance: @escaping (EnhanceLowPerformanceEvent) -> Void,
               completion: @escaping (Error?) -> Void) {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
            try session.setActive(true)
            let gigaEdgeEnhanceUnit = try EnhanceAVAudioUnit(features: features, settings: settings)
            gigaEdgeEnhanceUnit.onLowPerformance = onLowPerformance
            gigaEdgeEnhanceUnit.onProcessingFailure = onProcessingFailure
            try configureEngine(gigaEdgeEnhanceUnit: gigaEdgeEnhanceUnit)
            completion(nil)
        } catch {
            completion(error)
        }
    }

    func stop() {
        if let engine {
            engine.stop()
            engine.pause()

            if let gigaEdgeEnhanceUnit {
                gigaEdgeEnhanceUnit.onLowPerformance = nil
                gigaEdgeEnhanceUnit.onProcessingFailure = nil
                gigaEdgeEnhanceUnit.reset()
                if let audioUnit = gigaEdgeEnhanceUnit.auAudioUnit as? EnhanceAUAudioUnit,
                   audioUnit.renderResourcesAllocated {
                    audioUnit.deallocateRenderResources()
                }
                engine.disconnectAndDetachIfNeeded(gigaEdgeEnhanceUnit)
            }

            if let delayNode {
                engine.disconnectAndDetachIfNeeded(delayNode)
            }

            engine.reset()
        }

        engine = nil
        gigaEdgeEnhanceUnit = nil
        delayNode = nil
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    private func configureEngine(gigaEdgeEnhanceUnit: EnhanceAVAudioUnit) throws {
        let engine = AVAudioEngine()
        let delayNode = AVAudioUnitDelay()
        delayNode.delayTime = 1.0
        delayNode.feedback = 0
        delayNode.wetDryMix = 100

        engine.attach(gigaEdgeEnhanceUnit)
        engine.attach(delayNode)

        let inputFormat = engine.inputNode.outputFormat(forBus: 0)
        engine.connect(engine.inputNode, to: gigaEdgeEnhanceUnit, format: inputFormat)
        engine.connect(gigaEdgeEnhanceUnit, to: delayNode, format: inputFormat)
        engine.connect(delayNode, to: engine.mainMixerNode, format: inputFormat)

        try engine.start()

        self.engine = engine
        self.gigaEdgeEnhanceUnit = gigaEdgeEnhanceUnit
        self.delayNode = delayNode
    }

    func setFeatures(_ features: EnhanceFeatures) throws {
        try gigaEdgeEnhanceUnit?.setFeatures(features)
    }
}
