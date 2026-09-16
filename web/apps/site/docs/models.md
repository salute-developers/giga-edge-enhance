---
title: Models
---

# Models


## Repository contributors

Contributors restore the model and test assets tracked by this repository through Git LFS:

```bash
git lfs checkout
```

Never commit credentials, private storage URLs, or downloaded `.onnx`, `.tflite`, and packed model
binaries outside the repository's established LFS rules.

## Platform packaging

- Native applications provide a model path or packed model through the public pipeline configuration.
- Android applications add only the model AARs they intend to ship and discover their names through
  `GigaDenoiserModels.available(context)`.
- iOS applications use model bundles discovered by `GigaDenoiserModelCatalog`.
- The Web SDK package contains validated packed models and also accepts a custom model buffer.

Loading and validating a model can allocate memory and perform I/O. Do it before the real-time audio
callback begins.
