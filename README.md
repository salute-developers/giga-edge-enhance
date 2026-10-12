# GigaEdge Spench

![GigaEdge Spench](spench_logo.svg)

GigaEdge Spench is a cross-platform SDK for real-time speech enhancement. It combines neural noise
suppression with DSP for
native, mobile, and web applications.

The repository includes the C and C++ engine, WebAssembly integration, Android and iOS SDKs,
command-line tools, demos, tests, and release packaging. The interactive demo and rendered
documentation are available on the repository's GitHub Pages.

## SDKs

| Platform | Integration | Documentation |
| --- | --- | --- |
| C and C++ | Public C API and `gigaedge::spench` C++ API | [Core guide](web/apps/site/docs/core/index.md) |
| Web | Web Audio, AudioWorklet, workers, and WebAssembly | [Web SDK](web/packages/web-sdk/README.md) |
| Android | Kotlin API backed by the native processing pipeline | [Android SDK](android/README.md) |
| iOS | Swift API for `AVAudioEngine` and direct buffer processing | [iOS SDK](ios/README.md) |

## Build and test

Git LFS is required for repository-managed models and test assets. Docker provides the default
toolchain for native Linux, WebAssembly, Web, and Android builds; Apple and Windows targets use their
host toolchains.

```bash
git lfs install
git lfs checkout

tools/build native-debug-onnx
tools/test native-debug-onnx
```

Use the repository's [`tools`](tools/README.md) commands for supported builds, tests, installation,
and packaging. Build outputs are written under `out/`.

## Documentation

- [Getting started](web/apps/site/docs/getting-started.md)
- [Core command-line tools](web/apps/site/docs/core/cli.md)
- [Models](web/apps/site/docs/models.md)
- [Build and test guide](web/apps/site/docs/development/build-and-test.md)
- [Web workspace](web/README.md) and [documentation website](web/apps/site/README.md)
- [CI and releases](web/apps/site/docs/development/ci-and-releases.md)
- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Changelog](CHANGELOG.md)

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
