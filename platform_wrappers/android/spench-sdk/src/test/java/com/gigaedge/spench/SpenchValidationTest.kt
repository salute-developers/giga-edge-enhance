package com.gigaedge.spench

import org.junit.Test

class SpenchValidationTest {
    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsShortInputBuffer() {
        SpenchValidation.requireBufferSize("input", FloatArray(1), 2)
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsNegativeFrameCount() {
        SpenchValidation.requireNumFrames(-1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsPathLikeModelName() {
        SpenchValidation.requireModelName("../quality48")
    }

    @Test
    fun validationAllowsNullOptionalModelName() {
        SpenchValidation.requireOptionalModelName(null)
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsInvalidOptionalModelName() {
        SpenchValidation.requireOptionalModelName("../quality48")
    }

    @Test(expected = IllegalArgumentException::class)
    fun validationRejectsInvalidSampleRate() {
        SpenchValidation.requireSampleRate(0)
    }
}
