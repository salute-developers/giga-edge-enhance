import XCTest
@testable import EnhanceAudioKit
import EnhanceAudioKit_Private

final class RingBufferTests: XCTestCase {
    func testPushPopRoundTrip() {
        let ring = RingBuffer(capacity: 8)
        let input: [Float] = [0, 1, 2, 3]
        var output = [Float](repeating: 0, count: 4)
        let outputCount = UInt32(output.count)

        let pushed = input.withUnsafeBufferPointer {
            ring.push($0.baseAddress!, count: UInt32(input.count))
        }
        XCTAssertEqual(pushed, 4)
        XCTAssertEqual(ring.availableToRead, 4)

        let popped = output.withUnsafeMutableBufferPointer {
            ring.pop($0.baseAddress!, count: outputCount)
        }
        XCTAssertEqual(popped, 4)
        XCTAssertEqual(output, input)
        XCTAssertEqual(ring.availableToRead, 0)
    }

    func testWrapAroundAndLimitedSpace() {
        let ring = RingBuffer(capacity: 8)
        let first: [Float] = [1, 2, 3, 4, 5, 6]
        var poppedFirst = [Float](repeating: 0, count: 4)
        let poppedFirstCount = UInt32(poppedFirst.count)

        _ = first.withUnsafeBufferPointer {
            ring.push($0.baseAddress!, count: UInt32(first.count))
        }

        _ = poppedFirst.withUnsafeMutableBufferPointer {
            ring.pop($0.baseAddress!, count: poppedFirstCount)
        }
        XCTAssertEqual(poppedFirst, [1, 2, 3, 4])

        let second: [Float] = [7, 8, 9, 10, 11, 12]
        let pushedSecond = second.withUnsafeBufferPointer {
            ring.push($0.baseAddress!, count: UInt32(second.count))
        }
        XCTAssertEqual(pushedSecond, 6)

        var finalOut = [Float](repeating: 0, count: 8)
        let finalOutCount = UInt32(finalOut.count)
        let finalCount = finalOut.withUnsafeMutableBufferPointer {
            ring.pop($0.baseAddress!, count: finalOutCount)
        }
        XCTAssertEqual(finalCount, 8)
        XCTAssertEqual(Array(finalOut.prefix(Int(finalCount))), [5, 6, 7, 8, 9, 10, 11, 12])
    }

    func testRingBufferDeallocates() {
        weak var weakRing: RingBuffer?

        autoreleasepool {
            let ring = RingBuffer(capacity: 32)
            weakRing = ring

            var scratch = [Float](repeating: 1, count: 16)
            let scratchCount = UInt32(scratch.count)
            _ = scratch.withUnsafeBufferPointer {
                ring.push($0.baseAddress!, count: scratchCount)
            }
            _ = scratch.withUnsafeMutableBufferPointer {
                ring.pop($0.baseAddress!, count: scratchCount)
            }
        }

        XCTAssertNil(weakRing)
    }
}
