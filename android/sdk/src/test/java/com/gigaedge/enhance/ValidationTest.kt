package com.gigaedge.enhance

import org.junit.Test

class ValidationTest {
    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsShortInputBuffer() {
        Validation.requireBufferSize("input", FloatArray(1), 2)
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsNegativeFrameCount() {
        Validation.requireNumFrames(-1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsPathLikeModelName() {
        Validation.requireModelName("../quality48")
    }

    @Test
    fun validationAllowsNullOptionalModelName() {
        Validation.requireOptionalModelName(null)
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsInvalidOptionalModelName() {
        Validation.requireOptionalModelName("../quality48")
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsInvalidSampleRate() {
        Validation.requireSampleRate(0)
    }
}
