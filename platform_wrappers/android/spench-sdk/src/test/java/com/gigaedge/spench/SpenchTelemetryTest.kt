package com.gigaedge.spench

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SpenchTelemetryTest {
    @Test
    fun emptyMeanReturnsNull() {
        val telemetry = SpenchTelemetry(emptyMap(), emptyMap())

        assertNull(telemetry.normalizationMeanMilliseconds)
        assertNull(telemetry.noiseSuppressionMeanMilliseconds)
    }

    @Test
    fun weightedMeanIsCorrect() {
        val telemetry = SpenchTelemetry(
            normalizationCounters = mapOf(1.0 to 1, 3.0 to 3),
            noiseSuppressionCounters = mapOf(2.0 to 2, 4.0 to 2),
        )

        assertEquals(2.5, telemetry.normalizationMeanMilliseconds!!, 1.0e-9)
        assertEquals(3.0, telemetry.noiseSuppressionMeanMilliseconds!!, 1.0e-9)
    }

    @Test
    fun subtractionKeepsPositiveDeltasOnly() {
        val current = SpenchTelemetry(
            normalizationCounters = mapOf(1.0 to 4, 2.0 to 2, 3.0 to 1),
            noiseSuppressionCounters = mapOf(1.0 to 1),
        )
        val previous = SpenchTelemetry(
            normalizationCounters = mapOf(1.0 to 1, 2.0 to 2, 3.0 to 4),
            noiseSuppressionCounters = mapOf(1.0 to 1),
        )

        val delta = current - previous

        assertEquals(mapOf(1.0 to 3), delta.normalizationCounters)
        assertEquals(emptyMap<Double, Int>(), delta.noiseSuppressionCounters)
    }
}
