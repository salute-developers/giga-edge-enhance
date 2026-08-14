package com.gigaedge.spench

import android.content.Context
import android.content.res.AssetManager
import java.io.Closeable
import java.util.concurrent.Executor
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.locks.ReentrantReadWriteLock
import kotlin.concurrent.read
import kotlin.concurrent.write

/**
 * Real-time mono audio processor backed by the native Spench C++ core.
 *
 * A processor owns exactly one native instance and a native worker thread. [process] moves mono
 * `Float` PCM through lock-free ring buffers and never blocks on DSP; the worker runs the enabled
 * feature stages (normalization and/or noise suppression) on chunks selected by
 * [SpenchSettings.maxProcessingChunkSeconds]. Output the worker has not produced yet is zero-filled
 * instead of blocking the caller.
 *
 * All processing features start disabled; enable them at runtime with [setFeatures]. The instance
 * is [Closeable] and must be closed to release native resources.
 *
 * Threading: [process], [reset], [setFeatures] and reading [telemetry] may be called concurrently
 * from different threads and do not serialize against each other. [process] is intended to be
 * driven from a single audio/capture thread. [close] is the only exclusive operation; it waits for
 * in-flight calls to finish, then destroys the native processor. Using the processor after [close]
 * throws [IllegalStateException].
 *
 * @param context any [Context]; the application context and its [android.content.res.AssetManager]
 *   are retained for loading noise-suppression model assets.
 * @param sampleRateHz input sample rate in Hz; must be greater than zero. Throws
 *   [IllegalArgumentException] otherwise.
 * @param callbackExecutor optional executor used to dispatch [onLowPerformance] callbacks. When
 *   `null`, callbacks run synchronously on the [process] caller thread; when supplied, each event
 *   is delivered via `callbackExecutor.execute { ... }`.
 * @property settings the immutable configuration captured at construction; fixed for the lifetime
 *   of the processor.
 */
class SpenchProcessor(
    context: Context,
    sampleRateHz: Int,
    val settings: SpenchSettings = SpenchSettings(),
    private val callbackExecutor: Executor? = null,
) : Closeable {
    private val closed = AtomicBoolean(false)

    // process/reset/setFeatures/telemetry hold the read lock, so they run concurrently with each other
    // (the native layer is internally thread-safe) and never serialize the audio thread against a
    // telemetry or feature call. Only close() takes the write lock, guaranteeing no native call is
    // in flight when the native processor is destroyed.
    private val lifecycleLock = ReentrantReadWriteLock()
    private val assetManager: AssetManager = context.applicationContext.assets

    /**
     * Optional listener for low-performance (adaptive-latency) events.
     *
     * Events are queued natively and delivered from [process] after each call drains them. With
     * `callbackExecutor = null` the listener runs synchronously on the [process] caller thread;
     * otherwise it runs via the supplied executor. Native code never invokes this listener directly.
     * Assignable at any time; the field is volatile.
     */
    @Volatile
    var onLowPerformance: ((SpenchLowPerformanceEvent) -> Unit)? = null

    /**
     * A cumulative snapshot of processing-time histograms for the enabled feature stages.
     *
     * Buckets and means are in milliseconds. Telemetry accumulates for the lifetime of the
     * processor and is NOT cleared by [reset]; compute a delta between two reads with
     * [SpenchTelemetry.minus]. Reading this property never waits for an in-progress processing
     * chunk, so it is safe to poll from a UI or background thread.
     *
     * @throws IllegalStateException if the processor has been closed.
     */
    val telemetry: SpenchTelemetry
        get() = withOpenHandle { handle -> nativeGetTelemetry(handle).toTelemetry() }

    @Volatile
    private var nativeHandle: Long = 0L

    init {
        SpenchValidation.requireSampleRate(sampleRateHz)
        nativeHandle = nativeCreate(
            sampleRateHz,
            settings.lowPerformanceWindowSeconds,
            settings.lowPerformanceMaxRefillTimes,
            settings.lowPerformanceMaxRefillTimeFactor,
            settings.minLatency,
            settings.latencyIncreaseStep,
            settings.telemetryRoundingMilliseconds,
            settings.useVadNormalization,
            settings.unstableReverseProcessingOrder,
            settings.unstableCustomNormalizationConfig,
            settings.maxProcessingChunkSeconds,
        )
        check(nativeHandle != 0L) { "Failed to create native processor" }
    }

    /**
     * Processes one chunk of mono audio.
     *
     * The first `numFrames` samples of [input] are enqueued for the worker, and up to `numFrames`
     * already-processed samples are written into [output]. Any samples the worker has not produced
     * yet are zero-filled. This call is non-blocking with respect to DSP and is intended to be
     * driven from a single audio/capture thread. After forwarding samples it delivers any pending
     * [onLowPerformance] events.
     *
     * A `numFrames` of 0 is a no-op. Buffers may be larger than `numFrames`; only the first
     * `numFrames` elements are read/written.
     *
     * @param input mono `Float` PCM samples, size at least `numFrames`.
     * @param output destination buffer, size at least `numFrames`.
     * @param numFrames number of frames to process; must be `>= 0`.
     * @throws IllegalArgumentException if `numFrames < 0` or either buffer is smaller than
     *   `numFrames`.
     * @throws IllegalStateException if the processor has been closed.
     */
    fun process(input: FloatArray, output: FloatArray, numFrames: Int) {
        SpenchValidation.requireNumFrames(numFrames)
        SpenchValidation.requireBufferSize("input", input, numFrames)
        SpenchValidation.requireBufferSize("output", output, numFrames)
        if (numFrames == 0) {
            return
        }
        val events = withOpenHandle { handle ->
            nativeProcess(handle, input, output, numFrames)
            nativeDrainLowPerformanceEvents(handle)
        }
        deliverLowPerformanceEvents(events)
    }

    /**
     * Clears buffered audio and internal streaming state so the next [process] call starts fresh.
     *
     * Enabled feature stages are rebuilt but the current feature selection is preserved. Cumulative
     * [telemetry] is NOT cleared. Safe to call concurrently with [process].
     *
     * @throws IllegalStateException if the processor has been closed.
     */
    fun reset() {
        withOpenHandle { handle ->
            nativeReset(handle)
        }
    }

    /**
     * Enables or disables the feature stages at runtime.
     *
     * Only stages whose configuration actually changed are rebuilt. Enabling noise suppression
     * loads the packed model asset `spench/models/{name}.spench` from the app's merged assets.
     * Safe to call concurrently with [process].
     *
     * @param features desired stage configuration. A `null`
     *   [SpenchFeatures.noiseSuppression] disables noise suppression.
     * @throws IllegalArgumentException if the model name is invalid, or if the requested model
     *   asset cannot be found or read.
     * @throws IllegalStateException if the processor has been closed.
     */
    fun setFeatures(features: SpenchFeatures) {
        SpenchValidation.requireOptionalModelName(features.noiseSuppression)
        withOpenHandle { handle ->
            nativeSetFeatures(handle, assetManager, features.normalization, features.noiseSuppression)
        }
    }

    /**
     * Releases the native processor and stops its worker thread.
     *
     * This is the only exclusive operation: it waits for any in-flight [process], [reset],
     * [setFeatures] or [telemetry] call to finish before destroying native state. It is idempotent;
     * calling it more than once is safe. After close, any further use throws [IllegalStateException].
     */
    override fun close() {
        if (!closed.compareAndSet(false, true)) {
            return
        }
        lifecycleLock.write {
            val handle = nativeHandle
            nativeHandle = 0L
            if (handle != 0L) {
                nativeDestroy(handle)
            }
        }
    }

    private inline fun <T> withOpenHandle(block: (Long) -> T): T = lifecycleLock.read {
        check(!closed.get()) { "SpenchProcessor is already closed" }
        val handle = nativeHandle
        check(handle != 0L) { "Native processor is unavailable" }
        block(handle)
    }

    private fun deliverLowPerformanceEvents(events: Array<SpenchLowPerformanceEvent>) {
        for (event in events) {
            val callback = onLowPerformance ?: continue
            val executor = callbackExecutor
            if (executor == null) {
                callback(event)
            } else {
                executor.execute { callback(event) }
            }
        }
    }

    private fun SpenchNativeTelemetry.toTelemetry(): SpenchTelemetry = SpenchTelemetry(
        normalizationCounters = normalizationBuckets.toCounterMap(normalizationCounts),
        noiseSuppressionCounters = noiseSuppressionBuckets.toCounterMap(noiseSuppressionCounts),
    )

    private fun DoubleArray.toCounterMap(counts: IntArray): Map<Double, Int> {
        require(size == counts.size) { "Native telemetry bucket/count length mismatch" }
        return indices.associate { index -> this[index] to counts[index] }
    }

    @Suppress("LongParameterList")
    private external fun nativeCreate(
        sampleRateHz: Int,
        lowPerformanceWindowSeconds: Double,
        lowPerformanceMaxRefillTimes: Int,
        lowPerformanceMaxRefillTimeFactor: Double,
        minLatency: Double,
        latencyIncreaseStep: Double,
        telemetryRoundingMilliseconds: Double,
        useVadNormalization: Boolean,
        unstableReverseProcessingOrder: Boolean,
        unstableCustomNormalizationConfig: String?,
        maxProcessingChunkSeconds: Double,
    ): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeProcess(handle: Long, input: FloatArray, output: FloatArray, numFrames: Int)
    private external fun nativeReset(handle: Long)
    private external fun nativeGetTelemetry(handle: Long): SpenchNativeTelemetry
    private external fun nativeDrainLowPerformanceEvents(handle: Long): Array<SpenchLowPerformanceEvent>
    private external fun nativeSetFeatures(
        handle: Long,
        assetManager: AssetManager,
        normalization: Boolean,
        noiseSuppression: String?,
    )

    companion object {
        init {
            System.loadLibrary("spench-sdk")
        }
    }
}
