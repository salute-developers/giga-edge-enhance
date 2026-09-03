---
sidebar_position: 2
title: Getting started
---

# Getting started

The fastest way to evaluate Spench is the [browser demo](/). For product integration, choose the
platform-specific guide from the documentation overview.

Repository contributors should run supported builds through `tools/spench` from the repository root:

```bash
tools/spench build native-debug-onnx
tools/spench test native-debug-onnx
tools/spench build web
tools/spench test web
tools/spench build android
tools/spench test android
tools/spench build ios
tools/spench test ios
```

Docker is the default for native, WebAssembly, Android, and web targets. iOS uses the host Xcode
toolchain. Use `--host` only when the matching local toolchain is already installed.
