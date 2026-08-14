package com.gigaedge.spench

import android.content.Context

/**
 * Runtime discovery of the noise-suppression models packaged into the app.
 *
 * Models are contributed by importing model AAR modules (for example `:spench-model-full24k`); only
 * imported models are packaged. Each contributes an asset `spench/models/{name}.spench`.
 */
object SpenchModels {
    private const val MODEL_ASSET_DIR = "spench/models"
    private const val MODEL_ASSET_SUFFIX = ".spench"

    /**
     * Lists the names of models packaged in the app, sorted alphabetically.
     *
     * Discovery scans merged assets under `spench/models/`, keeps entries ending in `.spench`, and
     * strips that suffix. A returned name can be passed as [SpenchFeatures.noiseSuppression].
     *
     * @param context any [Context]; the application context is used to read assets.
     * @return sorted model names, or an empty list if no model assets are present.
     */
    fun available(context: Context): List<String> {
        val assetManager = context.applicationContext.assets
        val assets = assetManager.list(MODEL_ASSET_DIR) ?: return emptyList()
        return assets
            .asSequence()
            .filter { it.endsWith(MODEL_ASSET_SUFFIX) }
            .map { it.removeSuffix(MODEL_ASSET_SUFFIX) }
            .filter { it.isNotBlank() }
            .sorted()
            .toList()
    }
}
