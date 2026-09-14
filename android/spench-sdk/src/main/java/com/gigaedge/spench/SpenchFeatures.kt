package com.gigaedge.spench

/**
 * Selection of feature stages to run, passed to [SpenchProcessor.setFeatures].
 *
 * The default disables both stages, matching a freshly constructed [SpenchProcessor].
 *
 * @property normalization enables the normalization stage (loudness control, noise gate, limiter).
 * @property noiseSuppression name of the packed model to use for noise suppression, or `null` to
 *   disable it. The name must resolve to a merged asset `spench/models/{name}.spench`; discover
 *   available names with [SpenchModels.available]. May contain only letters, digits, `.`, `_` and
 *   `-`.
 */
data class SpenchFeatures(
    val normalization: Boolean = false,
    val noiseSuppression: String? = null,
)
