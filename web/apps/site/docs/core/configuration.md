---
title: Configuration
---

# Configuration and processing

Prepare the denoiser, DSP chain, and echo canceller before a real-time callback. The callback
passes mono PCM through contexts that already exist.

## Denoiser

`Denoiser` loads a noise-suppression model from a YAML file, a YAML string, or a
`PackedModelReader`. Copy construction is disabled. Move the object, or create it in the place that
owns it for the session.

`CreateContext(input_rate, output_rate)` starts one stream. Pass the rates of the buffers you
supply. `0, 0` means those buffers are already at `feature_params.sampling_freq` and the resampler
stays off. Any other pair is resampled to and from the model rate. One denoiser can own many
contexts. Process each context on its own stream; concurrent `ProcessChunk` calls use different
contexts.

`ProcessChunk` accepts mono `float` in `[-1, 1]` or mono `int16_t`. `GetFinalChunk` flushes the
overlap-add tail and replaces the output vector. `DestroyContext` takes a pointer to the context
pointer and clears it. Destroy every context before the denoiser.

`GetModelSampleRate` and `GetModelInputFrameSize` report the model contract. `GetModelInputFrameSize`
is the hop, in samples, at the model rate. `ReloadConfig` rebuilds the feature extractor and
inference runner from the current configuration. Call it outside the real-time callback.

## Model YAML

A model file has three sections. The shipped 48 kHz discriminative ONNX config is
`models/quality48/onnx/config.yaml`:

```yaml
model:
  model_path: model.onnx
feature_params:
  sampling_freq: 48000
  frame_size_ms: 32
  frame_stride_ms: 16
  window_type: hanning
inference:
  gpu: false
  engine_num_threads: 1
  model_type: denoiser_discr_web_model
```

`model_path` is resolved next to the configuration file. `window_type` is `hanning` or `hamming`.
`model_type` is one of:

| Model | `model_type` | Engines |
| --- | --- | --- |
| 8 kHz quantized | `denoiser_discr_hq_model` | ONNX |
| Discriminative 48 kHz | `denoiser_discr_web_model` | ONNX and TFLite |
| Hybrid 48 kHz | `denoiser_hybrid_web_model` | ONNX and TFLite |

The 8 kHz quantized config sets `sampling_freq: 8000` and may set `skip_zeroed_chunks: true`.
Trained hybrid weights are not in the repository. The roles and package ids are in
[Models](../models.md).

## Packed models

A `.ges` file is a little-endian package: `uint32` configuration size, YAML bytes, `uint32` model
size, then the model bytes. The file ends at the model payload. `PackedModelReader` accepts a path
or a memory buffer and exposes `config()` and `model()`. Pass `std::make_shared<PackedModelReader>(...)`
to `Denoiser`.

## Return codes

`GigaEdgeSpenchRetCode` is an unscoped enum. `SUCCESS` is `0`. Check `code < SUCCESS` in C and
`code < GigaEdgeSpenchRetCode::SUCCESS` in C++.

`CONTEXT_ACCUMULATION` (`1`), `RESAMPLING_INPUT_BUFFER_ACCUMULATION` (`2`), and
`RESAMPLING_OUTPUT_BUFFER_ACCUMULATION` (`3`) mean the call succeeded and the output may be empty
while the model or a resampler fills its window. Negative values are failures, including a bad
configuration path, a model that the selected engine cannot start, an allocation failure, and a
resample or inference error. The names are in `spench/errors.h`.

## C API ownership

`GigaEdgeSpenchDenoiseInit` and `GigaEdgeSpenchDenoiseCreateContext` write handles through pointer
arguments. `GigaEdgeSpenchDenoiseProcessChunkFp` and `GigaEdgeSpenchDenoiseGetFinalChunkFp` allocate
the output buffer. `output_size` is a byte count. `free` that buffer after every call, including a
call that returns an accumulation code. `GigaEdgeSpenchDenoiseDestroyContext` runs before
`GigaEdgeSpenchDenoiseFree`. The `Int` variants convert PCM16 and follow the same ownership.

## Normalization

`AudioProcessor` runs the DSP chain. The default constructor uses built-in settings. A YAML path or
YAML string reads `sound_processing`:

- `loudness_ctrl` — target loudness, maximum gain, bypass time
- `range_ctrl` — leveler attack, hold, and release
- `auto_gain_ctrl` — target level and rise/fall steps
- `noise_gate_ctrl` — threshold and envelope times
- `dynamic_range_compressor_ctrl` — threshold, ratio, knee, and envelope times

Each block carries the sample rate of the buffers passed to that processor. `ProcessChunk` and
`GetFinalChunk` use the same success and accumulation codes as the denoiser. Build this object
before the callback. The DSP demo is `core/cli/demo/3-cpp_dsp_api_demo.cpp`.

## Acoustic echo cancellation

`EchoCanceller` is available in the native SDK. Construct it from YAML. The `echo_cancellation`
section sets `sample_rate`, `filter_len`, `chunk_len_ms`, `residual_echo_suppression_enabled`,
`delay_compensator_enabled`, and `type`. The supported type value in the current configuration is
`speex`.

Pass the microphone and the far-end loopback as two mono float vectors, or pass one interleaved
chunk. `GetFinalChunk` is unused and returns success. Android and iOS feature APIs do not expose
this stage; see the [capability matrix](../capabilities.md).
