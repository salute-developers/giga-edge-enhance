---
title: Command-line tools
---

# Command-line tools

Native builds include focused tools for denoising, normalization, and acoustic echo cancellation.
Build them with a supported native preset, then inspect each command's `--help` output for the exact
arguments supported by that build.

```bash
tools/build native-release-onnx
tools/shell

# Inside the container
out/build/container-clang-llvm-tools/native-release-onnx/core/cli/demo/gigadenoiser --help
```

The shell starts in `/workspace` with the repository, user identity, and development caches mounted
the same way as other containerized workflows. It does not build the CLI automatically. Run `exit`
or press Ctrl-D to remove the disposable container and return to the host terminal.
