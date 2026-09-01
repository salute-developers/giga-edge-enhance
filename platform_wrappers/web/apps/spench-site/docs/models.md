---
title: Models
---

# Models

Spench model files are runtime assets, not source dependencies. Keep model selection explicit and
match the model's expected sample rate to the audio pipeline.

## Repository contributors

Authorized contributors restore development and test models through DVC:

```bash
dvc pull
```

Never commit `.dvc/config`, credentials, private storage URLs, or downloaded `.onnx`, `.tflite`, and
packed model binaries outside the repository's established LFS/DVC rules.

## Platform packaging

- Native applications provide a model path or packed model through the public pipeline configuration.
- Android applications add only the model AARs they intend to ship and discover their names through
  `SpenchModels.available(context)`.
- iOS applications use model bundles discovered by `SpenchModelCatalog`.
- The Web SDK package contains validated packed models and also accepts a custom model buffer.

Loading and validating a model can allocate memory and perform I/O. Do it before the real-time audio
callback begins.
