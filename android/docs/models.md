# Models

Noise-suppression models are packaged and loaded as **assets**, and are **not** bundled in
`gigaedge-spench-sdk`.

## Packaging

`gigaedge-spench-sdk` ships no models. Each model is a separate Android library module (a model AAR) that
contributes a single packed asset. You add the models you need alongside the SDK:

```kotlin
dependencies {
    implementation(project(":gigaedge-spench-sdk"))
    implementation(project(":gigaedge-spench-model-quality48"))
    implementation(project(":gigaedge-spench-model-hybrid48"))
}
```

Only the model AARs you import are packaged into your app. If you import none, noise suppression has
nothing to load and `Models.available(context)` returns an empty list (normalization still
works).

## Asset layout

Each model module places its packed model under:

```text
spench/models/{name}.ges
```

For example, `:gigaedge-spench-model-quality48` contributes `spench/models/quality48.ges`. When your app
is built, this asset is merged into the app's assets alongside any other model modules you import.

## Available models

| Module | Asset | Name constant |
| --- | --- | --- |
| `:gigaedge-spench-model-quality48` | `spench/models/quality48.ges` | `com.gigaedge.spench.models.Quality48Model.NAME` (`"quality48"`) |
| `:gigaedge-spench-model-hybrid48` | `spench/models/hybrid48.ges` | `com.gigaedge.spench.models.Hybrid48Model.NAME` (`"hybrid48"`) |

Both models process audio at 48 kHz. `quality48` is discriminative; `hybrid48` is hybrid.

## Runtime discovery

Discover the models actually packaged in the running app with `Models.available(context)`. It
scans merged assets under `spench/models/`, keeps `.ges` files, strips the suffix, and returns the
names **sorted**:

```kotlin
import com.gigaedge.spench.Features
import com.gigaedge.spench.Models

val modelIds = Models.available(context) // e.g. ["hybrid48", "quality48"]
processor.setFeatures(Features(noiseSuppression = modelIds.firstOrNull()))
```

Runtime discovery keeps a model picker in sync with whatever modules are imported. If you prefer a
compile-time dependency on a specific model, use the name constant directly:

```kotlin
import com.gigaedge.spench.models.Quality48Model

processor.setFeatures(Features(noiseSuppression = Quality48Model.NAME))
```

Use `Hybrid48Model.NAME` from `com.gigaedge.spench.models` to select the hybrid model.

## Selecting a model

Pass the name (without the `.ges` suffix) as `Features.noiseSuppression`. The name may
contain only letters, digits, `.`, `_`, and `-`. Passing `null` disables noise suppression.

## Missing-model behavior

If you request a model whose asset is not packaged, `setFeatures(...)` throws
`IllegalArgumentException` with a clear "model asset not found" message. This happens when the
matching `:gigaedge-spench-model-*` module is not on the app's `implementation` classpath, or the name is
misspelled. Prefer selecting from `Models.available(context)` to avoid this. See
[troubleshooting.md](troubleshooting.md).
