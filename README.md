# GigaEdge Spench

![GigaEdge Spench](spench_logo.svg)

GigaEdge Spench is a cross-platform SDK for real-time speech enhancement. It adds neural noise
suppression, loudness normalization, and, on supported platforms, acoustic echo cancellation to
native, mobile, and web applications.

## Choose an SDK

| Platform | Start here | First call |
| --- | --- | --- |
| C and C++ | [Quickstart](web/apps/site/docs/core/quickstart.md) | `Denoiser` or `GigaEdgeSpenchDenoise*` |
| Web | [Web SDK](web/packages/web-sdk/README.md) | `GigaEdgeSpenchSDK.create` |
| Android | [Android SDK](android/README.md) | `Processor.process` |
| iOS | [Quick Start](ios/Sources/GigaEdgeSpenchAudioKit/Documentation.docc/QuickStart.md) | `DenoiserAVAudioUnit` or `Processor` |

The browser demo is published with the documentation site. The
[capability matrix](web/apps/site/docs/capabilities.md) shows which features and
models each SDK exposes. Release archives and the Web SDK npm tarball are
described in [Distribution](web/apps/site/docs/distribution.md).

```cpp
#include <spench.h>

#include <vector>

int main() {
  gigaedge::spench::Denoiser denoiser("models/quality48/onnx/config.yaml");
  auto* context = denoiser.CreateContext(48000, 48000);

  std::vector<float> input;  // Mono PCM in [-1, 1], at the context input rate.
  std::vector<float> output;
  const auto code = denoiser.ProcessChunk(input, output, context);
  denoiser.DestroyContext(&context);
  return code < GigaEdgeSpenchRetCode::SUCCESS ? 1 : 0;
}
```

Prepare the pipeline before a real-time callback. `ProcessChunk` can return success before it emits
a full buffer while the model collects overlap. The [C and C++ quickstart](web/apps/site/docs/core/quickstart.md)
shows the streaming loop, `GetFinalChunk`, and the C API.

## Use a release

Pin a [GitHub Release](https://github.com/salute-developers/gigadenoiser/releases). Each release
contains the tested native, Android, iOS, and Web SDK archives. The Web SDK archive is an npm
tarball. The release workflow does not publish that tarball to the npm registry.

A native archive exports the CMake package `gigaedge-spench`:

```cmake
find_package(gigaedge-spench CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE gigaedge-spench::gigaedge-spench)
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
- [Command-line tools](web/apps/site/docs/core/cli.md)
- [Contributing](CONTRIBUTING.md) and [support](SUPPORT.md)

Report vulnerabilities privately as described in the [security policy](SECURITY.md).

## License

GigaEdge Spench is available under the [MIT License](LICENSE).

## Acknowledgement

GigaEdge Spench includes work by people who contributed to the product at different times:

- Artem Sokolov
- Artem Khaidarov
- Yana Lysova
- Vitaliy Yulis
- Dmitriy Zhelonkin
- Alexander Gladkikh
