import XCTest
@testable import EnhanceAudioKit

final class EnhanceAVAudioUnitTests: XCTestCase {
    func testRegisterIsThreadSafe() {
        let group = DispatchGroup()
        let queue = DispatchQueue(label: "EnhanceAVAudioUnitTests.register", attributes: .concurrent)

        for _ in 0..<20 {
            group.enter()
            queue.async {
                EnhanceAVAudioUnit.register()
                group.leave()
            }
        }

        XCTAssertEqual(group.wait(timeout: .now() + 2), .success)
        XCTAssertNoThrow(try EnhanceAVAudioUnit(features: .standard, settings: .default))
    }

    func testInitGigaEdgeEnhanceAudioUnit() throws {
        let features = EnhanceFeatures(normalization: false,
                                       noiseSuppression: EnhanceModelDescriptor(rawValue: "quality48"))
        let settings = EnhanceSettings(lowPerformanceWindowSeconds: 2,
                                      lowPerformanceMaxRefillTimes: 5,
                                      lowPerformanceMaxRefillTimeFactor: 4,
                                      minLatency: 0.08,
                                      latencyIncreaseStep: 0.05,
                                      useVadNormalization: true,
                                      unstableReverseProcessingOrder: true)
        let audioUnit = try EnhanceAVAudioUnit(features: features, settings: settings)

        XCTAssertEqual(audioUnit.features, features)
        XCTAssertEqual(audioUnit.settings, settings)
        XCTAssertEqual(audioUnit.telemetry, EnhanceTelemetry(audioProcessingCounters: [:], denoiserCounters: [:]))

        let updatedFeatures = EnhanceFeatures(normalization: true, noiseSuppression: nil)
        try audioUnit.setFeatures(updatedFeatures)
        XCTAssertEqual(audioUnit.features, updatedFeatures)

        audioUnit.reset()
    }
}
