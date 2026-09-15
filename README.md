# GigaDenoiser

GigaDenoiser is a cross-platform speech-enhancement SDK for real-time noise suppression, loudness
normalization, and acoustic echo cancellation. The repository contains the native C and C++ engine,
WebAssembly and Web Audio integration, Android and iOS SDKs, command-line tools, demos, tests, and
release packaging.

The interactive Web SDK demo and complete documentation are published together as a static GitHub
Pages website. When working in the repository, use the scripts documented in
[`tools/README.md`](tools/README.md) as the supported build and test interface.

## SDKs

| Surface | Location | Intended use |
| --- | --- | --- |
| C and C++ | `core/include/gigadenoiser/` | Native applications and direct buffer processing |
| Web | `web/packages/web-sdk/` | Web Audio, AudioWorklet, workers, and WebAssembly |
| Android | `android/` | Kotlin applications over the JNI/native pipeline |
| iOS | `ios/` | AVAudioEngine and direct Swift buffer processing |

Public headers under `core/include/gigadenoiser/` are compatibility-sensitive. Exported C symbols retain
their global `GigaDenoiser*` prefix; public C++ types live in `gigaedge::denoiser`.

## Requirements

- Git with Git LFS support.
- Docker for the default native, WebAssembly, Android, and web workflows.
- Xcode for iOS and host macOS builds.
- Visual Studio 2022 for Windows builds.
- Node.js from `web/.nvmrc` for host web development.

Restore LFS objects after cloning:

```bash
git lfs install
git lfs checkout
```

## Models

Model binaries are not ordinary Git source files. Repository contributors restore the model and test
assets tracked by this repository through Git LFS:

```bash
git lfs checkout
```

Do not commit model credentials, private artifact URLs, or downloaded model binaries. Platform SDK
consumers should use the model packages documented for that platform.

## Build and test

Run commands from the repository root:

The complete command and target matrix is in [`tools/README.md`](tools/README.md).

Docker is the default for Native, Web, and Android. Use `--host` for those targets only when the
matching local toolchain is already installed. macOS, iOS, and Windows always use their host
toolchains. Commands are deliberately composable: `build`
creates outputs, while `test`, `package`, and `install` consume an existing build without rebuilding it.

Common validation commands:

```bash
# Fast native development check
tools/build native-debug-onnx
tools/test native-debug-onnx

# Open the native Linux container to run built command-line tools
tools/shell

# Release engine coverage
tools/build native-release-onnx
tools/test native-release-onnx
tools/build native-release-tflite
tools/test native-release-tflite

# Memory-safety coverage
tools/build native-asan-onnx
tools/test native-asan-onnx

# Web SDK, including its full and DSP WebAssembly modules
tools/build web
tools/test web
tools/package web
tools/test web-playwright

# Platform wrappers
tools/build android
tools/test android
tools/build ios
tools/test ios
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

Link against an installed or packaged GigaDenoiser build and include the supported public headers. A
minimal C++ pipeline creates the processor with a model/configuration, processes mono floating-point
frames, and keeps all allocation and pipeline rebuild work off the real-time callback.

The exact constructor and configuration types vary by pipeline and inference-engine build. Use the
public headers and the documentation generated from the same revision rather than copying internal
classes from `core/src/`.

Command-line examples are available through the installed tools:

```bash
giga-denoiser --help
giga-denoiser denoise --help
giga-denoiser norm --help
giga-denoiser echo --help
```

## Web SDK

The Web SDK is an npm workspace package with a separate demo service:

```bash
tools/build web

cd web
npm run test:web-sdk
npm run build:web-sdk
```

The build stages validated WASM, worker, worklet, model, and type-definition assets into the package.
Do not edit staged or generated WebAssembly artifacts manually.

For local demo development:

```bash
cd web
npm ci
npm start --workspace=@gigaedge/denoiser-site
```

The static site installs a same-origin COI service worker so GitHub Pages can expose the isolated
context required for `SharedArrayBuffer`. The first visit may reload once while the worker takes
control; unsupported environments fall back to MessageChannel.

## Documentation

The documentation website is built by the `@gigaedge/denoiser-site` workspace. Its staging script collects all
user-facing repository Markdown while excluding generated, vendored, cached, and internal agent
files. Platform READMEs and iOS DocC articles remain canonical in their existing locations.

```bash
cd web
npm run build --workspace=@gigaedge/denoiser-site
```

The static output is written to `web/apps/site/build/` and is suitable
for GitHub Pages.

## Development

- Follow [`STYLE.md`](STYLE.md) and `.clang-format` for C and C++ changes.
- Keep tests close to affected behavior under `core/tests/` or the matching platform wrapper.
- Run the narrowest relevant check first, then broaden coverage for cross-platform, ABI, model,
  memory-ownership, or packaging changes.
- Do not edit dependency sources under `out/cache/cpm/`; update `package-lock.cmake` or the
  corresponding patch under `cmake/patches/` instead.
- Preserve generated-file boundaries and keep lockfiles synchronized with manifest changes.

For C and C++ style validation:

```bash
git-clang-format --diff HEAD --extensions c,cc,cpp,h,hpp -- gigadenoiser
```

## Releases

`VERSION_NUMBER` is the release version source. Release Please manages version and changelog updates, while
GitHub Actions builds and attaches validated platform artifacts. A rerun may verify an existing
artifact with the same hash but must not overwrite a published version with different bytes.

See [`CHANGELOG.md`](CHANGELOG.md) for release history.

## Security

Report suspected vulnerabilities privately as described in the
[security policy](https://github.com/salute-developers/gigadenoiser/blob/master/SECURITY.md). Do not disclose security
issues in a public GitHub issue.

## License

GigaDenoiser is licensed under the [MIT License](LICENSE).
