import Foundation

/// Cumulative per-stage processing-time histograms collected by a GigaEdgeSpench
/// processor or audio unit.
///
/// Telemetry is read-only and snapshot-style: every call to
/// ``DenoiserAVAudioUnit/telemetry`` or ``Processor/telemetry`` returns a
/// fresh, immutable copy. Use ``-(_:_:)`` to compute a positive delta between
/// two snapshots when you need windowed statistics.
///
/// See <doc:Telemetry> for usage patterns.
public struct Telemetry: Equatable, Sendable {
    /// Empty snapshot, useful as a baseline placeholder.
    public static let empty = Telemetry(audioProcessingCounters: [:], denoiserCounters: [:])

    /// Histogram of normalization-stage durations in milliseconds, bucketed
    /// by ``Settings/telemetryRoundingMilliseconds``.
    public let audioProcessingCounters: [Double: Int]
    /// Histogram of denoiser-stage durations in milliseconds, bucketed by
    /// ``Settings/telemetryRoundingMilliseconds``.
    public let denoiserCounters: [Double: Int]

    internal init(audioProcessingCounters: [Double: Int], denoiserCounters: [Double: Int]) {
        self.audioProcessingCounters = audioProcessingCounters
        self.denoiserCounters = denoiserCounters
    }

    /// Weighted mean of `audioProcessingCounters`.
    public var audioProcessingMeanMilliseconds: Double? {
        Self.mean(for: audioProcessingCounters)
    }

    /// Weighted mean of `denoiserCounters`.
    public var denoiserMeanMilliseconds: Double? {
        Self.mean(for: denoiserCounters)
    }

    /// Returns the positive delta `lhs - rhs`. Buckets that do not increase
    /// are dropped from the result; buckets that newly appear in `lhs`
    /// are kept. Use this to compute windowed counters between two
    /// snapshots taken on the same processor or audio unit.
    public static func - (lhs: Telemetry, rhs: Telemetry) -> Telemetry {
        Telemetry(audioProcessingCounters: subtract(lhs.audioProcessingCounters, rhs.audioProcessingCounters),
                        denoiserCounters: subtract(lhs.denoiserCounters, rhs.denoiserCounters))
    }

    private static func subtract(_ lhs: [Double: Int], _ rhs: [Double: Int]) -> [Double: Int] {
        var difference: [Double: Int] = [:]

        for (bucket, count) in lhs {
            let delta = count - rhs[bucket, default: 0]
            if delta > 0 {
                difference[bucket] = delta
            }
        }

        return difference
    }

    private static func mean(for counters: [Double: Int]) -> Double? {
        let sampleCount = counters.values.reduce(0, +)
        guard sampleCount > 0 else { return nil }

        let weightedSum = counters.reduce(0.0) { partialResult, entry in
            partialResult + (entry.key * Double(entry.value))
        }
        return weightedSum / Double(sampleCount)
    }
}

internal final class TelemetryStore {
    private let stateLock = UnfairLock()
    private let roundingMilliseconds: Double
    private var audioProcessingCounters: [Int: Int] = [:]
    private var denoiserCounters: [Int: Int] = [:]

    init(roundingMilliseconds: Double) {
        self.roundingMilliseconds = roundingMilliseconds
    }

    func recordAudioProcessing(milliseconds: Double) {
        record(milliseconds: milliseconds, in: \TelemetryStore.audioProcessingCounters)
    }

    func recordDenoiser(milliseconds: Double) {
        record(milliseconds: milliseconds, in: \TelemetryStore.denoiserCounters)
    }

    func snapshot() -> Telemetry {
        stateLock.withLock {
            Telemetry(audioProcessingCounters: makeSnapshot(from: audioProcessingCounters),
                            denoiserCounters: makeSnapshot(from: denoiserCounters))
        }
    }

    func roundedBucketMilliseconds(for milliseconds: Double) -> Double {
        Double(bucketIndex(for: milliseconds)) * roundingMilliseconds
    }

    private func record(milliseconds: Double, in keyPath: ReferenceWritableKeyPath<TelemetryStore, [Int: Int]>) {
        let bucketIndex = bucketIndex(for: milliseconds)
        stateLock.withLock {
            self[keyPath: keyPath][bucketIndex, default: 0] += 1
        }
    }

    private func bucketIndex(for milliseconds: Double) -> Int {
        Int((milliseconds / roundingMilliseconds).rounded())
    }

    private func makeSnapshot(from counters: [Int: Int]) -> [Double: Int] {
        Dictionary(uniqueKeysWithValues: counters.map { (Double($0.key) * roundingMilliseconds, $0.value) })
    }
}
