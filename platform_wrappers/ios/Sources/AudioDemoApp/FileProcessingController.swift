import AVFoundation
import SpenchAudioKit

final class FileProcessingController {
    private var engine: AVAudioEngine?
    private var player: AVAudioPlayerNode?
    private var spenchUnit: SpenchAVAudioUnit?
    private var outputFile: AVAudioFile?

    var telemetry: SpenchTelemetry {
        spenchUnit?.telemetry ?? .empty
    }
    var onProcessingFailure: ((String) -> Void)?

    func process(inputURL: URL,
                 features: SpenchFeatures,
                 settings: SpenchSettings,
                 onLowPerformance: @escaping (SpenchLowPerformanceEvent) -> Void,
                 completion: @escaping (Result<URL, Error>) -> Void) {
        do {
            let spenchUnit = try SpenchAVAudioUnit(features: features, settings: settings)
            spenchUnit.onLowPerformance = onLowPerformance
            spenchUnit.onProcessingFailure = onProcessingFailure
            try startProcessing(inputURL: inputURL,
                                spenchUnit: spenchUnit,
                                completion: completion)
        } catch {
            completion(.failure(error))
        }
    }

    func cancel() {
        cleanupProcessingGraph()
    }

    func setFeatures(_ features: SpenchFeatures) throws {
        try spenchUnit?.setFeatures(features)
    }

    private func startProcessing(inputURL: URL,
                                 spenchUnit: SpenchAVAudioUnit,
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
        engine.attach(spenchUnit)
        engine.mainMixerNode.outputVolume = 0

        engine.connect(player, to: spenchUnit, format: processingFormat)
        engine.connect(spenchUnit, to: engine.mainMixerNode, format: processingFormat)

        spenchUnit.installTap(onBus: 0,
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
        self.spenchUnit = spenchUnit
        self.outputFile = outputFile
    }

    private func cleanupProcessingGraph() {
        player?.stop()
        engine?.stop()

        if let engine {
            if let player {
                engine.disconnectAndDetachIfNeeded(player)
            }

            if let spenchUnit {
                spenchUnit.onLowPerformance = nil
                spenchUnit.onProcessingFailure = nil
                spenchUnit.removeTap(onBus: 0)
                engine.disconnectAndDetachIfNeeded(spenchUnit)
            }

            engine.reset()
        }

        engine = nil
        player = nil
        spenchUnit = nil
        outputFile = nil
    }
}
