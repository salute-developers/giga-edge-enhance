import XCTest
@testable import GigaDenoiserAudioKit

final class LowPerformanceMonitorTests: XCTestCase {
    func testNoEventBeforeThreshold() {
        var monitor = makeMonitor(maxRefillTimes: 3)

        XCTAssertNil(monitor.handleUnderrun(now: 0.1, meanProcessingTimeMilliseconds: 5))
        XCTAssertNil(monitor.handleUnderrun(now: 0.2, meanProcessingTimeMilliseconds: 5))
        XCTAssertEqual(monitor.currentLatencySeconds, 0.096, accuracy: 0.0001)
    }

    func testEventEmitsWhenThresholdReachedInsideWindow() {
        var monitor = makeMonitor(maxRefillTimes: 2)

        _ = monitor.handleUnderrun(now: 0.1, meanProcessingTimeMilliseconds: 4)
        let event = monitor.handleUnderrun(now: 0.2, meanProcessingTimeMilliseconds: 6)

        XCTAssertEqual(event?.sampleRate, 48_000)
        XCTAssertEqual(event?.currentLatency ?? 0, 0.096, accuracy: 0.0001)
        XCTAssertEqual(event?.meanProcessingTimeMilliseconds ?? 0, 6, accuracy: 0.0001)
        XCTAssertEqual(event?.refillCount, 2)
    }

    func testUnderrunsOutsideWindowExpire() {
        var monitor = makeMonitor(maxRefillTimes: 2, window: 0.1)

        XCTAssertNil(monitor.handleUnderrun(now: 0.1, meanProcessingTimeMilliseconds: nil))
        XCTAssertNil(monitor.handleUnderrun(now: 0.4, meanProcessingTimeMilliseconds: nil))
        XCTAssertEqual(monitor.currentLatencySeconds, 0.064, accuracy: 0.0001)
    }

    func testLatencyGrowthIsCapped() {
        var monitor = makeMonitor(maxRefillTimes: 10, minLatency: 0.2, step: 0.2, maxLatency: 0.256)

        for index in 0..<5 {
            _ = monitor.handleUnderrun(now: Double(index) * 0.01, meanProcessingTimeMilliseconds: nil)
        }

        XCTAssertEqual(monitor.currentLatencySeconds, 0.256, accuracy: 0.0001)
        XCTAssertEqual(monitor.primeThresholdFrames(), 12_288)
    }

    func testSlowRefillRelaxesPrimingRequirement() {
        var monitor = makeMonitor(maxRefillTimes: 4, refillTimeFactor: 1)

        XCTAssertNil(monitor.handleUnderrun(now: 0.1, meanProcessingTimeMilliseconds: 5))
        XCTAssertFalse(monitor.shouldRelaxRefillRequirement(now: 0.15))
        XCTAssertTrue(monitor.shouldRelaxRefillRequirement(now: 0.17))
    }

    func testStateResetsAfterEventEmission() {
        var monitor = makeMonitor(maxRefillTimes: 2)

        _ = monitor.handleUnderrun(now: 0.1, meanProcessingTimeMilliseconds: 5)
        _ = monitor.handleUnderrun(now: 0.2, meanProcessingTimeMilliseconds: 5)

        XCTAssertEqual(monitor.refillTimes.count, 0)
        XCTAssertEqual(monitor.refillStartTime, .infinity)
        XCTAssertNil(monitor.handleUnderrun(now: 0.3, meanProcessingTimeMilliseconds: 5))
    }

    func testResetRestoresInitialLatencyAndClearsTracking() {
        var monitor = makeMonitor(maxRefillTimes: 3)

        _ = monitor.handleUnderrun(now: 0.1, meanProcessingTimeMilliseconds: 5)
        monitor.reset(sampleRate: 44_100)

        XCTAssertEqual(monitor.sampleRate, 44_100)
        XCTAssertEqual(monitor.currentLatencySeconds, 0.064, accuracy: 0.0001)
        XCTAssertEqual(monitor.refillTimes.count, 0)
        XCTAssertEqual(monitor.refillStartTime, .infinity)
    }

    private func makeMonitor(maxRefillTimes: Int,
                             window: Double = 1,
                             refillTimeFactor: Double = 3,
                             minLatency: Double = 0.064,
                             step: Double = 0.032,
                             maxLatency: Double = 0.256) -> LowPerformanceMonitor {
        let settings = Settings(lowPerformanceWindowSeconds: window,
                                      lowPerformanceMaxRefillTimes: maxRefillTimes,
                                      lowPerformanceMaxRefillTimeFactor: refillTimeFactor,
                                      minLatency: minLatency,
                                      latencyIncreaseStep: step)
        return LowPerformanceMonitor(settings: settings,
                                           sampleRate: 48_000,
                                           maxLatencySeconds: maxLatency)
    }
}
