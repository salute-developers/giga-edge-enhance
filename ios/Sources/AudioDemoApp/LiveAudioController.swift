import AVFoundation
import GigaEdgeSpenchAudioKit

final class LiveAudioController {
    private var engine: AVAudioEngine?
    private var gigaEdgeSpenchUnit: DenoiserAVAudioUnit?
    private var delayNode: AVAudioUnitDelay?

    var telemetry: Telemetry {
        gigaEdgeSpenchUnit?.telemetry ?? .empty
    }
    var onProcessingFailure: ((String) -> Void)?

    func start(features: Features,
               settings: Settings,
               onLowPerformance: @escaping (LowPerformanceEvent) -> Void,
               completion: @escaping (Error?) -> Void) {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
            try session.setActive(true)
            let gigaEdgeSpenchUnit = try DenoiserAVAudioUnit(features: features, settings: settings)
            gigaEdgeSpenchUnit.onLowPerformance = onLowPerformance
            gigaEdgeSpenchUnit.onProcessingFailure = onProcessingFailure
            try configureEngine(gigaEdgeSpenchUnit: gigaEdgeSpenchUnit)
            completion(nil)
        } catch {
            completion(error)
        }
    }

    func stop() {
        if let engine {
            engine.stop()
            engine.pause()

            if let gigaEdgeSpenchUnit {
                gigaEdgeSpenchUnit.onLowPerformance = nil
                gigaEdgeSpenchUnit.onProcessingFailure = nil
                gigaEdgeSpenchUnit.reset()
                if let audioUnit = gigaEdgeSpenchUnit.auAudioUnit as? DenoiserAUAudioUnit,
                   audioUnit.renderResourcesAllocated {
                    audioUnit.deallocateRenderResources()
                }
                engine.disconnectAndDetachIfNeeded(gigaEdgeSpenchUnit)
            }

            if let delayNode {
                engine.disconnectAndDetachIfNeeded(delayNode)
            }

            engine.reset()
        }

        engine = nil
        gigaEdgeSpenchUnit = nil
        delayNode = nil
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    private func configureEngine(gigaEdgeSpenchUnit: DenoiserAVAudioUnit) throws {
        let engine = AVAudioEngine()
        let delayNode = AVAudioUnitDelay()
        delayNode.delayTime = 1.0
        delayNode.feedback = 0
        delayNode.wetDryMix = 100

        engine.attach(gigaEdgeSpenchUnit)
        engine.attach(delayNode)

        let inputFormat = engine.inputNode.outputFormat(forBus: 0)
        engine.connect(engine.inputNode, to: gigaEdgeSpenchUnit, format: inputFormat)
        engine.connect(gigaEdgeSpenchUnit, to: delayNode, format: inputFormat)
        engine.connect(delayNode, to: engine.mainMixerNode, format: inputFormat)

        try engine.start()

        self.engine = engine
        self.gigaEdgeSpenchUnit = gigaEdgeSpenchUnit
        self.delayNode = delayNode
    }

    func setFeatures(_ features: Features) throws {
        try gigaEdgeSpenchUnit?.setFeatures(features)
    }
}
