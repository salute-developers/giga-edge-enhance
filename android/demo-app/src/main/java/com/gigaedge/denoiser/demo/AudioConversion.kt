package com.gigaedge.denoiser.demo

import kotlin.math.roundToInt

internal object AudioConversion {
    fun pcm16ToFloat(input: ShortArray, output: FloatArray, size: Int) {
        for (i in 0 until size) {
            output[i] = input[i] / 32768.0f
        }
    }

    fun floatToPcm16(input: FloatArray, output: ShortArray, size: Int) {
        for (i in 0 until size) {
            val sample = (input[i] * 32767.0f).roundToInt().coerceIn(-32768, 32767)
            output[i] = sample.toShort()
        }
    }
}
