import XCTest
@testable import GigaEdgeSpenchAudioKit

final class DenoiserAVAudioUnitTests: XCTestCase {
    func testRegisterIsThreadSafe() {
        let group = DispatchGroup()
        let queue = DispatchQueue(label: "DenoiserAVAudioUnitTests.register", attributes: .concurrent)

        for _ in 0..<20 {
            group.enter()
            queue.async {
                DenoiserAVAudioUnit.register()
                group.leave()
            }
        }

        XCTAssertEqual(group.wait(timeout: .now() + 2), .success)
        XCTAssertNoThrow(try DenoiserAVAudioUnit(features: .standard, settings: .default))
    }

    func testInitGigaEdgeSpenchAudioUnit() throws {
        let features = Features(normalization: false,
                                       noiseSuppression: ModelDescriptor(rawValue: "quality48"))
        let settings = Settings(lowPerformanceWindowSeconds: 2,
                                      lowPerformanceMaxRefillTimes: 5,
                                      lowPerformanceMaxRefillTimeFactor: 4,
                                      minLatency: 0.08,
                                      latencyIncreaseStep: 0.05,
                                      useVadNormalization: true,
                                      unstableReverseProcessingOrder: true)
        let audioUnit = try DenoiserAVAudioUnit(features: features, settings: settings)

        XCTAssertEqual(audioUnit.features, features)
        XCTAssertEqual(audioUnit.settings, settings)
        XCTAssertEqual(audioUnit.telemetry, Telemetry(audioProcessingCounters: [:], denoiserCounters: [:]))

        let updatedFeatures = Features(normalization: true, noiseSuppression: nil)
        try audioUnit.setFeatures(updatedFeatures)
        XCTAssertEqual(audioUnit.features, updatedFeatures)

        audioUnit.reset()
    }
}
