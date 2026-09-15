package com.gigaedge.denoiser

import org.junit.Test

class SettingsTest {
    @Test
    fun defaultsAreValid() {
        Settings()
    }

    @Test
    fun copyValidatesInvariants() {
        val defaults = Settings()
        try {
            defaults.copy(minLatency = -1.0)
            throw AssertionError("copy with invalid value should throw")
        } catch (expected: IllegalArgumentException) {
            // expected: copy() runs the same init validation
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidWindow() {
        Settings(lowPerformanceWindowSeconds = 0.0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMaxRefillTimes() {
        Settings(lowPerformanceMaxRefillTimes = 0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMaxRefillFactor() {
        Settings(lowPerformanceMaxRefillTimeFactor = Double.POSITIVE_INFINITY)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMinLatency() {
        Settings(minLatency = Double.NaN)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidLatencyIncreaseStep() {
        Settings(latencyIncreaseStep = -0.1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMaxProcessingChunkSeconds() {
        Settings(maxProcessingChunkSeconds = Double.POSITIVE_INFINITY)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsZeroTelemetryRounding() {
        Settings(telemetryRoundingMilliseconds = 0.0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsNanTelemetryRounding() {
        Settings(telemetryRoundingMilliseconds = Double.NaN)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInfiniteTelemetryRounding() {
        Settings(telemetryRoundingMilliseconds = Double.POSITIVE_INFINITY)
    }
}
