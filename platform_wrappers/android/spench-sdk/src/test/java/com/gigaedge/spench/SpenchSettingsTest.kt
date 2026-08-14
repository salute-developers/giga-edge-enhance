package com.gigaedge.spench

import org.junit.Test

class SpenchSettingsTest {
    @Test
    fun defaultsAreValid() {
        SpenchSettings()
    }

    @Test
    fun copyValidatesInvariants() {
        val defaults = SpenchSettings()
        try {
            defaults.copy(minLatency = -1.0)
            throw AssertionError("copy with invalid value should throw")
        } catch (expected: IllegalArgumentException) {
            // expected: copy() runs the same init validation
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidWindow() {
        SpenchSettings(lowPerformanceWindowSeconds = 0.0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMaxRefillTimes() {
        SpenchSettings(lowPerformanceMaxRefillTimes = 0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMaxRefillFactor() {
        SpenchSettings(lowPerformanceMaxRefillTimeFactor = Double.POSITIVE_INFINITY)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMinLatency() {
        SpenchSettings(minLatency = Double.NaN)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidLatencyIncreaseStep() {
        SpenchSettings(latencyIncreaseStep = -0.1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInvalidMaxProcessingChunkSeconds() {
        SpenchSettings(maxProcessingChunkSeconds = Double.POSITIVE_INFINITY)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsZeroTelemetryRounding() {
        SpenchSettings(telemetryRoundingMilliseconds = 0.0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsNanTelemetryRounding() {
        SpenchSettings(telemetryRoundingMilliseconds = Double.NaN)
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsInfiniteTelemetryRounding() {
        SpenchSettings(telemetryRoundingMilliseconds = Double.POSITIVE_INFINITY)
    }
}
