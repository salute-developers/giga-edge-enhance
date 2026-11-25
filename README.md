# GigaEdge Enhance

<img src="enhance_logo.svg" alt="GigaEdge Enhance" width="33%">

GigaEdge Enhance is a cross-platform SDK for real-time speech enhancement. It adds neural noise
suppression, loudness normalization, and, on supported platforms, acoustic echo cancellation to
native, mobile, and web applications.

[Models on Hugging Face](https://huggingface.co/ai-sage/GigaEdge-Enhance) · [Research paper](https://arxiv.org/abs/2611.13243) · [Listen to the 15-second comparison](samples/README.md)

## Choose an SDK

| Platform | Start here |
| --- | --- |
| C and C++ | [Quickstart](web/apps/site/docs/core/quickstart.md) |
| Python | [Quick start](python/readme.md#quick-start) |
| Web | [Web SDK](web/packages/web-sdk/README.md) |
| Android | [Android SDK](android/README.md) |
| iOS | [Quick Start](ios/Sources/EnhanceAudioKit/Documentation.docc/QuickStart.md) |

The browser demo is published with the documentation site. The
[capability matrix](web/apps/site/docs/capabilities.md) shows which features and
models each SDK exposes. Release archives and the Web SDK npm tarball are
described in [Distribution](web/apps/site/docs/distribution.md).

```cpp
#include <giga-edge-enhance.h>

#include <vector>

int main() {
  giga_edge::enhance::Denoiser denoiser("models/quality48/onnx/config.yaml");
  auto* context = denoiser.CreateContext(48000, 48000);

  std::vector<float> input;  // Mono PCM in [-1, 1], at the context input rate.
  std::vector<float> output;
  const auto code = denoiser.ProcessChunk(input, output, context);
  denoiser.DestroyContext(&context);
  return code < GigaEdgeEnhanceRetCode::SUCCESS ? 1 : 0;
}
```

Prepare the pipeline before a real-time callback. `ProcessChunk` can return success before it emits
a full buffer while the model collects overlap. The [C and C++ quickstart](web/apps/site/docs/core/quickstart.md)
shows the streaming loop, `GetFinalChunk`, and the C API.

## Use a release

Pin a [GitHub Release](https://github.com/salute-developers/giga-edge-enhance/releases). Each release
contains the tested native, Android, iOS, and Web SDK archives. The Web SDK archive is an npm
tarball. The release workflow does not publish that tarball to the npm registry.

A native archive exports the CMake package `giga-edge-enhance`:

```cmake
find_package(giga-edge-enhance CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE giga-edge::enhance)
```

Android and iOS packages are consumed as described in their SDK guides. The repository build can
produce the same native archive locally with `tools/package`.

## Build the repository

This path is for contributors. Git LFS restores models and test audio. Docker is the default
toolchain for native Linux, WebAssembly, Web, and Android; Apple and Windows targets use their host
toolchains.

```bash
git lfs install
git lfs checkout
tools/build native-debug-onnx
tools/test native-debug-onnx
```

Supported commands, presets, and packages are documented in [`tools`](tools/README.md). Build
outputs are written under `out/`.

## Documentation

- [Getting started](web/apps/site/docs/getting-started.md)
- [Distribution](web/apps/site/docs/distribution.md)
- [Capabilities](web/apps/site/docs/capabilities.md)
- [C and C++ quickstart](web/apps/site/docs/core/quickstart.md)
- [Models](web/apps/site/docs/models.md)
- [Audio normalization](web/apps/site/docs/core/normalization.md)
- [Acoustic echo cancellation (AEC)](web/apps/site/docs/core/echo-cancellation.md)
- [Command-line tools](web/apps/site/docs/core/cli.md)
- [Contributing](CONTRIBUTING.md) and [support](SUPPORT.md)

Report vulnerabilities privately as described in the [security policy](SECURITY.md).

## License

GigaEdge Enhance is available under the [MIT License](LICENSE.txt).

## Acknowledgement

GigaEdge Enhance includes work by people who contributed to the product at different times:

- Artem Sokolov
- Artem Khaidarov
- Yana Lysova
- Vitaliy Yulis
- Dmitriy Zhelonkin
- Alexander Gladkikh
