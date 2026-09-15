package com.gigaedge.denoiser

import androidx.annotation.Keep

/**
 * Signal that the processor is struggling to keep up and has raised its output latency.
 *
 * Emitted when the number of output refills within the configured window reaches
 * [Settings.lowPerformanceMaxRefillTimes]. Delivered through [Processor.onLowPerformance].
 *
 * Constructed from native (JNI) code; the constructor is kept for minified consumer builds via the
 * SDK's consumer ProGuard rules.
 *
 * @property sampleRateHz the processor's input sample rate in Hz.
 * @property currentLatencySeconds the current adaptive output latency in seconds.
 * @property meanProcessingTimeMilliseconds recent mean chunk processing time in milliseconds, or
 *   `null` when no measurement is available yet.
 * @property refillCount number of refills observed in the window when the event fired.
 */
@Keep
data class LowPerformanceEvent(
    val sampleRateHz: Int,
    val currentLatencySeconds: Double,
    val meanProcessingTimeMilliseconds: Double?,
    val refillCount: Int,
)
