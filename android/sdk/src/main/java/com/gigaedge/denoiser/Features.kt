package com.gigaedge.denoiser

/**
 * Selection of feature stages to run, passed to [Processor.setFeatures].
 *
 * The default disables both stages, matching a freshly constructed [Processor].
 *
 * @property normalization enables the normalization stage (loudness control, noise gate, limiter).
 * @property noiseSuppression name of the packed model to use for noise suppression, or `null` to
 *   disable it. The name must resolve to a merged asset `gigadenoiser/models/{name}.gdn`; discover
 *   available names with [Models.available]. May contain only letters, digits, `.`, `_` and
 *   `-`.
 */
data class Features(
    val normalization: Boolean = false,
    val noiseSuppression: String? = null,
)
