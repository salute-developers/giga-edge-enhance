import AVFoundation
import SpenchAudioKit

final class LiveAudioController {
    private var engine: AVAudioEngine?
    private var spenchUnit: SpenchAVAudioUnit?
    private var delayNode: AVAudioUnitDelay?

    var telemetry: SpenchTelemetry {
        spenchUnit?.telemetry ?? .empty
    }

    func start(features: SpenchFeatures,
               settings: SpenchSettings,
               onLowPerformance: @escaping (SpenchLowPerformanceEvent) -> Void,
               completion: @escaping (Error?) -> Void) {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
            try session.setActive(true)
            let spenchUnit = try SpenchAVAudioUnit(features: features, settings: settings)
            spenchUnit.onLowPerformance = onLowPerformance
            try configureEngine(spenchUnit: spenchUnit)
            completion(nil)
        } catch {
            completion(error)
        }
    }

    func stop() {
        if let engine {
            engine.stop()
            engine.pause()

            if let spenchUnit {
                spenchUnit.onLowPerformance = nil
                spenchUnit.reset()
                if let audioUnit = spenchUnit.auAudioUnit as? SpenchAUAudioUnit,
                   audioUnit.renderResourcesAllocated {
                    audioUnit.deallocateRenderResources()
                }
                engine.disconnectAndDetachIfNeeded(spenchUnit)
            }

            if let delayNode {
                engine.disconnectAndDetachIfNeeded(delayNode)
            }

            engine.reset()
        }

        engine = nil
        spenchUnit = nil
        delayNode = nil
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    private func configureEngine(spenchUnit: SpenchAVAudioUnit) throws {
        let engine = AVAudioEngine()
        let delayNode = AVAudioUnitDelay()
        delayNode.delayTime = 1.0
        delayNode.feedback = 0
        delayNode.wetDryMix = 100

        engine.attach(spenchUnit)
        engine.attach(delayNode)

        let inputFormat = engine.inputNode.outputFormat(forBus: 0)
        engine.connect(engine.inputNode, to: spenchUnit, format: inputFormat)
        engine.connect(spenchUnit, to: delayNode, format: inputFormat)
        engine.connect(delayNode, to: engine.mainMixerNode, format: inputFormat)

        try engine.start()

        self.engine = engine
        self.spenchUnit = spenchUnit
        self.delayNode = delayNode
    }

    func setFeatures(_ features: SpenchFeatures) throws {
        try spenchUnit?.setFeatures(features)
    }
}
