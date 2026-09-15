---
title: Build and test
---

# Build and test

Run builds from the repository root through `tools`. Start with the narrowest target affected
by a change and broaden coverage when code crosses inference engines, platforms, ABI boundaries, or
memory ownership.

```bash
tools/build native-debug-onnx
tools/test native-debug-onnx
tools/build native-asan-onnx
tools/test native-asan-onnx
tools/build web
tools/test web
```

Build and test are separate operations. Test commands require existing build outputs and never invoke
the corresponding build command implicitly.

Generated build trees, dependency caches, staged WebAssembly, and package artifacts are not source
files and must not be committed.
