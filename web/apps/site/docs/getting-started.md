---
sidebar_position: 2
title: Getting started
---

# Getting started

The fastest way to evaluate GigaEdge Spench is the [browser demo](/). For product integration, choose the
platform-specific guide from the documentation overview.

Repository contributors should run supported builds through `tools` from the repository root:

```bash
tools/build native-debug-onnx
tools/test native-debug-onnx
tools/build web
tools/test web
tools/build android
tools/test android
tools/build ios
tools/test ios
```

Docker is the default for native, WebAssembly, Android, and web targets. Use `--host` only when
running one of those targets with an installed local toolchain. macOS, iOS, and Windows always use
their host toolchains.
