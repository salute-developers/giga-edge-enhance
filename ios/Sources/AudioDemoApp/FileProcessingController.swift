import AVFoundation
import GigaEdgeSpenchAudioKit

final class FileProcessingController {
    private var engine: AVAudioEngine?
    private var player: AVAudioPlayerNode?
    private var gigaEdgeSpenchUnit: DenoiserAVAudioUnit?
    private var outputFile: AVAudioFile?

    var telemetry: Telemetry {
        gigaEdgeSpenchUnit?.telemetry ?? .empty
    }
    var onProcessingFailure: ((String) -> Void)?

    func process(inputURL: URL,
                 features: Features,
                 settings: Settings,
                 onLowPerformance: @escaping (LowPerformanceEvent) -> Void,
                 completion: @escaping (Result<URL, Error>) -> Void) {
        do {
            let gigaEdgeSpenchUnit = try DenoiserAVAudioUnit(features: features, settings: settings)
            gigaEdgeSpenchUnit.onLowPerformance = onLowPerformance
            gigaEdgeSpenchUnit.onProcessingFailure = onProcessingFailure
            try startProcessing(inputURL: inputURL,
                                gigaEdgeSpenchUnit: gigaEdgeSpenchUnit,
                                completion: completion)
        } catch {
            completion(.failure(error))
        }
    }

    func cancel() {
        cleanupProcessingGraph()
    }

    func setFeatures(_ features: Features) throws {
        try gigaEdgeSpenchUnit?.setFeatures(features)
    }

    private func startProcessing(inputURL: URL,
                                 gigaEdgeSpenchUnit: DenoiserAVAudioUnit,
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
        engine.attach(gigaEdgeSpenchUnit)
        engine.mainMixerNode.outputVolume = 0

        engine.connect(player, to: gigaEdgeSpenchUnit, format: processingFormat)
        engine.connect(gigaEdgeSpenchUnit, to: engine.mainMixerNode, format: processingFormat)

        gigaEdgeSpenchUnit.installTap(onBus: 0,
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
        self.gigaEdgeSpenchUnit = gigaEdgeSpenchUnit
        self.outputFile = outputFile
    }

    private func cleanupProcessingGraph() {
        player?.stop()
        engine?.stop()

        if let engine {
            if let player {
                engine.disconnectAndDetachIfNeeded(player)
            }

            if let gigaEdgeSpenchUnit {
                gigaEdgeSpenchUnit.onLowPerformance = nil
                gigaEdgeSpenchUnit.onProcessingFailure = nil
                gigaEdgeSpenchUnit.removeTap(onBus: 0)
                engine.disconnectAndDetachIfNeeded(gigaEdgeSpenchUnit)
            }

            engine.reset()
        }

        engine = nil
        player = nil
        gigaEdgeSpenchUnit = nil
        outputFile = nil
    }
}
