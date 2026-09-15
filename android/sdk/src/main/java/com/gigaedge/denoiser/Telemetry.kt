package com.gigaedge.denoiser

import androidx.annotation.Keep

/**
 * Cumulative processing-time telemetry for the feature stages, read from [Processor.telemetry].
 *
 * Each map is a histogram whose key is a bucket midpoint in milliseconds (a multiple of the
 * processor's `telemetryRoundingMilliseconds`) and whose value is the number of processed chunks
 * that fell in that bucket. Counts accumulate for the lifetime of the processor and are not cleared
 * by reset. Subtract an earlier snapshot with [minus] to obtain a per-interval delta.
 *
 * @property normalizationCounters histogram of normalization-stage durations, keyed by milliseconds.
 * @property noiseSuppressionCounters histogram of noise-suppression-stage durations, keyed by
 *   milliseconds.
 */
data class Telemetry(
    val normalizationCounters: Map<Double, Int>,
    val noiseSuppressionCounters: Map<Double, Int>,
) {
    /** Count-weighted mean normalization duration in milliseconds, or `null` if no samples. */
    val normalizationMeanMilliseconds: Double?
        get() = mean(normalizationCounters)

    /** Count-weighted mean noise-suppression duration in milliseconds, or `null` if no samples. */
    val noiseSuppressionMeanMilliseconds: Double?
        get() = mean(noiseSuppressionCounters)

    /**
     * Returns the per-bucket delta `this - other`, keeping only strictly positive counts.
     *
     * Use it to measure the work done between two snapshots: read telemetry, process, read again,
     * then subtract the earlier value from the later one.
     */
    operator fun minus(other: Telemetry): Telemetry = Telemetry(
        normalizationCounters = subtract(normalizationCounters, other.normalizationCounters),
        noiseSuppressionCounters = subtract(noiseSuppressionCounters, other.noiseSuppressionCounters),
    )

    private companion object {
        fun subtract(lhs: Map<Double, Int>, rhs: Map<Double, Int>): Map<Double, Int> = lhs.mapNotNull { (bucket, count) ->
            val delta = count.toLong() - (rhs[bucket] ?: 0).toLong()
            if (delta > 0) bucket to delta.coerceAtMost(Int.MAX_VALUE.toLong()).toInt() else null
        }.toMap()

        fun mean(counters: Map<Double, Int>): Double? {
            val sampleCount = counters.values.sumOf { it.toDouble() }
            if (sampleCount <= 0) return null
            return counters.entries.sumOf { (bucket, count) -> bucket * count.toDouble() } / sampleCount
        }
    }
}

/** Constructed from native (JNI) code; keep the constructor for minified consumer builds. */
@Keep
internal data class NativeTelemetry(
    val normalizationBuckets: DoubleArray,
    val normalizationCounts: IntArray,
    val noiseSuppressionBuckets: DoubleArray,
    val noiseSuppressionCounts: IntArray,
)
