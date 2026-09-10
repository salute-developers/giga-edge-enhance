# Models

Noise-suppression models are packaged and loaded as **assets**, and are **not** bundled in
`spench-sdk`.

## Packaging

`spench-sdk` ships no models. Each model is a separate Android library module (a model AAR) that
contributes a single packed asset. You add the models you need alongside the SDK:

```kotlin
dependencies {
    implementation(project(":spench-sdk"))
    implementation(project(":spench-model-quality48"))
}
```

Only the model AARs you import are packaged into your app. If you import none, noise suppression has
nothing to load and `SpenchModels.available(context)` returns an empty list (normalization still
works).

## Asset layout

Each model module places its packed model under:

```text
spench/models/{name}.spench
```

For example, `:spench-model-quality48` contributes `spench/models/quality48.spench`. When your app
is built, this asset is merged into the app's assets alongside any other model modules you import.

## Available models

| Module | Asset | Name constant |
| --- | --- | --- |
| `:spench-model-quality48` | `spench/models/quality48.spench` | `com.gigaedge.spench.models.SpenchQuality48Model.NAME` (`"quality48"`) |

## Runtime discovery

Discover the models actually packaged in the running app with `SpenchModels.available(context)`. It
scans merged assets under `spench/models/`, keeps `.spench` files, strips the suffix, and returns the
names **sorted**:

```kotlin
import com.gigaedge.spench.SpenchFeatures
import com.gigaedge.spench.SpenchModels

val modelIds = SpenchModels.available(context) // e.g. ["quality48"]
processor.setFeatures(SpenchFeatures(noiseSuppression = modelIds.firstOrNull()))
```

Runtime discovery keeps a model picker in sync with whatever modules are imported. If you prefer a
compile-time dependency on a specific model, use the name constant directly:

```kotlin
import com.gigaedge.spench.models.SpenchFull48kModel

processor.setFeatures(SpenchFeatures(noiseSuppression = SpenchFull48kModel.NAME))
```

## Selecting a model

Pass the name (without the `.spench` suffix) as `SpenchFeatures.noiseSuppression`. The name may
contain only letters, digits, `.`, `_`, and `-`. Passing `null` disables noise suppression.

## Missing-model behavior

If you request a model whose asset is not packaged, `setFeatures(...)` throws
`IllegalArgumentException` with a clear "model asset not found" message. This happens when the
matching `:spench-model-*` module is not on the app's `implementation` classpath, or the name is
misspelled. Prefer selecting from `SpenchModels.available(context)` to avoid this. See
[troubleshooting.md](troubleshooting.md).
