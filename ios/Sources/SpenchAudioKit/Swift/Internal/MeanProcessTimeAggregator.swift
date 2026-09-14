import Foundation

/// Sliding-window arithmetic mean for the worker thread. Keeps an O(1)
/// running sum so the per-chunk mean update doesn't touch O(window) memory
/// like a naive `reduce` would.
struct MeanProcessTimeAggregator {
    private let window: Int
    private var samples: [Double]
    private var sum: Double = 0
    private var index: Int = 0
    private var filled: Int = 0

    init(window: Int) {
        self.window = max(1, window)
        samples = [Double](repeating: 0, count: self.window)
    }

    mutating func append(_ value: Double) -> Double {
        if filled == window {
            sum -= samples[index]
        } else {
            filled += 1
        }
        samples[index] = value
        sum += value
        index = (index + 1) % window

        return sum / Double(filled)
    }

    mutating func reset() {
        sum = 0
        index = 0
        filled = 0
        samples.withUnsafeMutableBufferPointer { pointer in
            pointer.update(repeating: 0)
        }
    }
}
