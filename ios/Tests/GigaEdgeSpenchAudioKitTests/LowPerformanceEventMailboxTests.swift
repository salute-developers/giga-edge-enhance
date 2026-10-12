import XCTest
@testable import GigaEdgeSpenchAudioKit
import GigaEdgeSpenchAudioKit_Private

final class LowPerformanceEventMailboxTests: XCTestCase {
    func testMailboxTransfersOneEventWithoutOverwriting() {
        let mailbox = LowPerformanceEventMailbox()

        XCTAssertTrue(mailbox.storeSampleRate(48_000,
                                              currentLatency: 0.096,
                                              meanProcessingMilliseconds: 4.5,
                                              refillCount: 3))
        XCTAssertFalse(mailbox.storeSampleRate(44_100,
                                               currentLatency: 0.128,
                                               meanProcessingMilliseconds: 8,
                                               refillCount: 4))

        var sampleRate = 0.0
        var currentLatency = 0.0
        var meanProcessingMilliseconds = 0.0
        var refillCount: UInt64 = 0
        XCTAssertTrue(mailbox.takeSampleRate(&sampleRate,
                                             currentLatency: &currentLatency,
                                             meanProcessingMilliseconds: &meanProcessingMilliseconds,
                                             refillCount: &refillCount))
        XCTAssertEqual(sampleRate, 48_000)
        XCTAssertEqual(currentLatency, 0.096)
        XCTAssertEqual(meanProcessingMilliseconds, 4.5)
        XCTAssertEqual(refillCount, 3)
        XCTAssertFalse(mailbox.takeSampleRate(&sampleRate,
                                              currentLatency: &currentLatency,
                                              meanProcessingMilliseconds: &meanProcessingMilliseconds,
                                              refillCount: &refillCount))
    }
}
