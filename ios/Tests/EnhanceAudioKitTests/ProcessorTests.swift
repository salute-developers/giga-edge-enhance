import XCTest
@testable import EnhanceAudioKit

final class ProcessorTests: XCTestCase {
    func testProcessorPassesThroughWhenFeaturesDisabled() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100,
                                            features: EnhanceFeatures(normalization: false,
                                                                     noiseSuppression: nil))

        let input: [Float] = [0.5, -0.5, 0.25, -0.25]
        var output = Array(repeating: Float.zero, count: input.count)

        let processed = try processor.process(input: input, output: &output)

        XCTAssertEqual(processed, UInt32(input.count))
        XCTAssertEqual(output, input)
        XCTAssertEqual(processor.telemetry.audioProcessingCounters, [:])
        XCTAssertEqual(processor.telemetry.denoiserCounters, [:])
    }

    func testFeaturesCanBeUpdated() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100)

        XCTAssertEqual(processor.features, .standard)

        let disabledFeatures = EnhanceFeatures(normalization: false, noiseSuppression: nil)
        try processor.setFeatures(disabledFeatures)
        XCTAssertEqual(processor.features, disabledFeatures)

        let enabledFeatures = EnhanceFeatures(normalization: true,
                                              noiseSuppression: EnhanceModelDescriptor(rawValue: "quality48"))
        try processor.setFeatures(enabledFeatures)
        XCTAssertEqual(processor.features, enabledFeatures)
    }

    func testInvalidNoiseSuppressionModelThrows() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100)

        XCTAssertThrowsError(try processor.setFeatures(EnhanceFeatures(normalization: true,
                                                                      noiseSuppression: EnhanceModelDescriptor(rawValue: "missing"))))
    }

    func testSampleRateCanBeUpdated() throws {
        let features = EnhanceFeatures(normalization: true,
                                       noiseSuppression: EnhanceModelDescriptor(rawValue: "quality48"))
        let processor = try EnhanceProcessor(sampleRate: 44_100, features: features)

        try processor.setSampleRate(48_000)

        XCTAssertEqual(processor.sampleRate, 48_000)
        XCTAssertEqual(processor.features, features)
    }

    func testInvalidSampleRateThrows() {
        XCTAssertThrowsError(try EnhanceProcessor(sampleRate: 0))
    }

    func testInvalidSettingsThrow() {
        let settings = EnhanceSettings(lowPerformanceWindowSeconds: 0)
        XCTAssertThrowsError(try EnhanceProcessor(sampleRate: 44_100, settings: settings))
    }

    func testValidatedSettingsUseGigaEdgeErrorDomain() {
        let settings = EnhanceSettings(lowPerformanceWindowSeconds: 0)

        XCTAssertThrowsError(try settings.validated()) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, "com.gigaedge.EnhanceAudioKit.EnhanceError")
        }
    }

    func testInvalidTelemetryRoundingThrows() {
        let settings = EnhanceSettings(telemetryRoundingMilliseconds: 0)
        XCTAssertThrowsError(try EnhanceProcessor(sampleRate: 44_100, settings: settings))
    }

    func testInvalidMaxProcessingChunkThrows() {
        XCTAssertThrowsError(try EnhanceSettings(maxProcessingChunkSeconds: 0).validated())
        XCTAssertThrowsError(try EnhanceSettings(maxProcessingChunkSeconds: .infinity).validated())
    }

    func testMismatchedBuffersThrow() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100)
        let input: [Float] = [1, 2]
        var output: [Float] = [0]
        XCTAssertThrowsError(try processor.process(input: input, output: &output))
    }

    func testResetSucceedsForConfiguredPipelines() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100,
                                            features: EnhanceFeatures(normalization: true,
                                                                      noiseSuppression: EnhanceModelDescriptor(rawValue: "quality48")))

        let input = Array(repeating: Float(0.25), count: 512)
        var output = Array(repeating: Float.zero, count: input.count)
        _ = try processor.process(input: input, output: &output)
        let telemetryBeforeReset = processor.telemetry

        try processor.reset()

        XCTAssertEqual(processor.features.noiseSuppression, EnhanceModelDescriptor(rawValue: "quality48"))
        XCTAssertEqual(processor.telemetry, telemetryBeforeReset)
    }

    func testResetRestoresNormalizationState() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100,
                                            features: EnhanceFeatures(normalization: true,
                                                                     noiseSuppression: nil))
        let input = Array(repeating: Float(0.25), count: 512)
        var initialOutput = Array(repeating: Float.zero, count: input.count)
        _ = try processor.process(input: input, output: &initialOutput)

        var output = Array(repeating: Float.zero, count: input.count)
        for _ in 0..<20 {
            _ = try processor.process(input: input, output: &output)
        }

        try processor.reset()
        _ = try processor.process(input: input, output: &output)

        XCTAssertEqual(output, initialOutput)
    }

    func testTelemetryRecordsAudioProcessingCounters() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100,
                                            features: EnhanceFeatures(normalization: true, noiseSuppression: nil),
                                            settings: EnhanceSettings(telemetryRoundingMilliseconds: 0.25))

        let input = Array(repeating: Float(0.25), count: 512)
        var output = Array(repeating: Float.zero, count: input.count)
        _ = try processor.process(input: input, output: &output)

        let telemetry = processor.telemetry
        XCTAssertEqual(telemetry.denoiserCounters, [:])
        XCTAssertEqual(telemetry.audioProcessingCounters.values.reduce(0, +), 1)
        XCTAssertTrue(telemetry.audioProcessingCounters.keys.allSatisfy { isMultiple(of: 0.25, value: $0) })
    }

    func testTelemetryRecordsBothProcessingStages() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100,
                                            features: EnhanceFeatures(normalization: true,
                                                                     noiseSuppression: EnhanceModelDescriptor(rawValue: "quality48")),
                                            settings: EnhanceSettings(telemetryRoundingMilliseconds: 0.25))

        let input = Array(repeating: Float(0.25), count: 512)
        var output = Array(repeating: Float.zero, count: input.count)
        _ = try processor.process(input: input, output: &output)

        let telemetry = processor.telemetry
        XCTAssertEqual(telemetry.audioProcessingCounters.values.reduce(0, +), 1)
        XCTAssertEqual(telemetry.denoiserCounters.values.reduce(0, +), 1)
    }

    func testDenoiserProcessesNativeChunk() throws {
        let processor = try EnhanceProcessor(
            sampleRate: 48_000,
            features: EnhanceFeatures(normalization: false, noiseSuppression: .quality48)
        )
        var samples = Array(repeating: Float(0.25), count: 1_536)
        let frameCount = samples.count
        var processingFailure: String?

        for _ in 0..<4 {
            processingFailure = samples.withUnsafeMutableBufferPointer { pointer in
                processor.processInPlace(pointer.baseAddress!, frameCount: frameCount)
            }
        }

        XCTAssertNil(processingFailure)
    }

    func testDenoiserProcessesNativeChunkForHybrid48() throws {
        let processor = try EnhanceProcessor(
            sampleRate: 48_000,
            features: EnhanceFeatures(normalization: false, noiseSuppression: .hybrid48)
        )
        var samples = Array(repeating: Float(0.25), count: 1_536)
        let frameCount = samples.count
        var processingFailure: String?

        for _ in 0..<4 {
            processingFailure = samples.withUnsafeMutableBufferPointer { pointer in
                processor.processInPlace(pointer.baseAddress!, frameCount: frameCount)
            }
        }

        XCTAssertNil(processingFailure)
    }

    func testTelemetrySurvivesSampleRateUpdate() throws {
        let processor = try EnhanceProcessor(sampleRate: 44_100,
                                            features: EnhanceFeatures(normalization: true, noiseSuppression: nil))
        let input = Array(repeating: Float(0.25), count: 512)
        var output = Array(repeating: Float.zero, count: input.count)

        _ = try processor.process(input: input, output: &output)
        let initialCount = processor.telemetry.audioProcessingCounters.values.reduce(0, +)

        try processor.setSampleRate(48_000)
        _ = try processor.process(input: input, output: &output)

        XCTAssertEqual(processor.telemetry.audioProcessingCounters.values.reduce(0, +), initialCount + 1)
    }

    func testAvailableModelsFindInstalledBundles() throws {
        let modelIDs = try EnhanceModelCatalog.availableModels().map(\.rawValue)

        XCTAssertEqual(modelIDs, ["fast8", "hybrid48", "quality48"])
    }

    func testNormalizationConfigIncludesVadParametersWhenEnabled() {
        let settings = EnhanceSettings(useVadNormalization: true)

        let config = EnhanceProcessor.normalizationConfig(sampleRate: 44_100, settings: settings)

        XCTAssertTrue(config.contains("use_vad: true"))
        XCTAssertTrue(config.contains("target_level_db: -18.0"))
        XCTAssertTrue(config.contains("sample_rate: 44100"))
    }

    func testNormalizationConfigUsesCustomOverrideWhenProvided() {
        let customConfig = "sound_processing:\n  loudness_ctrl:\n    sample_rate: 8000\n"
        let settings = EnhanceSettings(unstableCustomNormalizationConfig: customConfig)

        let config = EnhanceProcessor.normalizationConfig(sampleRate: 48_000, settings: settings)

        XCTAssertEqual(config, customConfig)
    }

    private func isMultiple(of step: Double, value: Double) -> Bool {
        let ratio = value / step
        return abs(ratio.rounded() - ratio) < 0.000_001
    }
}
