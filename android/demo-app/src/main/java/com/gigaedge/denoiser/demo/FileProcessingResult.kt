package com.gigaedge.denoiser.demo

internal data class FileProcessingResult(
    val inputName: String,
    val outputName: String,
    val sampleRateHz: Int,
    val inputFramesProcessed: Int,
    val outputFramesWritten: Int,
)
