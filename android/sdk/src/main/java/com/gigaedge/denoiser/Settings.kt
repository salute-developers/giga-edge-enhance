package com.gigaedge.denoiser

/**
 * Immutable processor configuration captured by [Processor] at construction.
 *
 * Invariants are enforced in [init], so an invalid instance can never be constructed and every
 * consumer — including `copy(...)` — is validated against this single source of truth. Because a
 * held instance is always valid, callers do not need to re-validate settings elsewhere.
 *
 * All latency values are in seconds; [telemetryRoundingMilliseconds] is in milliseconds. The
 * `unstable*` fields are advanced/experimental knobs that may change and are off by default.
 *
 * @property lowPerformanceWindowSeconds sliding window, in seconds, over which output refills
 *   (underruns) are counted for the low-performance signal. Must be finite and greater than zero.
 * @property lowPerformanceMaxRefillTimes number of refills within the window that triggers a
 *   [LowPerformanceEvent]. Must be greater than zero.
 * @property lowPerformanceMaxRefillTimeFactor multiplier applied to the current latency that bounds
 *   how long the worker may take to refill before the requirement is relaxed. Must be finite and
 *   non-negative.
 * @property minLatency initial and minimum output latency in seconds. Must be finite and
 *   non-negative.
 * @property latencyIncreaseStep amount, in seconds, by which latency grows per additional refill in
 *   the window. Must be finite and non-negative.
 * @property maxProcessingChunkSeconds maximum real-time processing chunk duration in seconds. The
 *   effective duration is capped by [minLatency] and 256 ms, floored to 16 ms steps, and never
 *   falls below 16 ms. Must be finite and greater than zero.
 * @property telemetryRoundingMilliseconds bucket width, in milliseconds, for telemetry histograms.
 *   Must be finite and greater than zero.
 * @property useVadNormalization enables voice-activity-driven normalization tuning. Ignored when
 *   [unstableCustomNormalizationConfig] is set.
 * @property unstableReverseProcessingOrder runs noise suppression before normalization instead of
 *   after. Experimental.
 * @property unstableCustomNormalizationConfig raw normalization config (YAML) that overrides the
 *   built-in normalization chain when non-null. Experimental.
 * @throws IllegalArgumentException if any invariant above is violated.
 */
data class Settings(
    val lowPerformanceWindowSeconds: Double = 1.0,
    val lowPerformanceMaxRefillTimes: Int = 4,
    val lowPerformanceMaxRefillTimeFactor: Double = 3.0,
    val minLatency: Double = 0.064,
    val latencyIncreaseStep: Double = 0.032,
    val telemetryRoundingMilliseconds: Double = 0.25,
    val useVadNormalization: Boolean = false,
    val unstableReverseProcessingOrder: Boolean = false,
    val unstableCustomNormalizationConfig: String? = null,
    val maxProcessingChunkSeconds: Double = 0.016,
) {
    init {
        require(lowPerformanceWindowSeconds > 0.0 && lowPerformanceWindowSeconds.isFinite()) {
            "lowPerformanceWindowSeconds must be greater than zero"
        }
        require(lowPerformanceMaxRefillTimes > 0) {
            "lowPerformanceMaxRefillTimes must be greater than zero"
        }
        require(lowPerformanceMaxRefillTimeFactor >= 0.0 && lowPerformanceMaxRefillTimeFactor.isFinite()) {
            "lowPerformanceMaxRefillTimeFactor must be finite and non-negative"
        }
        require(minLatency >= 0.0 && minLatency.isFinite()) {
            "minLatency must be finite and non-negative"
        }
        require(latencyIncreaseStep >= 0.0 && latencyIncreaseStep.isFinite()) {
            "latencyIncreaseStep must be finite and non-negative"
        }
        require(maxProcessingChunkSeconds > 0.0 && maxProcessingChunkSeconds.isFinite()) {
            "maxProcessingChunkSeconds must be finite and greater than zero"
        }
        require(telemetryRoundingMilliseconds > 0.0 && telemetryRoundingMilliseconds.isFinite()) {
            "telemetryRoundingMilliseconds must be greater than zero"
        }
    }
}
