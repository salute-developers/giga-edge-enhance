import AVFoundation
import GigaDenoiserAudioKit

final class LiveAudioController {
    private var engine: AVAudioEngine?
    private var gigaDenoiserUnit: DenoiserAVAudioUnit?
    private var delayNode: AVAudioUnitDelay?

    var telemetry: Telemetry {
        gigaDenoiserUnit?.telemetry ?? .empty
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
            let gigaDenoiserUnit = try DenoiserAVAudioUnit(features: features, settings: settings)
            gigaDenoiserUnit.onLowPerformance = onLowPerformance
            gigaDenoiserUnit.onProcessingFailure = onProcessingFailure
            try configureEngine(gigaDenoiserUnit: gigaDenoiserUnit)
            completion(nil)
        } catch {
            completion(error)
        }
    }

    func stop() {
        if let engine {
            engine.stop()
            engine.pause()

            if let gigaDenoiserUnit {
                gigaDenoiserUnit.onLowPerformance = nil
                gigaDenoiserUnit.onProcessingFailure = nil
                gigaDenoiserUnit.reset()
                if let audioUnit = gigaDenoiserUnit.auAudioUnit as? DenoiserAUAudioUnit,
                   audioUnit.renderResourcesAllocated {
                    audioUnit.deallocateRenderResources()
                }
                engine.disconnectAndDetachIfNeeded(gigaDenoiserUnit)
            }

            if let delayNode {
                engine.disconnectAndDetachIfNeeded(delayNode)
            }

            engine.reset()
        }

        engine = nil
        gigaDenoiserUnit = nil
        delayNode = nil
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    private func configureEngine(gigaDenoiserUnit: DenoiserAVAudioUnit) throws {
        let engine = AVAudioEngine()
        let delayNode = AVAudioUnitDelay()
        delayNode.delayTime = 1.0
        delayNode.feedback = 0
        delayNode.wetDryMix = 100

        engine.attach(gigaDenoiserUnit)
        engine.attach(delayNode)

        let inputFormat = engine.inputNode.outputFormat(forBus: 0)
        engine.connect(engine.inputNode, to: gigaDenoiserUnit, format: inputFormat)
        engine.connect(gigaDenoiserUnit, to: delayNode, format: inputFormat)
        engine.connect(delayNode, to: engine.mainMixerNode, format: inputFormat)

        try engine.start()

        self.engine = engine
        self.gigaDenoiserUnit = gigaDenoiserUnit
        self.delayNode = delayNode
    }

    func setFeatures(_ features: Features) throws {
        try gigaDenoiserUnit?.setFeatures(features)
    }
}
