---
title: Quickstart
---

# C and C++ quickstart

This page processes one mono stream with the discriminative 48 kHz model. The same calls are implemented
with file I/O in the repository demos:

- [`core/cli/demo/1-cpp_denoiser_api_demo.cpp`](https://github.com/salute-developers/gigadenoiser/blob/master/core/cli/demo/1-cpp_denoiser_api_demo.cpp)
- [`core/cli/demo/2-c_denoiser_api_demo.c`](https://github.com/salute-developers/gigadenoiser/blob/master/core/cli/demo/2-c_denoiser_api_demo.c)
- [`core/cli/demo/3-cpp_dsp_api_demo.cpp`](https://github.com/salute-developers/gigadenoiser/blob/master/core/cli/demo/3-cpp_dsp_api_demo.cpp)

Link `giga-edge::enhance` and include only the public headers. Create the denoiser and
its context before entering a real-time callback.

## Model configuration

`models/quality48/onnx/config.yaml` selects the packaged ONNX model and its 48 kHz contract. Keep
`model.onnx` beside that file, or change `model_path`. A packed `.enhance` file can be loaded with
`PackedModelReader` instead of a YAML path.

Create the context with the sample rate of the buffers you pass in. Use `0, 0` only when those
buffers are already at the model rate and no resampling is required.

## C++

```cpp
#include <giga-edge-enhance.h>

#include <iostream>
#include <vector>

int main() {
  giga_edge::enhance::Denoiser denoiser("models/quality48/onnx/config.yaml");
  giga_edge::enhance::Context<float>* context = denoiser.CreateContext(48000, 48000);

  std::vector<float> input;  // Replace with one mono chunk in [-1, 1].
  std::vector<float> output;
  auto code = denoiser.ProcessChunk(input, output, context);
  if (code < GigaEdgeEnhanceRetCode::SUCCESS) {
    std::cerr << "ProcessChunk failed: " << static_cast<int>(code) << '\n';
    denoiser.DestroyContext(&context);
    return 1;
  }

  // Flush samples retained for overlap-add. output is replaced, not appended.
  code = denoiser.GetFinalChunk(output, context);
  denoiser.DestroyContext(&context);
  return code < GigaEdgeEnhanceRetCode::SUCCESS ? 1 : 0;
}
```

Repeat `ProcessChunk` for each input chunk. A return code below `SUCCESS` is an error. `SUCCESS` and
the positive accumulation codes mean the call completed; the output can still be empty or shorter
than the input while the model fills its overlap. Append each non-empty output, then append the
vector returned by `GetFinalChunk` at the end of the stream.

## C

```c
#include <stdint.h>
#include <stdlib.h>

#include <enhance/c_api.h>

int main(void) {
  GigaEdgeEnhanceDenoise denoiser = NULL;
  GigaEdgeEnhanceContext context = NULL;
  GigaEdgeEnhanceRetCode code =
      GigaEdgeEnhanceDenoiseInit("models/quality48/onnx/config.yaml", &denoiser);
  if (code < SUCCESS) {
    return 1;
  }

  code = GigaEdgeEnhanceDenoiseCreateContext(denoiser, &context, 48000, 48000);
  if (code < SUCCESS) {
    GigaEdgeEnhanceDenoiseFree(&denoiser);
    return 1;
  }

  const float input[] = {0.0f};
  uint8_t* output = NULL;
  uint64_t output_size = 0;
  code = GigaEdgeEnhanceDenoiseProcessChunkFp(
      denoiser, context, (const uint8_t*)input, sizeof(input), &output, &output_size);
  free(output);

  output = NULL;
  output_size = 0;
  if (code >= SUCCESS) {
    code = GigaEdgeEnhanceDenoiseGetFinalChunkFp(denoiser, context, &output, &output_size);
    free(output);
  }

  GigaEdgeEnhanceDenoiseDestroyContext(denoiser, &context);
  GigaEdgeEnhanceDenoiseFree(&denoiser);
  return code < SUCCESS ? 1 : 0;
}
```

`output_size` is a byte count. `ProcessChunkFp` allocates `output`; free it after every call,
including the final chunk. Do not free the denoiser while a context is still alive.

## Check the return code

Negative codes are failures. `SUCCESS` is zero. Positive codes report that processing succeeded but
the model or resampler is still accumulating input, so the corresponding output may be empty. The
enumerators are in `enhance/errors.h`.

## DSP and echo cancellation

Normalization and acoustic echo cancellation use `AudioProcessor` and `EchoCanceller` from
`<giga-edge-enhance.h>`. The DSP demo builds a loudness, noise-gate, and limiter chain before denoising.
Configuration fields are described in [Configuration](configuration.md).
