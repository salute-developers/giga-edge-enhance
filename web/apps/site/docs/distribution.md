---
sidebar_position: 3
title: Distribution
---

# Distribution

GitHub Releases and npm can be used together. A release archive is the tested
byte-for-byte artifact. An npm package is an install format for the Web SDK,
not a second build.

## What a release contains

The release workflow promotes the archives produced by a successful CI run of the
tagged commit. Each asset is named `gigaedge-enhance-<target>-<tag>.zip`, and the
release also includes `SHA256SUMS`.

| Asset prefix | Contents |
| --- | --- |
| `gigaedge-enhance-linux-onnx` | Linux native package, ONNX Runtime |
| `gigaedge-enhance-linux-tflite` | Linux native package, TensorFlow Lite |
| `gigaedge-enhance-macos-arm64-onnx` | macOS Apple silicon, ONNX Runtime |
| `gigaedge-enhance-macos-arm64-tflite` | macOS Apple silicon, TensorFlow Lite |
| `gigaedge-enhance-macos-x86_64-onnx` | macOS Intel, ONNX Runtime |
| `gigaedge-enhance-macos-x86_64-tflite` | macOS Intel, TensorFlow Lite |
| `gigaedge-enhance-windows-x64-onnx` | Windows x64, ONNX Runtime |
| `gigaedge-enhance-windows-x64-tflite` | Windows x64, TensorFlow Lite |
| `gigaedge-enhance-android` | Android SDK and model modules |
| `gigaedge-enhance-ios` | iOS XCFramework and model bundles |
| `gigaedge-enhance-web-sdk` | npm tarball of `@gigaedge/enhance-web-sdk` |

Native archives export the CMake package `giga-edge-enhance`. After unpacking one,
configure the consumer with `find_package(giga-edge-enhance CONFIG REQUIRED)` and
link `giga-edge::enhance`.

The same archives can be built locally with `tools/package` for a concrete
target. Release publication uses the CI archives, not a package rebuilt from a
different commit.

## Web SDK and npm

`tools/package web` runs `npm pack` for `@gigaedge/enhance-web-sdk` and writes the
resulting `.tgz` to `out/artifacts/web/`. CI uploads that tarball, and the
release asset `gigaedge-enhance-web-sdk-<tag>.zip` contains it. Install the
unpacked tarball:

```bash
npm install ./gigaedge-enhance-web-sdk-1.0.0.tgz
```

The package name and version are inside the tarball; the filename above matches
the current package version. This is a normal npm install from a file. It does
not require the package to be in the public npm registry.

The current release workflow does not run `npm publish`. The package is also
marked `private`, so an accidental registry publish is rejected. Adding a
registry later means publishing this same tested tarball. GitHub Releases remain
the source archive for native, Android, iOS, and Web SDK builds.
