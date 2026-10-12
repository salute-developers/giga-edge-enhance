package com.gigaedge.spench.demo

import android.content.Context
import com.gigaedge.spench.Settings

internal object SettingsStore {
    private const val PREFS_NAME = "gigaedge-spench-demo-settings"
    private const val LOW_PERFORMANCE_WINDOW_SECONDS = "lowPerformanceWindowSeconds"
    private const val LOW_PERFORMANCE_MAX_REFILL_TIMES = "lowPerformanceMaxRefillTimes"
    private const val LOW_PERFORMANCE_MAX_REFILL_TIME_FACTOR = "lowPerformanceMaxRefillTimeFactor"
    private const val MIN_LATENCY = "minLatency"
    private const val LATENCY_INCREASE_STEP = "latencyIncreaseStep"
    private const val MAX_PROCESSING_CHUNK_SECONDS = "maxProcessingChunkSeconds"
    private const val TELEMETRY_ROUNDING_MILLISECONDS = "telemetryRoundingMilliseconds"
    private const val USE_VAD_NORMALIZATION = "useVadNormalization"
    private const val REVERSE_PROCESSING_ORDER = "reverseProcessingOrder"
    private const val CUSTOM_NORMALIZATION_CONFIG = "customNormalizationConfig"

    fun load(context: Context): Settings {
        val defaults = Settings()
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return Settings(
            lowPerformanceWindowSeconds = prefs.getString(
                LOW_PERFORMANCE_WINDOW_SECONDS,
                defaults.lowPerformanceWindowSeconds.toString(),
            )!!.toDouble(),
            lowPerformanceMaxRefillTimes = prefs.getInt(
                LOW_PERFORMANCE_MAX_REFILL_TIMES,
                defaults.lowPerformanceMaxRefillTimes,
            ),
            lowPerformanceMaxRefillTimeFactor = prefs.getString(
                LOW_PERFORMANCE_MAX_REFILL_TIME_FACTOR,
                defaults.lowPerformanceMaxRefillTimeFactor.toString(),
            )!!.toDouble(),
            minLatency = prefs.getString(MIN_LATENCY, defaults.minLatency.toString())!!.toDouble(),
            latencyIncreaseStep = prefs.getString(
                LATENCY_INCREASE_STEP,
                defaults.latencyIncreaseStep.toString(),
            )!!.toDouble(),
            maxProcessingChunkSeconds = prefs.getString(
                MAX_PROCESSING_CHUNK_SECONDS,
                defaults.maxProcessingChunkSeconds.toString(),
            )!!.toDouble(),
            telemetryRoundingMilliseconds = prefs.getString(
                TELEMETRY_ROUNDING_MILLISECONDS,
                defaults.telemetryRoundingMilliseconds.toString(),
            )!!.toDouble(),
            useVadNormalization = prefs.getBoolean(USE_VAD_NORMALIZATION, defaults.useVadNormalization),
            unstableReverseProcessingOrder = prefs.getBoolean(
                REVERSE_PROCESSING_ORDER,
                defaults.unstableReverseProcessingOrder,
            ),
            unstableCustomNormalizationConfig = prefs.getString(CUSTOM_NORMALIZATION_CONFIG, null)?.ifBlank { null },
        )
    }

    // Settings validity is enforced by Settings itself, so constructing the instance above (and
    // in SettingsActivity) is the single validation point; no duplicate rules live here.
    fun save(context: Context, settings: Settings) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit()
            .putString(LOW_PERFORMANCE_WINDOW_SECONDS, settings.lowPerformanceWindowSeconds.toString())
            .putInt(LOW_PERFORMANCE_MAX_REFILL_TIMES, settings.lowPerformanceMaxRefillTimes)
            .putString(LOW_PERFORMANCE_MAX_REFILL_TIME_FACTOR, settings.lowPerformanceMaxRefillTimeFactor.toString())
            .putString(MIN_LATENCY, settings.minLatency.toString())
            .putString(LATENCY_INCREASE_STEP, settings.latencyIncreaseStep.toString())
            .putString(MAX_PROCESSING_CHUNK_SECONDS, settings.maxProcessingChunkSeconds.toString())
            .putString(TELEMETRY_ROUNDING_MILLISECONDS, settings.telemetryRoundingMilliseconds.toString())
            .putBoolean(USE_VAD_NORMALIZATION, settings.useVadNormalization)
            .putBoolean(REVERSE_PROCESSING_ORDER, settings.unstableReverseProcessingOrder)
            .putString(CUSTOM_NORMALIZATION_CONFIG, settings.unstableCustomNormalizationConfig)
            .apply()
    }
}
