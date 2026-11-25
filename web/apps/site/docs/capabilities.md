---
sidebar_position: 4
title: Capabilities
---

# Capabilities

The native engine resamples between a stream's sample rate and the rate expected
by the selected model. Callers are not limited to 8 kHz or 48 kHz. Pass the
actual input and output rates when creating a stream context; Python, Android, iOS, and
Web pass that rate through their SDK entry points.

Independent streams can be processed at the same time. Give every stream its own
context and call the processing function for that context. The concurrent-stream
tests exercise this with one denoiser and several contexts. The 8 kHz quantized
model is the CPU-oriented choice when a server must keep many of those streams
moving at once.

## Feature matrix

| Capability | C and C++ | Python | Web | Android | iOS |
| --- | --- | --- | --- | --- | --- |
| Noise suppression | Yes | Yes, `Denoiser` and RNNoise | Yes | Yes | Yes |
| Normalization | Yes | Six standalone/composable DSP modules | Yes | Yes | Yes |
| Acoustic echo cancellation | Yes, `EchoCanceller` | Yes, explicit mono reference | Yes, `echoCancellation` | Not in the Kotlin feature API | Not in the Swift feature API |
| Built-in resampling | Yes | Denoiser conversion and standalone `Resampler` | Yes | Yes | Yes |
| 8 kHz quantized model | ONNX | ONNX `fast8` | Packed `fast8` | Not packaged | Packed `fast8` |
| Discriminative 48 kHz model | ONNX and TFLite | ONNX and TFLite `quality48` | Packed `quality48` | Packed `quality48` | Packed `quality48` |
| Hybrid 48 kHz model | ONNX and TFLite | ONNX and TFLite `hybrid48` | Packed `hybrid48` | Packed `hybrid48` | Packed `hybrid48` |

The Python SDK also provides voice activity detection, loudness metering and composable audio pipelines. [Python API reference](https://github.com/salute-developers/giga-edge-enhance/blob/master/python/reference.md)

Acoustic echo cancellation is part of the native library, Python and the Web SDK. The
Android `Features` type and the iOS `Features` type currently select
normalization and a noise-suppression model only. They do not expose an echo
reference input.

The 8 kHz quantized runner is ONNX-only. A TensorFlow Lite build rejects that
model type. The discriminative and hybrid 48 kHz runners exist for both ONNX
Runtime and TensorFlow Lite; see [Models](models.md).
