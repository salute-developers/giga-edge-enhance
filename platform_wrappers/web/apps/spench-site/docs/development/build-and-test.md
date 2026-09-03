---
title: Build and test
---

# Build and test

Run builds from the repository root through `tools/spench`. Start with the narrowest target affected
by a change and broaden coverage when code crosses inference engines, platforms, ABI boundaries, or
memory ownership.

```bash
tools/spench build native-debug-onnx
tools/spench test native-debug-onnx
tools/spench build native-asan-onnx
tools/spench test native-asan-onnx
tools/spench build web
tools/spench test web
```

Build and test are separate operations. Test commands require existing build outputs and never invoke
the corresponding build command implicitly.

Generated build trees, dependency caches, staged WebAssembly, and package artifacts are not source
files and must not be committed.
