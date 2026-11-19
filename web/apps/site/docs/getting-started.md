---
sidebar_position: 2
title: Getting started
---

# Getting started

Use this page to add GigaEdge Enhance to an application. Repository setup, Docker, and the full test
matrix are in [Build and test](development/build-and-test.md).

## Choose a platform

| Goal | Guide | Runtime contract |
| --- | --- | --- |
| Native application or C ABI | [C and C++ quickstart](core/quickstart.md) | Mono float PCM through `Denoiser` or the C API |
| Browser | [Web SDK reference](/docs/source/web/packages/web-sdk/) | `AudioContext`, worker, and packed `.enhance` models |
| Android | [Android SDK](/docs/source/android/) | Non-blocking mono `FloatArray` processing |
| iOS | [iOS Quick Start](/docs/source/ios/Sources/EnhanceAudioKit/Documentation.docc/QuickStart/) | `AVAudioEngine` or direct mono buffers |

Feature availability differs by SDK. See the [capability matrix](capabilities.md)
before enabling echo cancellation or selecting a model.

## Use a release

Download the archive for one version from the
[GitHub Releases](https://github.com/salute-developers/gigadenoiser/releases) page. Releases are the
publication channel for native packages, Android and iOS artifacts, WASM, and the Web SDK. The Web
SDK package name is `@gigaedge/enhance-web-sdk`; it is not published to npm.

After unpacking a native archive, point CMake at its package config:

```cmake
find_package(gigaedge-enhance CONFIG REQUIRED)

add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE gigaedge::enhance)
target_compile_features(your_app PRIVATE cxx_std_17)
```

Include `<enhance.h>` from C++ or `<enhance/c_api.h>` from C. Continue with the
[C and C++ quickstart](core/quickstart.md).

Android applications add the SDK module and only the model modules they ship. iOS applications link
`EnhanceAudioKit` and embed the model bundles selected by `ModelCatalog`. The Web SDK quickstart
in its package reference shows how to create an audio node from a release build.

## Native C and C++

1. Install a native release archive, or build `native-release-onnx` from source.
2. Load `models/quality48/onnx/config.yaml` for the discriminative 48 kHz ONNX model, or pass a packed
   `.enhance` model to `PackedModelReader`.
3. Create one denoiser and one context per stream.
4. Call `ProcessChunk` with mono PCM, then `GetFinalChunk` when the stream ends.
5. Destroy the context before releasing the denoiser.

The complete calls and the runnable demos are in the [C and C++ quickstart](core/quickstart.md).
File processing is also available through [gigaedge-enhance](core/cli.md).

## Web

Install the release archive of `@gigaedge/enhance-web-sdk` into the application build. Create an
`AudioContext`, then a node:

```ts
const sdk = GigaEdgeEnhanceSDK.create(audioContext);
const node = await sdk.createAudioNode({
  noiseCancellation: 'quality48',
  normalization: true,
  echoCancellation: false,
});
```

Connect `node` between a media source and `audioContext.destination`. The
[Web SDK reference](/docs/source/web/packages/web-sdk/) covers settings, custom models, telemetry,
and the cross-origin isolation required for shared memory.

## Android

Add the SDK and a model module, then process mono input on the capture thread:

```kotlin
Processor(context, sampleRateHz).use { processor ->
    processor.setFeatures(Features(normalization = true, noiseSuppression = "quality48"))
    val output = FloatArray(input.size)
    processor.process(input, output, input.size)
}
```

`process` is non-blocking and may return silence while the worker warms up. The
[Android getting started guide](/docs/source/android/docs/getting-started/) covers Gradle setup,
permissions, threading, and the first-run checklist.

## iOS

Create an audio unit with the bundled discriminative 48 kHz model, or use `Processor` for direct buffers:

```swift
let features = Features(normalization: true, noiseSuppression: .quality48)
let unit = try DenoiserAVAudioUnit(features: features, settings: settings)
```

The [iOS Quick Start](/docs/source/ios/Sources/EnhanceAudioKit/Documentation.docc/QuickStart/)
includes telemetry, feature changes, and in-place processing. Model bundles and error handling have
their own DocC articles.

## Build from source

Contributors restore Git LFS assets and use `tools` from the repository root:

```bash
git lfs install
git lfs checkout
tools/build native-debug-onnx
tools/test native-debug-onnx
```

Docker is the default for native, WebAssembly, Android, and web targets. macOS, iOS, and Windows use
their host toolchains. The [build and test guide](development/build-and-test.md) lists the remaining
presets.
