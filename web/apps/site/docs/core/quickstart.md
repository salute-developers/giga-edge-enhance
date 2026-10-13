---
title: Quickstart
---

# C and C++ quickstart

This page processes one mono stream with the discriminative 48 kHz model. The same calls are implemented
with file I/O in the repository demos:

- [`core/cli/demo/1-cpp_denoiser_api_demo.cpp`](https://github.com/salute-developers/gigadenoiser/blob/master/core/cli/demo/1-cpp_denoiser_api_demo.cpp)
- [`core/cli/demo/2-c_denoiser_api_demo.c`](https://github.com/salute-developers/gigadenoiser/blob/master/core/cli/demo/2-c_denoiser_api_demo.c)
- [`core/cli/demo/3-cpp_dsp_api_demo.cpp`](https://github.com/salute-developers/gigadenoiser/blob/master/core/cli/demo/3-cpp_dsp_api_demo.cpp)

Link `gigaedge-spench::gigaedge-spench` and include only the public headers. Create the denoiser and
its context before entering a real-time callback.

## Model configuration

`models/quality48/onnx/config.yaml` selects the packaged ONNX model and its 48 kHz contract. Keep
`model.onnx` beside that file, or change `model_path`. A packed `.ges` file can be loaded with
`PackedModelReader` instead of a YAML path.

Create the context with the sample rate of the buffers you pass in. Use `0, 0` only when those
buffers are already at the model rate and no resampling is required.

## C++

```cpp
#include <spench.h>

#include <iostream>
#include <vector>

int main() {
  gigaedge::spench::Denoiser denoiser("models/quality48/onnx/config.yaml");
  gigaedge::spench::Context<float>* context = denoiser.CreateContext(48000, 48000);

  std::vector<float> input;  // Replace with one mono chunk in [-1, 1].
  std::vector<float> output;
  auto code = denoiser.ProcessChunk(input, output, context);
  if (code < GigaEdgeSpenchRetCode::SUCCESS) {
    std::cerr << "ProcessChunk failed: " << static_cast<int>(code) << '\n';
    denoiser.DestroyContext(&context);
    return 1;
  }

  // Flush samples retained for overlap-add. output is replaced, not appended.
  code = denoiser.GetFinalChunk(output, context);
  denoiser.DestroyContext(&context);
  return code < GigaEdgeSpenchRetCode::SUCCESS ? 1 : 0;
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

#include <spench/c_api.h>

int main(void) {
  GigaEdgeSpenchDenoise denoiser = NULL;
  GigaEdgeSpenchContext context = NULL;
  GigaEdgeSpenchRetCode code =
      GigaEdgeSpenchDenoiseInit("models/quality48/onnx/config.yaml", &denoiser);
  if (code < SUCCESS) {
    return 1;
  }

  code = GigaEdgeSpenchDenoiseCreateContext(denoiser, &context, 48000, 48000);
  if (code < SUCCESS) {
    GigaEdgeSpenchDenoiseFree(&denoiser);
    return 1;
  }

  const float input[] = {0.0f};
  uint8_t* output = NULL;
  uint64_t output_size = 0;
  code = GigaEdgeSpenchDenoiseProcessChunkFp(
      denoiser, context, (const uint8_t*)input, sizeof(input), &output, &output_size);
  free(output);

  output = NULL;
  output_size = 0;
  if (code >= SUCCESS) {
    code = GigaEdgeSpenchDenoiseGetFinalChunkFp(denoiser, context, &output, &output_size);
    free(output);
  }

  GigaEdgeSpenchDenoiseDestroyContext(denoiser, &context);
  GigaEdgeSpenchDenoiseFree(&denoiser);
  return code < SUCCESS ? 1 : 0;
}
```

`output_size` is a byte count. `ProcessChunkFp` allocates `output`; free it after every call,
including the final chunk. Do not free the denoiser while a context is still alive.

## Check the return code

Negative codes are failures. `SUCCESS` is zero. Positive codes report that processing succeeded but
the model or resampler is still accumulating input, so the corresponding output may be empty. The
enumerators are in `spench/errors.h`.

## DSP and echo cancellation

Normalization and acoustic echo cancellation use `AudioProcessor` and `EchoCanceller` from
`<spench.h>`. The DSP demo builds a loudness, noise-gate, and limiter chain before denoising.
Configuration fields are described in [Configuration](configuration.md).
