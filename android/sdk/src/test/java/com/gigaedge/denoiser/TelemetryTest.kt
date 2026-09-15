package com.gigaedge.denoiser

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class TelemetryTest {
    @Test
    fun emptyMeanReturnsNull() {
        val telemetry = Telemetry(emptyMap(), emptyMap())

        assertNull(telemetry.normalizationMeanMilliseconds)
        assertNull(telemetry.noiseSuppressionMeanMilliseconds)
    }

    @Test
    fun weightedMeanIsCorrect() {
        val telemetry = Telemetry(
            normalizationCounters = mapOf(1.0 to 1, 3.0 to 3),
            noiseSuppressionCounters = mapOf(2.0 to 2, 4.0 to 2),
        )

        assertEquals(2.5, telemetry.normalizationMeanMilliseconds!!, 1.0e-9)
        assertEquals(3.0, telemetry.noiseSuppressionMeanMilliseconds!!, 1.0e-9)
    }

    @Test
    fun weightedMeanDoesNotOverflowLargeCounts() {
        val telemetry = Telemetry(
            normalizationCounters = mapOf(1.0 to Int.MAX_VALUE, 3.0 to Int.MAX_VALUE),
            noiseSuppressionCounters = emptyMap(),
        )

        assertEquals(2.0, telemetry.normalizationMeanMilliseconds!!, 1.0e-9)
    }

    @Test
    fun subtractionKeepsPositiveDeltasOnly() {
        val current = Telemetry(
            normalizationCounters = mapOf(1.0 to 4, 2.0 to 2, 3.0 to 1),
            noiseSuppressionCounters = mapOf(1.0 to 1),
        )
        val previous = Telemetry(
            normalizationCounters = mapOf(1.0 to 1, 2.0 to 2, 3.0 to 4),
            noiseSuppressionCounters = mapOf(1.0 to 1),
        )

        val delta = current - previous

        assertEquals(mapOf(1.0 to 3), delta.normalizationCounters)
        assertEquals(emptyMap<Double, Int>(), delta.noiseSuppressionCounters)
    }

    @Test
    fun subtractionDoesNotOverflowUntrustedSnapshots() {
        val current = Telemetry(mapOf(1.0 to Int.MAX_VALUE), emptyMap())
        val previous = Telemetry(mapOf(1.0 to Int.MIN_VALUE), emptyMap())

        assertEquals(mapOf(1.0 to Int.MAX_VALUE), (current - previous).normalizationCounters)
    }
}
