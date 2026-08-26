import XCTest
@testable import SpenchAudioKit

final class SpenchAVAudioUnitTests: XCTestCase {
    func testRegisterIsThreadSafe() {
        let group = DispatchGroup()
        let queue = DispatchQueue(label: "SpenchAVAudioUnitTests.register", attributes: .concurrent)

        for _ in 0..<20 {
            group.enter()
            queue.async {
                SpenchAVAudioUnit.register()
                group.leave()
            }
        }

        XCTAssertEqual(group.wait(timeout: .now() + 2), .success)
        XCTAssertNoThrow(try SpenchAVAudioUnit(features: .standard, settings: .default))
    }

    func testInitSpenchAudioUnit() throws {
        let features = SpenchFeatures(normalization: false,
                                       noiseSuppression: SpenchModelDescriptor(rawValue: "quality48k"))
        let settings = SpenchSettings(lowPerformanceWindowSeconds: 2,
                                      lowPerformanceMaxRefillTimes: 5,
                                      lowPerformanceMaxRefillTimeFactor: 4,
                                      minLatency: 0.08,
                                      latencyIncreaseStep: 0.05,
                                      useVadNormalization: true,
                                      unstableReverseProcessingOrder: true)
        let audioUnit = try SpenchAVAudioUnit(features: features, settings: settings)

        XCTAssertEqual(audioUnit.features, features)
        XCTAssertEqual(audioUnit.settings, settings)
        XCTAssertEqual(audioUnit.telemetry, SpenchTelemetry(audioProcessingCounters: [:], denoiserCounters: [:]))

        let updatedFeatures = SpenchFeatures(normalization: true, noiseSuppression: nil)
        try audioUnit.setFeatures(updatedFeatures)
        XCTAssertEqual(audioUnit.features, updatedFeatures)

        audioUnit.reset()
    }
}
