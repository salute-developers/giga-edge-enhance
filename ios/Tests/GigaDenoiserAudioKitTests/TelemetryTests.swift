import XCTest
@testable import GigaDenoiserAudioKit

final class TelemetryTests: XCTestCase {
    func testRoundedBucketMillisecondsUsesNearestConfiguredStep() {
        let store = TelemetryStore(roundingMilliseconds: 0.25)

        XCTAssertEqual(store.roundedBucketMilliseconds(for: 6.74), 6.75, accuracy: 0.000_001)
        XCTAssertEqual(store.roundedBucketMilliseconds(for: 6.51), 6.5, accuracy: 0.000_001)
        XCTAssertEqual(store.roundedBucketMilliseconds(for: 6.375), 6.5, accuracy: 0.000_001)
    }

    func testRepeatedEqualBucketsCollapseIntoSingleSnapshotKey() {
        let store = TelemetryStore(roundingMilliseconds: 0.25)

        store.recordAudioProcessing(milliseconds: 6.74)
        store.recordAudioProcessing(milliseconds: 6.76)
        store.recordAudioProcessing(milliseconds: 6.750_000_000_1)

        let snapshot = store.snapshot()

        XCTAssertEqual(snapshot.audioProcessingCounters.count, 1)
        XCTAssertEqual(snapshot.audioProcessingCounters[6.75], 3)
    }

    func testSnapshotSeparatesAudioProcessingAndDenoiserCounters() {
        let store = TelemetryStore(roundingMilliseconds: 0.125)

        store.recordAudioProcessing(milliseconds: 3.17)
        store.recordAudioProcessing(milliseconds: 3.18)
        store.recordDenoiser(milliseconds: 1.07)

        let snapshot = store.snapshot()

        XCTAssertEqual(snapshot.audioProcessingCounters, [3.125: 2])
        XCTAssertEqual(snapshot.denoiserCounters, [1.125: 1])
    }

    func testTelemetryDifferenceClampsAtZero() {
        let baseline = Telemetry(audioProcessingCounters: [1.0: 4, 2.0: 2],
                                       denoiserCounters: [3.0: 5])
        let current = Telemetry(audioProcessingCounters: [1.0: 6, 2.0: 1, 4.0: 2],
                                      denoiserCounters: [3.0: 5, 5.0: 1])

        let difference = current - baseline

        XCTAssertEqual(difference.audioProcessingCounters, [1.0: 2, 4.0: 2])
        XCTAssertEqual(difference.denoiserCounters, [5.0: 1])
    }

    func testMeanUsesWeightedAverage() {
        let telemetry = Telemetry(audioProcessingCounters: [1.0: 1, 2.0: 3],
                                        denoiserCounters: [0.5: 2, 1.5: 2])

        XCTAssertNotNil(telemetry.audioProcessingMeanMilliseconds)
        XCTAssertNotNil(telemetry.denoiserMeanMilliseconds)
        XCTAssertEqual(telemetry.audioProcessingMeanMilliseconds!, 1.75, accuracy: 0.000_001)
        XCTAssertEqual(telemetry.denoiserMeanMilliseconds!, 1.0, accuracy: 0.000_001)
    }
}
