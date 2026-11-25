# Development tools

Run every command from the repository root. The scripts in this directory are
the supported interface for configuring, building, testing, installing, and
packaging GigaEdge Enhance. Prefer them to ad hoc CMake, Docker, Gradle, Xcode, or
npm command chains.

Docker is the default for native Linux, WebAssembly, Web, and Android work.
Host mode is retained for explicit manual troubleshooting only; normal builds,
tests, and reported validation must omit `--host`. macOS, Windows, and iOS
targets use their required host toolchains implicitly.

## Commands

```text
tools/build TARGET [--host]
tools/test TARGET [--host]
tools/package TARGET [--host]
tools/install TARGET [--host]
tools/shell
tools/ci ACTION ...
```

`build` creates build output. `test`, `package`, and `install` consume an
existing build produced with the same target and execution mode.

## Build

```text
tools/build native-debug-onnx [--host]
tools/build native-release-onnx [--host]
tools/build native-debug-tflite [--host]
tools/build native-release-tflite [--host]
tools/build native-release-static-onnx [--host]
tools/build native-release-static-tflite [--host]
tools/build native-asan-onnx [--host]
tools/build native-model-packer [--host]

tools/build wasm-debug [--host]
tools/build wasm-release [--host]
tools/build wasm-dsp-debug [--host]
tools/build wasm-dsp-release [--host]

tools/build macos-x86_64-debug-onnx
tools/build macos-x86_64-release-onnx
tools/build macos-x86_64-debug-tflite
tools/build macos-x86_64-release-tflite
tools/build macos-x86_64-release-static-onnx
tools/build macos-x86_64-asan-onnx
tools/build macos-arm64-debug-onnx
tools/build macos-arm64-release-onnx
tools/build macos-arm64-debug-tflite
tools/build macos-arm64-release-tflite
tools/build macos-arm64-release-static-onnx
tools/build macos-arm64-asan-onnx

tools/build windows-x64-debug-onnx
tools/build windows-x64-release-onnx
tools/build windows-x64-debug-tflite
tools/build windows-x64-release-tflite
tools/build windows-x64-release-static-tflite
tools/build windows-x64-asan-onnx

tools/build ios-device-debug
tools/build ios-device-release
tools/build ios-simulator-debug
tools/build ios-simulator-release

tools/build android [--host]
tools/build ios
tools/build web [--host]
```

`tools/build ios` builds both native iOS slices and the complete XCFramework.
The slices remain available for focused native-core work.

## Test

All native targets except `native-model-packer` can be passed to `tools/test`.
All macOS and Windows targets listed under Build can also be tested. Native and
WebAssembly tests accept `--host`; desktop targets do not.

```text
tools/test native-debug-onnx [--host]
tools/test native-release-onnx [--host]
tools/test native-debug-tflite [--host]
tools/test native-release-tflite [--host]
tools/test native-release-static-onnx [--host]
tools/test native-release-static-tflite [--host]
tools/test native-asan-onnx [--host]

tools/test wasm-debug [--host]
tools/test wasm-release [--host]
tools/test wasm-dsp-debug [--host]
tools/test wasm-dsp-release [--host]

tools/test android [--host]
tools/test android-ui
tools/test ios
tools/test web [--host]
tools/test web-playwright [--host]
```

`android-ui` requires Linux KVM and always uses the Android UI container. iOS
slice presets do not have standalone CTest suites; use `tools/test ios`.

## Package

```text
tools/package native-release-onnx [--host]
tools/package native-release-tflite [--host]
tools/package web [--host]
```

Packages are written below `out/artifacts/`. Build the same target first.

## Install

`tools/install` supports every native target except `native-model-packer`, plus
all macOS and Windows targets listed under Build. It installs below
`out/artifacts/install/TARGET` and builds an external CMake consumer to verify
the installed package.

```text
tools/install native-release-onnx [--host]
tools/install macos-arm64-release-onnx
tools/install windows-x64-release-tflite
```

## Shell

```text
tools/shell
```

This opens the native Linux development container at `/workspace`. The
repository is mounted read/write, and the host home is mounted at `/host-home`.
`/host-home` becomes the shell's home, so `cd ~` opens the host home and makes
files such as `~/Music/input.wav` available to containerized command-line tools.
Project caches remain under `/workspace/out/cache`.

The shell does not build automatically. Build a native target first, then run
its tools from `out/build/container-clang-llvm-tools/TARGET`.

## CI Operations

These commands support composite actions and workflows. Contributors normally
do not need them for source-level work.

```text
tools/ci image-key {native|web|android|android-ui|python}
tools/ci recipe-key {native|web|android|python}
tools/ci cache-key TARGET [--host]
tools/ci prepare-deps TARGET [--host]
tools/ci dependency-key TARGET [--host]
tools/ci dependency-archive TARGET [--host]
tools/ci restore-deps TARGET ARCHIVE [--host]
```

`prepare-deps` accepts these group aliases in addition to concrete targets:

```text
android
web
ios
macos
windows
```

Concrete dependency targets are native ONNX/TFLite targets,
`python-release-onnx`, `python-release-tflite`, `wasm-debug`,
`wasm-release`, all macOS and Windows ONNX/TFLite targets, all four iOS slice
targets, and:

```text
android-release-tflite-arm64-v8a
android-release-tflite-armeabi-v7a
android-release-tflite-x86_64
android-release-tflite-x86
```

`native-model-packer` and `wasm-dsp-*` do not use inference-engine dependency
bundles.

## Common Workflows

### Python SDK

Run the following from a checkout with Git LFS files restored (`git lfs pull`). On Linux, the `python` target uses Docker. On macOS use `python-macos` with Xcode Command Line Tools. On Windows use `python-windows` in x64 Developer PowerShell.

```sh
python tools/build python
python tools/test python
python tools/package python
```

The development wheel is written to `out/artifacts/<target>/`. Tests install it in a separate environment and compare Python output with the C++ SDK. For a sanitizer build on macOS, set `GEE_PYTHON_SANITIZE=1` for the build and test. Rebuild normally before packaging.

For release wheels, use a Python 3.11+ driver and run `tools/build python-wheels` followed by `tools/test python-wheels`. Linux also requires `tools/build python-sdist` and `tools/test python-sdist` before `tools/package python-wheels`. Run `tools/test python-lint` for formatting and type checks. CI repairs and tests CPython 3.10–3.14 wheels for Linux x86_64, macOS arm64 and x86_64, and Windows x64.

When model files change, run `python python/scripts/update_catalog.py` and review the hashes. The Hub repository uses the same `<model>/<backend>/` directories as `models/`, with the original weights and YAML, a model card and the root `LICENSE.txt`. The catalog ships in the Python package.

### Native, web and mobile SDKs

```bash
# Fast native validation
tools/build native-debug-onnx
tools/test native-debug-onnx

# Native release package
tools/build native-release-onnx
tools/test native-release-onnx
tools/install native-release-onnx
tools/package native-release-onnx

# Web SDK and browser demo
tools/build web
tools/test web
tools/package web
tools/test web-playwright

# Mobile SDKs
tools/build android
tools/test android
tools/build ios
tools/test ios
```
