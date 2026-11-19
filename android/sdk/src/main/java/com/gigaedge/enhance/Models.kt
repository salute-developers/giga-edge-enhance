package com.gigaedge.enhance

import android.content.Context

/**
 * Runtime discovery of the noise-suppression models packaged into the app.
 *
 * Models are contributed by importing model AAR modules (for example `:gigaedge-enhance-model-quality48`); only
 * imported models are packaged. Each contributes an asset `enhance/models/{name}.enhance`.
 */
object Models {
    private const val MODEL_ASSET_DIR = "enhance/models"
    private const val MODEL_ASSET_SUFFIX = ".enhance"

    /**
     * Lists the names of models packaged in the app, sorted alphabetically.
     *
     * Discovery scans merged assets under `enhance/models/`, keeps entries ending in `.enhance`, and
     * strips that suffix. A returned name can be passed as [Features.noiseSuppression].
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
