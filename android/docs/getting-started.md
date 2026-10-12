# Getting started

This guide gets the GigaEdge Spench Android Audio SDK running in your app.

## What the SDK does

The SDK processes **mono `FloatArray` PCM** audio in real time through two optional feature stages:

- **Normalization** — loudness control, noise gate, and limiter.
- **Noise suppression** — a neural denoiser loaded from a packed model asset.

You give it the input sample rate at construction, push chunks through `process(...)`, and read
cumulative timing telemetry. Audio capture, playback, file I/O, and format conversion stay in your
app; the SDK only does DSP.

## Prerequisites

- **JDK 17**
- **Android SDK** with **API 35** (`compileSdk = 35`, `targetSdk = 35` for the demo). Minimum
  supported device API is **26**.
- **Android NDK 29.0.14206865** and **CMake 3.22.1** (installed via Android Studio / SDK Manager).
  These are only required to build the SDK's native library from source.
- At least one emulator or physical device to run instrumented tests.

The native library is built for the ABIs `arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86`.

## Adding the SDK

The SDK and models are consumed as **project modules** (they are not published to Maven Central).
Add the SDK plus every model package your app needs to your app module's `build.gradle.kts`:

```kotlin
dependencies {
    implementation(project(":gigaedge-spench-sdk"))
    implementation(project(":gigaedge-spench-model-quality48"))
}
```

Only the model AARs you import are packaged into your app. See [Models](models.md) for details on
packaging and runtime discovery.

## Minimal end-to-end example

Create a processor, enable features, process a chunk, and close it. `Processor` is
`Closeable`, so `use { }` guarantees native resources are released.

```kotlin
import com.gigaedge.spench.Features
import com.gigaedge.spench.Processor

fun processOneChunk(context: Context, sampleRateHz: Int, input: FloatArray) {
    Processor(context, sampleRateHz).use { processor ->
        processor.setFeatures(Features(normalization = true, noiseSuppression = "quality48"))
        val output = FloatArray(input.size)
        processor.process(input, output, input.size)
        // `output` holds processed samples; early samples may be zero until the worker warms up.
    }
}
```

Because the native worker runs asynchronously, the first few `process(...)` calls may return
zero-filled output before processed audio is available. This is expected — see
[Architecture](architecture.md) and [Threading](threading.md).

## Reading a telemetry delta

Telemetry is cumulative for the processor's lifetime. Subtract an earlier snapshot to measure a
single interval:

```kotlin
val before = processor.telemetry
processor.process(input, output, input.size)
val delta = processor.telemetry - before
val normalizationMeanMs = delta.normalizationMeanMilliseconds
```

See [Telemetry and performance](telemetry-and-performance.md).

## Discovering models at runtime

```kotlin
import com.gigaedge.spench.Models

val modelIds = Models.available(context) // sorted, e.g. ["quality48"]
processor.setFeatures(Features(noiseSuppression = modelIds.firstOrNull()))
```

## First-run checklist

1. The SDK and at least one `:gigaedge-spench-model-*` module are on your app's `implementation` classpath.
2. You construct `Processor` with the **actual input sample rate** in Hz (`> 0`).
3. You call `setFeatures(...)` — a fresh processor has all stages **disabled**.
4. Noise-suppression names come from [`Models.available(context)`](models.md) (or the
   `Quality48Model.NAME` constant).
5. Input/output buffers are at least `numFrames` long, and audio is **mono**.
6. You `close()` the processor (or use `use { }`) when done.
7. No manual ProGuard/R8 rules are needed; the SDK ships [consumer rules](proguard-r8.md).

## Building from the command line

Use the Gradle wrapper for reproducible builds:

```bash
./gradlew :gigaedge-spench-sdk:assembleDebug
./gradlew :demo-app:assembleDebug
```

Build everything (SDK, models, demo):

```bash
./scripts/build.sh
```

## Next steps

- [API reference](api-reference.md)
- [Architecture](architecture.md)
- [Threading](threading.md)
- [Demo app](demo-app.md)
