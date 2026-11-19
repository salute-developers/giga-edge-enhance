import Foundation

// Render-owned state machine that adjusts priming latency after repeated underruns.
internal struct EnhanceLowPerformanceMonitor {
    private let maxLatencySeconds: Double

    private let settings: EnhanceSettings
    private(set) var sampleRate: Double
    private(set) var currentLatencySeconds: Double
    private(set) var refillTimes: [Double] = []
    private(set) var refillStartTime: Double = .infinity

    init(settings: EnhanceSettings,
         sampleRate: Double,
         maxLatencySeconds: Double) {
        self.settings = settings
        self.sampleRate = sampleRate
        self.maxLatencySeconds = maxLatencySeconds
        currentLatencySeconds = min(max(settings.minLatency, 0), maxLatencySeconds)
        refillTimes.reserveCapacity(settings.lowPerformanceMaxRefillTimes)
    }

    mutating func reset(sampleRate: Double) {
        self.sampleRate = sampleRate
        currentLatencySeconds = min(max(settings.minLatency, 0), maxLatencySeconds)
        refillTimes.removeAll(keepingCapacity: true)
        refillStartTime = .infinity
    }

    mutating func updateSampleRate(_ sampleRate: Double) {
        self.sampleRate = sampleRate
    }

    mutating func markRefilled() {
        refillStartTime = .infinity
    }

    mutating func shouldRelaxRefillRequirement(now: Double) -> Bool {
        guard refillStartTime.isFinite else { return false }

        // Once refill takes too long, allow rendering to continue with whatever audio
        // is available instead of waiting for the prime threshold again.
        let elapsed = now - refillStartTime
        let maxRefillTime = settings.lowPerformanceMaxRefillTimeFactor * currentLatencySeconds
        guard elapsed > maxRefillTime else { return false }

        refillStartTime = .infinity
        return true
    }

    mutating func handleUnderrun(now: Double, meanProcessingTimeMilliseconds: Double?) -> EnhanceLowPerformanceEvent? {
        refillStartTime = now
        // Keep only the underruns that belong to the current rolling detection window.
        refillTimes.removeAll { now - $0 > settings.lowPerformanceWindowSeconds }
        refillTimes.append(now)

        // Each repeated underrun increases the target latency until the configured cap.
        currentLatencySeconds = min(
            Double(refillTimes.count - 1) * settings.latencyIncreaseStep + settings.minLatency,
            maxLatencySeconds
        )

        guard refillTimes.count >= settings.lowPerformanceMaxRefillTimes else {
            return nil
        }

        let event = EnhanceLowPerformanceEvent(sampleRate: sampleRate,
                                              currentLatency: currentLatencySeconds,
                                              meanProcessingTimeMilliseconds: meanProcessingTimeMilliseconds,
                                              refillCount: refillTimes.count)
        // Start a fresh detection cycle after notifying the client.
        refillTimes.removeAll(keepingCapacity: true)
        refillStartTime = .infinity
        return event
    }

    func primeThresholdFrames() -> UInt32 {
        max(1, UInt32((currentLatencySeconds * sampleRate).rounded(.up)))
    }
}
