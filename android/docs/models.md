# Models

Noise-suppression models are packaged and loaded as **assets**, and are **not** bundled in
`giga-denoiser-sdk`.

## Packaging

`giga-denoiser-sdk` ships no models. Each model is a separate Android library module (a model AAR) that
contributes a single packed asset. You add the models you need alongside the SDK:

```kotlin
dependencies {
    implementation(project(":giga-denoiser-sdk"))
    implementation(project(":giga-denoiser-model-quality48"))
}
```

Only the model AARs you import are packaged into your app. If you import none, noise suppression has
nothing to load and `Models.available(context)` returns an empty list (normalization still
works).

## Asset layout

Each model module places its packed model under:

```text
gigadenoiser/models/{name}.gdn
```

For example, `:giga-denoiser-model-quality48` contributes `gigadenoiser/models/quality48.gdn`. When your app
is built, this asset is merged into the app's assets alongside any other model modules you import.

## Available models

| Module | Asset | Name constant |
| --- | --- | --- |
| `:giga-denoiser-model-quality48` | `gigadenoiser/models/quality48.gdn` | `com.gigaedge.denoiser.models.Quality48Model.NAME` (`"quality48"`) |

## Runtime discovery

Discover the models actually packaged in the running app with `Models.available(context)`. It
scans merged assets under `gigadenoiser/models/`, keeps `.gdn` files, strips the suffix, and returns the
names **sorted**:

```kotlin
import com.gigaedge.denoiser.Features
import com.gigaedge.denoiser.Models

val modelIds = Models.available(context) // e.g. ["quality48"]
processor.setFeatures(Features(noiseSuppression = modelIds.firstOrNull()))
```

Runtime discovery keeps a model picker in sync with whatever modules are imported. If you prefer a
compile-time dependency on a specific model, use the name constant directly:

```kotlin
import com.gigaedge.denoiser.models.Quality48Model

processor.setFeatures(Features(noiseSuppression = Quality48Model.NAME))
```

## Selecting a model

Pass the name (without the `.gdn` suffix) as `Features.noiseSuppression`. The name may
contain only letters, digits, `.`, `_`, and `-`. Passing `null` disables noise suppression.

## Missing-model behavior

If you request a model whose asset is not packaged, `setFeatures(...)` throws
`IllegalArgumentException` with a clear "model asset not found" message. This happens when the
matching `:giga-denoiser-model-*` module is not on the app's `implementation` classpath, or the name is
misspelled. Prefer selecting from `Models.available(context)` to avoid this. See
[troubleshooting.md](troubleshooting.md).
