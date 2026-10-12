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
  `GigaEdgeSpenchModels.available(context)`.
- iOS applications use model bundles discovered by `GigaEdgeSpenchModelCatalog`.
- The Web SDK package contains validated packed models and also accepts a custom model buffer.

Loading and validating a model can allocate memory and perform I/O. Do it before the real-time audio
callback begins.

## Hybrid model contract

Set `inference.model_type: denoiser_hybrid_web_model` for an FP32 hybrid model with ONNX Runtime or TensorFlow Lite.
Hybrid models consume complex spectra in `[1, frequency, time, 2]` order (real, imaginary), plus
eight recurrent state tensors. They return the eight updated states followed by the enhanced
complex spectrum. Only the spectrum's time dimension may be dynamic; recurrent states have fixed
shapes and are initialized to zero for each new stream.

The recurrent state order is fullband hidden state, fullband noise PSD and PG, subband noise PSD
and PG, two convolution buffers, and second-stage hidden state. ONNX inputs and outputs must follow
this order, with the input spectrum first and output spectrum last. TensorFlow Lite uses the
`noisy_ri_in`, `prev_*_in`, `cur_*_out`, and `predicted_ri_out` signature names used by the hybrid
runner; physical tensor order may differ.

The repository's `hybrid_identity.onnx` and `hybrid_identity.tflite` are synthetic test fixtures,
not denoising models. Their generator is `core/tests/generate_hybrid_fixtures.py`. No trained hybrid
weights are included. The packed model format and neural-network weights are unchanged; shipped bundles are repacked
with the new configuration values.

### Configuration migration

Replace `best_quality` with `denoiser_discr_web_model` (48 kHz HQ), and `best_speed_new` with
`denoiser_discr_hq_model` (8 kHz half-band quantized ONNX). The legacy `best_speed` 24 kHz runner has been
removed; it has no compatible replacement value. Use a supported model and its matching feature
configuration. The default denoiser is now 48 kHz HQ. Old configuration values and packed bundles
containing them must be updated before loading them with this version.
