package com.gigaedge.denoiser

internal object Validation {
    private val modelNamePattern = Regex("[A-Za-z0-9._-]+")

    fun requireNumFrames(numFrames: Int) {
        require(numFrames >= 0) { "numFrames must be >= 0, was $numFrames" }
    }

    fun requireModelName(modelName: String) {
        require(modelName.isNotBlank()) { "modelName must not be blank" }
        require(modelNamePattern.matches(modelName)) {
            "modelName may contain only letters, digits, '.', '_', and '-'"
        }
    }

    fun requireOptionalModelName(modelName: String?) {
        if (modelName != null) {
            requireModelName(modelName)
        }
    }

    fun requireSampleRate(sampleRateHz: Int) {
        require(sampleRateHz > 0) { "sampleRateHz must be > 0, was $sampleRateHz" }
    }

    fun requireBufferSize(name: String, buffer: FloatArray, numFrames: Int) {
        require(buffer.size >= numFrames) {
            "$name size ${buffer.size} is smaller than numFrames $numFrames"
        }
    }
}
