import AVFoundation
import EnhanceAudioKit

final class FileProcessingController {
    private var engine: AVAudioEngine?
    private var player: AVAudioPlayerNode?
    private var gigaEdgeEnhanceUnit: EnhanceAVAudioUnit?
    private var outputFile: AVAudioFile?

    var telemetry: EnhanceTelemetry {
        gigaEdgeEnhanceUnit?.telemetry ?? .empty
    }
    var onProcessingFailure: ((String) -> Void)?

    func process(inputURL: URL,
                 features: EnhanceFeatures,
                 settings: EnhanceSettings,
                 onLowPerformance: @escaping (EnhanceLowPerformanceEvent) -> Void,
                 completion: @escaping (Result<URL, Error>) -> Void) {
        do {
            let gigaEdgeEnhanceUnit = try EnhanceAVAudioUnit(features: features, settings: settings)
            gigaEdgeEnhanceUnit.onLowPerformance = onLowPerformance
            gigaEdgeEnhanceUnit.onProcessingFailure = onProcessingFailure
            try startProcessing(inputURL: inputURL,
                                gigaEdgeEnhanceUnit: gigaEdgeEnhanceUnit,
                                completion: completion)
        } catch {
            completion(.failure(error))
        }
    }

    func cancel() {
        cleanupProcessingGraph()
    }

    func setFeatures(_ features: EnhanceFeatures) throws {
        try gigaEdgeEnhanceUnit?.setFeatures(features)
    }

    private func startProcessing(inputURL: URL,
                                 gigaEdgeEnhanceUnit: EnhanceAVAudioUnit,
                                 completion: @escaping (Result<URL, Error>) -> Void) throws {
        let inputFile = try AVAudioFile(forReading: inputURL)
        let engine = AVAudioEngine()
        let player = AVAudioPlayerNode()
        let processingFormat = inputFile.processingFormat
        let outputURL = FileManager.default.temporaryDirectory
            .appendingPathComponent("processed-\(UUID().uuidString).wav")
        let outputFile = try AVAudioFile(forWriting: outputURL,
                                         settings: processingFormat.settings,
                                         commonFormat: .pcmFormatFloat32,
                                         interleaved: false)
        var didFinish = false

        let finish: (Result<URL, Error>) -> Void = { [weak self] result in
            guard !didFinish else { return }
            didFinish = true
            self?.cleanupProcessingGraph()
            completion(result)
        }

        engine.attach(player)
        engine.attach(gigaEdgeEnhanceUnit)
        engine.mainMixerNode.outputVolume = 0

        engine.connect(player, to: gigaEdgeEnhanceUnit, format: processingFormat)
        engine.connect(gigaEdgeEnhanceUnit, to: engine.mainMixerNode, format: processingFormat)

        gigaEdgeEnhanceUnit.installTap(onBus: 0,
                              bufferSize: 1_024,
                              format: processingFormat) { buffer, _ in
            do {
                try outputFile.write(from: buffer)
            } catch {
                DispatchQueue.main.async {
                    finish(.failure(error))
                }
            }
        }

        player.scheduleFile(inputFile, at: nil) {
            DispatchQueue.main.async {
                finish(.success(outputURL))
            }
        }

        try engine.start()
        player.play()

        self.engine = engine
        self.player = player
        self.gigaEdgeEnhanceUnit = gigaEdgeEnhanceUnit
        self.outputFile = outputFile
    }

    private func cleanupProcessingGraph() {
        player?.stop()
        engine?.stop()

        if let engine {
            if let player {
                engine.disconnectAndDetachIfNeeded(player)
            }

            if let gigaEdgeEnhanceUnit {
                gigaEdgeEnhanceUnit.onLowPerformance = nil
                gigaEdgeEnhanceUnit.onProcessingFailure = nil
                gigaEdgeEnhanceUnit.removeTap(onBus: 0)
                engine.disconnectAndDetachIfNeeded(gigaEdgeEnhanceUnit)
            }

            engine.reset()
        }

        engine = nil
        player = nil
        gigaEdgeEnhanceUnit = nil
        outputFile = nil
    }
}
