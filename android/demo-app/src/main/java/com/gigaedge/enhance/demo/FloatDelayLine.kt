package com.gigaedge.enhance.demo

internal class FloatDelayLine(delayFrames: Int) {
    private val buffer = FloatArray(delayFrames.coerceAtLeast(1))
    private var writeIndex = 0

    val delayFrames: Int = buffer.size

    fun process(input: FloatArray, output: FloatArray, numFrames: Int) {
        for (i in 0 until numFrames) {
            output[i] = buffer[writeIndex]
            buffer[writeIndex] = input[i]
            writeIndex++
            if (writeIndex == buffer.size) {
                writeIndex = 0
            }
        }
    }
}
