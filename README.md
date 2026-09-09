# Spench

![Spench signal logo](spench_logo.svg "Speech Enhancement Channel")

Spench is a cross-platform speech-enhancement SDK for real-time noise suppression, loudness
normalization, and acoustic echo cancellation. The repository contains the native C and C++ engine,
WebAssembly and Web Audio integration, Android and iOS SDKs, command-line tools, demos, tests, and
release packaging.

The interactive Web SDK demo and complete documentation are published together as a static GitHub
Pages website. When working in the repository, use `tools/spench` as the supported build and test
entry point.

## SDKs

| Surface | Location | Intended use |
| --- | --- | --- |
| C and C++ | `spench/include/spench/` | Native applications and direct buffer processing |
| Web | `platform_wrappers/web/packages/web-sdk/` | Web Audio, AudioWorklet, workers, and WebAssembly |
| Android | `platform_wrappers/android/` | Kotlin applications over the JNI/native pipeline |
| iOS | `platform_wrappers/ios/` | AVAudioEngine and direct Swift buffer processing |

Public headers under `spench/include/spench/` are compatibility-sensitive. Existing exported C
symbols and public C++ methods are not renamed for style.

## Requirements

- Git with Git LFS support.
- Docker for the default native, WebAssembly, Android, and web workflows.
- Xcode for iOS and host macOS builds.
- Visual Studio 2022 for Windows builds.
- Node.js from `platform_wrappers/web/.nvmrc` for host web development.

Clone submodules and restore LFS objects before building:

```bash
git submodule update --init --recursive
git lfs install
git lfs checkout
```

## Models

Model binaries are not ordinary Git source files. Repository contributors retrieve authorized model
artifacts through DVC:

```bash
dvc pull
```

Do not commit `.dvc/config`, model credentials, private artifact URLs, or downloaded model binaries.
Platform SDK consumers should use the model packages documented for that platform.

## Build and test

Run commands from the repository root:

```bash
tools/spench <build|test|package|install|core|cache-key|prepare-deps> <target> [--host]
```

Docker is the default for Native, Web, and Android. Use `--host` for those targets only when the
matching local toolchain is already installed. macOS, iOS, and Windows always use their host
toolchains. Commands are deliberately composable: `build`
creates outputs, while `test`, `package`, and `install` consume an existing build without rebuilding it.

Common validation commands:

```bash
# Fast native development check
tools/spench build native-debug-onnx
tools/spench test native-debug-onnx

# Open the native Linux container to run built command-line tools
tools/spench shell native

# Release engine coverage
tools/spench build native-release-onnx
tools/spench test native-release-onnx
tools/spench build native-release-tflite
tools/spench test native-release-tflite

# Memory-safety coverage
tools/spench build native-asan-onnx
tools/spench test native-asan-onnx

# Web SDK, including its full and DSP WebAssembly modules
tools/spench build web
tools/spench test web
tools/spench package web
tools/spench test web-playwright

# Platform wrappers
tools/spench build android
tools/spench test android
tools/spench build ios
tools/spench test ios
```

The shell starts in `/workspace`, where the repository and Linux build outputs are mounted. The host
home directory is mounted read/write at `/host-home` and becomes the shell's home, so `cd ~` opens
the host home and files such as `~/Music/input.wav` are available to Linux command-line tools. Project
caches remain under `/workspace/out/cache`. The shell does not build automatically; run a native build
first, then execute its binaries from `out/build/container-clang-llvm-tools/<preset>`. Use `exit` or
Ctrl-D to leave the disposable container. Only use the shell with code you trust because it can modify
files in the host home directory.

### Supported target families

| Target family | Examples | Notes |
| --- | --- | --- |
| Native Linux | `native-debug-onnx`, `native-release-tflite` | Debug, Release, ASAN, and static variants |
| macOS | `macos-arm64-release-onnx` | Architecture-specific host builds |
| Windows | `windows-x64-release-tflite` | Invoke the helper with Python |
| WebAssembly | `wasm-release`, `wasm-dsp-release` | Full pipeline and DSP-only variants |
| Web | `web` | Stages WASM, tests the SDK, and builds packages |
| Android | `android` | SDK, JNI, unit, and optional device coverage |
| iOS | `ios` | Core, XCFramework, simulator tests, and DocC |

Artifacts are written under `out/artifacts/`. Generated build trees and dependency caches under
`out/`, `build/`, `.cpm/`, and `.cache/` are not source files.

## Native integration

Link against an installed or packaged Spench build and include the supported public headers. A
minimal C++ pipeline creates the processor with a model/configuration, processes mono floating-point
frames, and keeps all allocation and pipeline rebuild work off the real-time callback.

The exact constructor and configuration types vary by pipeline and inference-engine build. Use the
public headers and the documentation generated from the same revision rather than copying internal
classes from `spench/src/`.

Command-line examples are available through the installed tools:

```bash
spench-tool --help
spench-tool denoise --help
spench-tool norm --help
spench-tool echo --help
```

## Web SDK

The Web SDK is an npm workspace package with a separate demo service:

```bash
tools/spench build web

cd platform_wrappers/web
npm run test:web-sdk
npm run build:web-sdk
```

The build stages validated WASM, worker, worklet, model, and type-definition assets into the package.
Do not edit staged or generated WebAssembly artifacts manually.

For local demo development:

```bash
cd platform_wrappers/web
npm ci
npm start --workspace=@gigaedge/spench-site
```

The static site installs a same-origin COI service worker so GitHub Pages can expose the isolated
context required for `SharedArrayBuffer`. The first visit may reload once while the worker takes
control; unsupported environments fall back to MessageChannel.

## Documentation

The documentation website is built by the `@gigaedge/spench-site` workspace. Its staging script collects all
user-facing repository Markdown while excluding generated, vendored, cached, and internal agent
files. Platform READMEs and iOS DocC articles remain canonical in their existing locations.

```bash
cd platform_wrappers/web
npm run build --workspace=@gigaedge/spench-site
```

The static output is written to `platform_wrappers/web/apps/spench-site/build/` and is suitable
for GitHub Pages.

## Development

- Follow [`STYLE.md`](STYLE.md) and `.clang-format` for C and C++ changes.
- Keep tests close to affected behavior under `spench/tests/` or the matching platform wrapper.
- Run the narrowest relevant check first, then broaden coverage for cross-platform, ABI, model,
  memory-ownership, or packaging changes.
- Do not edit dependency sources under `out/cache/cpm/`; update `package-lock.cmake` or the
  corresponding patch under `cmake/patches/` instead.
- Preserve generated-file boundaries and keep lockfiles synchronized with manifest changes.

For C and C++ style validation:

```bash
git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- spench
```

## Releases

`SPENCH_VERSION` is the release version source. Release Please manages version and changelog updates, while
GitHub Actions builds and attaches validated platform artifacts. A rerun may verify an existing
artifact with the same hash but must not overwrite a published version with different bytes.

See [`CHANGELOG.md`](CHANGELOG.md) for release history.

## License

Spench is licensed under the [MIT License](LICENSE).
