---
title: Build and test
---

# Build and test

Run builds from the repository root through `tools/spench`. Start with the narrowest target affected
by a change and broaden coverage when code crosses inference engines, platforms, ABI boundaries, or
memory ownership.

```bash
tools/spench test native-debug-onnx
tools/spench test native-asan-onnx
tools/spench test wasm-release
tools/spench build web
```

Generated build trees, dependency caches, staged WebAssembly, and package artifacts are not source
files and must not be committed.
