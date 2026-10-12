# GigaEdge Spench Android Audio SDK

GigaEdge Spench is an Android audio SDK for real-time speech processing. It wraps a native GigaEdge Spench C++ core
behind a small Kotlin API: you feed it **mono `FloatArray` PCM**, and it runs optional
**normalization** (loudness control, noise gate, limiter) and **neural noise-suppression** stages on
a background worker thread. Audio capture, playback, and file I/O stay in your app — the SDK does the
DSP.

## Key capabilities

- Real-time, non-blocking `process(...)` for mono `FloatArray` PCM.
- Input sample rate fixed at construction; the native core resamples internally as needed.
- Two runtime-toggleable feature stages: normalization and noise suppression.
- Noise-suppression models imported as separate AARs and discovered at runtime.
- Cumulative timing telemetry and a low-performance / adaptive-latency signal.
- Explicit `Closeable` lifecycle; one native instance per processor.

## Quickstart

```kotlin
import com.gigaedge.spench.Features
import com.gigaedge.spench.Processor
import com.gigaedge.spench.Models

// sampleRateHz is your input rate in Hz; audio must be mono.
Processor(context, sampleRateHz).use { processor ->
    val model = Models.available(context).firstOrNull() // e.g. "quality48", or null
    processor.setFeatures(Features(normalization = true, noiseSuppression = model))

    val output = FloatArray(input.size)
    processor.process(input, output, input.size)       // non-blocking; warms up asynchronously

    val telemetry = processor.telemetry                 // cumulative; subtract snapshots for deltas
    processor.reset()                                   // clears buffers; keeps features + telemetry
}                                                       // close() releases native resources
```

Add the SDK and the model modules to your app's dependencies:

```kotlin
dependencies {
    implementation(project(":gigaedge-spench-sdk"))
    implementation(project(":gigaedge-spench-model-quality48"))
}
```

Only imported model AARs are packaged into your app. The SDK is consumed as a project module (not
published to Maven Central).

## Documentation

Full guides live in [`docs/`](docs/README.md):

- [Getting started](docs/getting-started.md) — prerequisites, dependencies, end-to-end example,
  first-run checklist.
- [API reference](docs/api-reference.md) — every public type and member.
- [Architecture](docs/architecture.md) — Kotlin API → thin JNI → native worker and ring buffers.
- [Threading](docs/threading.md) — the concurrency contract and callback delivery.
- [Telemetry and performance](docs/telemetry-and-performance.md) — histograms, deltas, means,
  low-performance events.
- [Models](docs/models.md) — packaging, asset layout, runtime discovery, missing-model behavior.
- [ProGuard / R8](docs/proguard-r8.md) — the consumer keep rules the SDK ships.
- [Demo app](docs/demo-app.md) — live monitor, file processing, settings, emulator mic caveats.
- [Troubleshooting](docs/troubleshooting.md) — common errors and fixes.

## Building and testing

Requires JDK 17, Android SDK API 35, and (to build the native library) NDK 29.0.14206865 with CMake
3.22.1. Use the Gradle wrapper:

```bash
./gradlew :gigaedge-spench-sdk:assembleDebug
./scripts/build.sh          # SDK + models + demo
```

Helper scripts:

- `scripts/build.sh` — Release assemble of the SDK, model modules, and demo app.
- `scripts/test-sdk.sh` — SDK JVM unit tests and SDK instrumented tests for an already-built Debug variant.
- `scripts/test-demo-ui.sh` — demo app connected instrumentation/UI tests.
- `scripts/test-all.sh` — Release JVM tests and optional connected Android tests for an already-built variant.

## Known limitations

- Mono `FloatArray` processing only.
- The demo file path supports only mono 16-bit PCM WAV input and writes WAV output.
- The demo live path is intentionally simple (no foreground service, minimal error recovery) and is
  not a production monitoring path.
- Android emulator microphone input can be unreliable or tonal; see
  [Demo app](docs/demo-app.md#emulator-microphone-caveats) and
  [Troubleshooting](docs/troubleshooting.md).
