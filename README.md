\mainpage

![](spench_logo.svg "Speech Enhancement Channel")

This is the implementation of speech enhancement library called __spench__.

## Table of Contents

- [Setup](#setup)
- [Models](#models)
- [Building spench and its dependencies](#building-spench-and-its-dependencies)
  - [Building in container](#building-in-container)
  - [Building manually](#building-manually)
- [Configuration restrictions](#configuration-restrictions)
- [How to use spench ?](#how-to-use-spench-)
- [Usage samples](#usage-samples)
  - [Denoising (C++ API)](#denoising-c-api)
  - [Denoising (C API)](#denoising-c-api-1)
  - [Dereverberation (C++ API)](#dereverberation-c-api)
  - [Normalization (C++ API)](#normalization-c-api)
  - [Normalization and Denoising using YAML string configuration (C++ API)](#normalization-and-denoising-using-yaml-string-configuration-c-api)
  - [Normalization (C API)](#normalization-c-api-1)
  - [Acoustic Echo Cancellation (C++ API)](#acoustic-echo-cancellation-c-api)
- [Config example](#config-example)
- [Web SDK](#web-sdk)
- [WASM and Web version](#wasm-and-web-version)
- [Releases](#releases)
- [Base image](#base-image)
- [For developers](#for-developers)
  - [Testing on a platform](#testing-on-a-platform)
  - [Memory leak validation](#memory-leak-validation)
  - [Code Style](#code-style)
  - [Create automatic documentation](#create-automatic-documentation)
- [Spench Demo](#spench-demo)
  - [Overview](#overview)
  - [Build](#build)
  - [Binaries produced](#binaries-produced)
  - [1) `demo_cpp` — Denoiser API Demo](#1-democpp--denoiser-api-demo)
  - [2) `spench-tool` — Multi-tool CLI](#2-spench-tool--multi-tool-cli)
    - [a) Denoise](#a-denoise)
    - [b) Normalize loudness (`norm`)](#b-normalize-loudness-norm)
    - [c) Echo cancellation (`echo`)](#c-echo-cancellation-echo)


## Setup

**Clone the project**

In case some submodule update happens run:

```shell
git submodule update --init --recursive
```

**Install doxygen**

To employ library for automatic documentation the following steps should be fulfilled:


Ubuntu/Debian:
```
apt-get install -y doxygen
```

CentOS/RedHat:
```
yum install -y doxygen
```

MacOS:
```
brew install doxygen
```

**Install pre-commit**

To employ library for pre-commit the following steps should be fulfilled:


Ubuntu/Debian:
```
apt install pre-commit
```

CentOS/RedHat:
```
yum install epel-release
yum install snapd
systemctl enable --now snapd.socket
snap install pre-commit --classic
```

MacOS:
```
brew install pre-commit
```

Windows setup:

We officially support Visual Studio 2022 version 17.

Build should be run from terminal x64 Native Tools Command Prompt for VS 2022.

MacOS and iOS setup:

We officially support Xcode version 15.3.

You may encounter a protobuf library conflict. To resolve it, run:

```
brew uninstall --ignore-dependencies protobuf@28
brew uninstall --ignore-dependencies protobuf@3
brew uninstall --force protobuf
brew cleanup
brew autoremove
brew install protobuf@3
```

## Models
All pretrained models for __spench__ library are stored in model folder. To download them, please paste credentials from
the project team to your .dvc/config file and run:

```
dvc pull
```

or

```
dvc pull -r s3ext
```

The model files .onnx and .tflite could be found in ```./models/denoise/{model}/inference``` folders.
Path to model should be mentioned in configuration file. The examples of configuration we store at ```spench/test/data``` folder.


## Building __spench__ and it's dependencies

### Building manually

The __spench__ project is a CMake based. To compile the library you need to follow the steps below:

```
mkdir build
cd build
cmake ..
make -j$(nproc)
```

There are some options which could be passed for cmake configuration:

```
-DBUILD_ONNX_ENGINE=ON (default) - Build project with onnx
-DBUILD_TFLITE_ENGINE=ON - Build project with tflite
```

```
-DCMAKE_TOOLCHAIN_FILE=<file_path> - NDK path for Android platforms building
-DANDROID_ABI=architecture - CPU architecture
-DANDROID_NATIVE_API_LEVEL=version - Android API version
-DBUILD_JAVA_BIND=OFF (default) - Build Java wrapper library for __spench__
```

```
-DCMAKE_BUILD_TYPE=Debug (default) - Type of build Release, Debug, RelWithDebInfo and MinSizeRel
-DBUILD_SHARED_LIBS=OFF (default) - Build __spench__ and its dependencies as static or shared libraries
-DBUILD_APPLE_ARCH=auto (default) - Target architecture for Apple platforms. Can be set to:
                                    - BUILD_APPLE_ARCH=arm   — to build for Apple Silicon (M1/M2)
                                    - BUILD_APPLE_ARCH=intel — to build for Intel-based Macs
                                    This flag is used during MacOS or iOS builds to explicitly set target CPU architecture.
```

```
-DBUILD_WITH_ADDRESS_SANITIZER=OFF (default) - Build library with or without memory sanitizer
-DBUILD_WITHOUT_SOXR=OFF (default) - Build with or without libsoxr library for audio resampling.
```
Example:

To build __spench__ with tflite:

```
cmake -DBUILD_TFLITE_ENGINE=ON ..
make -j$(nproc)
```

## Configuration restrictions

For MacOS platform, only the following __spench__ configuration is supported:

```
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TFLITE_ENGINE=ON -DBUILD_DEMO=ON ..
make -j$(nproc)
```
Set BUILD_APPLE_ARCH to arm for Apple Silicon (M1/M2), or to intel for Intel-based Mac.

For iOS device platform, only the following __spench__ configuration is supported:

```
cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_ONNX_ENGINE=ON \
-DBUILD_DEMO=ON -DBUILD_APPLE_ARCH=arm -DCMAKE_SYSTEM_NAME=iOS ..
make -j$(nproc)
```

For Android platform, __spench__ could be configured with Java binding library by the preinstalled Android NDK:

```
cmake -DCMAKE_TOOLCHAIN_FILE=/<path_to_ndk>/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=x86_64 -DANDROID_NATIVE_API_LEVEL=29 -DBUILD_JAVA_BIND=ON ..
make -j$(nproc)
```

For Windows platform, __spench__ could be built by the following way:

```
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TFLITE_ENGINE=ON \
-DBUILD_DEMO=OFF -DCPUINFO_BUILD_UNIT_TESTS=OFF ..
msbuild spench.sln /property:Configuration=Release /property:Platform=x64 -m -target:ALL_BUILD
```

## How to use __spench__ ?

Built project provides API header files and set of libraries:

Codes of errors published on:

```
spench/errors.h
```

C++ API could be found here:

```
spench/spench.h
```

C API which implemented as wrapper for C++ API could be found on:

```
spench/spench_c.h
```

The __spench__ has some third party dependencies and all of them are automatically built and should be employed for linking:
```
libspench.a
libutil__onnx.a for onnx or libutil__tflite.a for tflite
libyaml-cpp.a
libonnxruntime.so for onnx or libtensorflow-tflite.so for tflite
```

## Usage samples

---
### Denoising (C++ API)

```cpp
#include "spench/spench.h"

int main() {
    // 1. Create the denoiser from a YAML model config
    spench::SpenchDenoise denoiser("denoise_config.yaml");

    // 2. Create a streaming context (48 kHz I/O)
    // The first argument is the sample rate of your input audio,
    // the second argument is the desired output sample rate.
    // If the input sample rate differs from the one supported by the model
    // (currently 24 kHz), the model will automatically apply a resampler
    // to convert the audio to the required format.

    auto ctx = denoiser.CreateContext(48000, 48000);

    std::vector<float> noisy   = load_wav("noisy.wav");
    std::vector<float> enhanced;

    // 3. Process one frame (or feed in a loop)
    denoiser.ProcessChunk(noisy, enhanced, ctx);

    write_wav(enhanced, 48000, "enhanced.wav");
    denoiser.DestroyContext(&ctx);
    return 0;
}
```

---
### Denoising (C API)

```c
#include <spench/spench_c.h>

int main(int argc, char **argv) {
    SpenchDenoise denoiser;
    SpenchContext ctx;

    SpenchDenoiseInit("denoise_config.yaml", &denoiser);
    // Create context:
    // The first and second arguments are input and output sample rates.
    // Passing zeros means no resampling will be applied — the model will
    // use its default internal sample rate (e.g., 24 kHz).

    SpenchDenoiseCreateContext(denoiser, &ctx, 0, 0);

    float *noisy_frame   = get_frame();
    unsigned char *out   = NULL;
    uint64_t out_size    = 0;

    SpenchDenoiseProcessChunkFp(denoiser, ctx,
                                (unsigned char *)noisy_frame,
                                frame_byte_size,
                                &out, &out_size);

    if (out != NULL) {
        free(out);
        out = NULL;
    }

    SpenchDenoiseDestroyContext(denoiser, &ctx);
}
```

---
### Dereverberation (C++ API)

```cpp
#include "spench/spench.h"

int main() {
    spench::SpenchAudioDereverb dereverb("dereverb_config.yaml");
    auto ctx = dereverb.CreateContext();

    std::vector<float> reverberated;
    load_wav("reverb.wav", reverberated);

    std::vector<float> enhanced;
    dereverb.ProcessChunk(reverberated, enhanced, ctx);

    write_wav(enhanced, dereverb.GetModelSampleRate(), "dereverb.wav");
    dereverb.DestroyContext(&ctx);
}
```

---
### Normalization (C++ API)

```cpp
#include "spench/spench.h"

int main() {
    // Manual configuration in code.
    spench::SpenchAudioProcess dsp1;

    // Build a normalization pipeline:
    // RC  — Range Control: normalizes signal peak and applies optional gain limiting
    // NG  — Noise Gate: attenuates signals below –40 dBFS with attack/release/hold behavior
    // DRC — Dynamic Range Compressor: reduces gain above –15 dBFS based on ratio and timing
    dsp1.AddRC(48000)            // default: target –3 dB, max gain +10 dB
       .AddNG(48000, -40)       // gate below –40 dBFS
       .AddDRC(48000, -15);     // compress above –15 dBFS

    std::vector<float> input1 = load_some_frame();
    std::vector<float> output1;
    dsp1.ProcessChunk(input1, output1);

    // The modules are defined declaratively in a YAML file.
    // sound_processing:
    //   loudness_ctrl: ...
    //   auto_gain_ctrl: ...
    //   limiter_ctrl: ...

    spench::SpenchAudioProcess dsp2("config.yaml");

    std::vector<float> input2 = load_some_frame();
    std::vector<float> output2;
    dsp2.ProcessChunk(input2, output2);
}
```
---
### Normalization and Denoising using YAML string configuration (C++ API)

```cpp
#include "spench/spench.h"

int main() {
  // Define configuration as an inline YAML string.
  // This configuration updates only specific parameters — all others remain default.
  const char yaml_norm[] = R"(
sound_processing:
  noise_gate_ctrl:
    sample_rate: 48000
  dynamic_range_compressor_ctrl:
    sample_rate: 48000
  loudness_ctrl:
    sample_rate: 48000
  limiter_ctrl:
    sample_rate: 48000
)";
  // Define configuration pointing to the model file.
  static constexpr char yaml_denoise[] = R"(
model:
  model_path: data/models/fast_model_24khz.onnx
)";
    // Wrap it into std::string (required by constructor)
    std::string yaml_str(yaml_norm);

    // Create Normalization object from YAML string.
    spench::SpenchAudioProcess normalizer(std::string(yaml_norm));

    normalizer.AddLC().AddDRC().AddNG().AddLim();
    // Create Denoiser object directly from YAML string.
    spench::SpenchDenoise denoiser(std::string(yaml_denoise));
    // Create processing context (48 kHz I/O)
    auto ctx = denoiser.CreateContext(48000, 48000);

    // Load a noisy frame and process it
    std::vector<float> noisy = load_wav("noisy.wav");
    std::vector<float> enhanced;
    // Split noisy to chunks 16 ms
    // For each noisy_chunk in noisy
    {
        std::vector<float> enhanced_chunk;
        // Process audio frame
        normalizer.ProcessChunk(noisy_chunk, noisy_chunk);
        denoiser.ProcessChunk(noisy_chunk, enhanced_chunk, ctx);
        // Add enhanced chunks to enhanced
    }
    // Save the enhanced signal
    write_wav(enhanced, 48000, "enhanced.wav");

    // Clean up
    denoiser.DestroyContext(&ctx);
}
```
---
### Normalization (C API)

```c
#include <spench/spench_c.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
   // Manual configuration in code
   SpenchAudioProcess dsp1 = NULL;
   SpenchAudioProcessInit("config.yaml", &dsp1);

   // Build a normalization pipeline:
   // RC  — Range Control: normalizes signal peak and applies optional gain limiting
   // NG  — Noise Gate: attenuates signals below –40 dBFS with attack/release/hold behavior
   // DRC — Dynamic Range Compressor: reduces gain above –15 dBFS based on ratio and timing
   SpenchAudioProcessAddRc(dsp1);
   SpenchAudioProcessAddNgWithParam(dsp1, 48000, -40.0f, 0.05f, 0.02f, 0.05f);
   SpenchAudioProcessAddDrcWithParam(dsp1, 48000, -15.0f, 5, 0.0f, 0.01f, 0.2f);

   float* input1 = get_frame();  // ← your own data loader
   uint64_t frame_samples = ...; // number of float samples in `input1`

   uint8_t* output1 = NULL;
   uint64_t output_bytes1 = 0;

   SpenchAudioProcessChunkFp(dsp1,
                             (const uint8_t*)input1,
                             frame_samples * sizeof(float),
                             &output1,
                             &output_bytes1);

   // Use output1 as float*:
   float* result1 = (float*)output1;
   uint64_t result_samples1 = output_bytes1 / sizeof(float);

   SpenchAudioProcessFree(dsp1);
   free(output1);

   // The config specifies the parameters of the modules.
   // The modules are not created automatically,
   // they must be added manually and they will search for parameters in the config.
   //
   // sound_processing:
   //   loudness_ctrl: ...
   //   auto_gain_ctrl: ...
   //   limiter_ctrl: ...

   SpenchAudioProcess dsp2 = NULL;
   SpenchAudioProcessInit("config.yaml", &dsp2); // Loads parameters only

   // Add modules explicitly, without parameters — they will use values from the YAML config
   // LC  — Loudness Control: normalizes overall loudness with optional bypass window
   // AGC — Automatic Gain Control: adjusts signal level to target power over time
   // Lim — Limiter: prevents signal from exceeding threshold using peak limiting

   SpenchAudioProcessAddLc(dsp2);    // Loudness Control
   SpenchAudioProcessAddAgc(dsp2);   // Auto Gain Control
   SpenchAudioProcessAddLim(dsp2);   // Limiter

   float* input2 = get_frame();
   uint8_t* output2 = NULL;
   uint64_t output_bytes2 = 0;

   SpenchAudioProcessChunkFp(dsp2,
                             (const uint8_t*)input2,
                             frame_samples * sizeof(float),
                             &output2,
                             &output_bytes2);

   float* result2 = (float*)output2;
   uint64_t result_samples2 = output_bytes2 / sizeof(float);

   SpenchAudioProcessFree(dsp2);
   free(output2);
}
```

---
### Acoustic Echo Cancellation (C++ API)

```cpp
#include "spench/spench.h"

int main() {
   // Initialize echo canceler from YAML config
   spench::SpenchAudioEchoCancelation aec("aec_config.yaml");

   // Capture microphone and loopback audio
   std::vector<float> mic = capture_mic_frame();     // microphone input
   std::vector<float> lbk = capture_loopback();      // loopback (speaker output)

   std::vector<float> clean;

   // Process one frame (or feed in a loop)
   aec.ProcessChunk(mic, lbk, clean);

   write_wav(clean, 48000, "clean.wav");
}
```

## Config example
```yaml
# Denoising
model:
  model_path: ""
  model_inputs:  ["noisy_mag_5", "fb_h.1", "fb_c.1", "prev_fb_out_5"]
  model_outputs: ["fb_h", "fb_c", "new_fb_out_5", "model_out"]
feature_params:
  sampling_freq:    24000
  frame_size_ms:    32
  frame_stride_ms:  16
  window_type:      hanning
inference:
  gpu:                false
  engine_num_threads: 1
  model_type:        best_speed

# Dereverberation
model:
  model_path: ""
  model_inputs:  ["noisy_spec_5"]
  model_outputs: ["model_out", "mu", "logvar"]
feature_params:
  sampling_freq:    48000
  frame_size_ms:    32
  frame_stride_ms:  16
  window_type:      hanning
inference:
  gpu:                false
  engine_num_threads: 1
  model_type:        remove_reverb

# Sound processing
sound_processing:
  range_ctrl:
    sample_rate: 48000
    target_level_db: -3.0
    max_gain_db: 10
    attack_time: 0.0
    release_time: 300
    hold_time: 5
  auto_gain_ctrl:
    target_level: 1
    max_gain: 20.0
    average_len: 6000
    step_size_rise: 0.01
    step_size_fall: 0.01
  noise_gate_ctrl:
    sample_rate: 48000
    threshold: -40.0
    attack_time: 0.01
    release_time: 0.02
    hold_time: 0.05
  dynamic_range_compressor_ctrl:
    sample_rate: 48000
    threshold: -10.0
    ratio: 20
    knee_width: 0.0
    attack_time: 0.01
    release_time: 0.2
  loudness_ctrl:
    sample_rate: 48000
    target_level_db: -15.0
    bypass_time: 3.0
    max_gain_db: 30
  limiter_ctrl:
    sample_rate: 48000
    threshold: -3.0
    knee_width: 0.0
    attack_time: 0.0
    release_time: 0.2

# AEC
echo_cancellation:
  sample_rate:                       48000
  filter_len:                        4096
  chunk_len_ms:                      16
  residual_echo_suppression_enabled: true
  delay_compensator_enabled:         true
  type:                              speex
```

## Web SDK

For easy integration, we provide the **Spench Web SDK**.

**Package name:** `spench-web-sdk`

**Usage example (TypeScript):**


```ts
import { SpenchAudioNode, SpenchError, SpenchSDK } from 'spench-web-sdk';

// You can set the sample rate (SR) of the incoming stream, or pass undefined to use the output device's default SR.
const sampleRate: number | undefined = 24000;

// The context may be created with state === "suspended"; to activate it, call await ctx.resume().
// A user gesture may be required depending on the browser.
// See: https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/state
const ctx = new AudioContext({ sampleRate });

// Normalization is also required for the models to work correctly.
const features = {
  // For now, prefer the 'full24k' model; 'lite24k' is being updated.
  noiseCancellation: 'full24k' as const, // or 'lite24k'
  normalization: true,
} as const;

const sdk = SpenchSDK.create(ctx, {
  loggerVerbosity: 'verbose', // Use 'verbose' if you need more detailed logs
  logger: { info: ..., debug: ..., ... }, // Plug in an external logger if needed
  preloadFeatures: features, // Speeds up loading
  sendMetric: (metric, value) => console.log(metric, value), // Some metrics; safe to ignore during pilots
});

// You can pass features right away or call setFeatures later.
// The latter integrates the node into the graph faster—even if noise-cancellation resources aren't loaded yet.
const spenchNode: SpenchAudioNode | SpenchError = await sdk.createAudioNode();

if (spenchNode instanceof SpenchError) {
  throw spenchNode;
}

// If this event fires, the system is overloaded; disable effects to avoid artifacts.
spenchNode.addEventListener('low_performance', () => {});

// If this event fires, something unexpected broke in the workers; disable the node.
spenchNode.addEventListener('destroyed', () => {});

const callStream: MediaStream; // Input stream
const sourceStreamNode = ctx.createMediaStreamSource(callStream);
const destinationStreamNode = ctx.createMediaStreamDestination();

sourceStreamNode.connect(spenchNode).connect(destinationStreamNode);

const outputStream = destinationStreamNode.stream; // Cleaned stream

const res = await spenchNode.setFeatures(features);
if (res instanceof SpenchError) {
  throw res;
}

// You can reuse the node between calls; just call reset after a finished call.
spenchNode.reset();

// Free resources when done.
spenchNode.destroy();
await ctx.close();
```


## WASM and Web version

WASM modules are built within this repository's CI.
After WASM release (see Releases section) Web SDK CI is then triggered in order to test,
build and release a new SDK version.

[Spench for WASM readme.](./platform_wrappers/wasm/README.md)

## Releases

TODO rewrite

First of all, determine the latest release version of the target binary you'd like to publish. To do this, check out the
link above, click on the target (e.g. spench-wasm) and find the latest version.

To build and publish release distributions you must firstly create a branch with following pattern:

```
release/XX.XXX.XX


## Base image


## For developers

### Testing on a platform
To launch Unit tests for __spench__ just run:

```
make test
```

### Memory leak validation
Project could be built with sanitizer enabled to validate memory issues:

```
cmake -DBUILD_WITH_ADDRESS_SANITIZER=ON ..
make -j$(nproc)
```

### Code Style
Keep the code consistent with the corporate style. For that purposes after new changes have been added, run:

```
./check_style.sh
```

To automatically check the code style before the commit, run:

```
pre-commit install
```

After running ```git commit -m "your commit"``` you will see "Run style check script..." — it means the code style check will run automatically.

Check that __cpplint__ is already installed or install it by the command:

```
pip install cpplint==1.6.0
```

### Create automatic documentation
To create automatic documentation the following step should be fulfilled:

```
doxygen doc/spench.config
```

# Spench Demo

This section describes how to **build** and **run** the command‑line demo for denoising and normalization.

## Overview
The demos showcase two entry points:

1. **`demo_cpp`** — a minimal C++ API example that denoises a WAV file using a Spench model.
2. **`spench-tool`** — a multi‑tool CLI with subcommands for **denoise**, **loudness normalization**, and **echo cancellation**.

Both are located under `spench/demo/`.

---

## Build
Enable demos in CMake and build:

```bash
cmake -S . -B build -DBUILD_DEMO=ON
cmake --build build -j
```

---

## Binaries produced
When `-DBUILD_DEMO=ON` is set, the build generates at least these executables:

- **`demo_cpp`** — from `1-cpp_denoiser_api_demo.cpp`
- **`spench-tool`** — from `spench_cli.cpp` (subcommands: `denoise`, `norm`, `echo`)

---

## 1) `demo_cpp` — Denoiser API Demo
**Source:** `spench/demo/1-cpp_denoiser_api_demo.cpp`

**Synopsis**
```
./demo_cpp [flags] path_to_input_wav output_wav_save_path path_to_model_config
```

**Flags**
- `-h`, `--help` — show usage and exit
- `-r`, `--resample` — allow non-matching input sample rates

**Examples**
```bash
# Input WAV matches model sample rate
./demo_cpp tests/data/wavs/rec_noise_001_24k.wav enhanced.wav tests/data/config/default_configuration_fast_tflite_24khz.yaml

# Input WAV has a different sample rate → enable resampling
./demo_cpp --resample tests/data/wavs/rec_noise_001_48k.wav enhanced.wav tests/data/config/default_configuration_fast_tflite_24khz.yaml
```

**Behavior & notes**
- Validates that input/config files exist and that the output path is not a directory.
- Processing uses the model’s frame size and writes mono output.
- Sample-rate compatibility: if your model performs internal resampling (e.g., via context setup), --resample lets you pass any input rate. If not, the input WAV must match the model sample rate; otherwise results are undefined.

---

## 2) `spench-tool` — Multi‑tool CLI
**Source:** `spench/demo/spench_cli.cpp`

`spench-tool` exposes three subcommands:
- `denoise` — run the Spench denoiser on a file
- `norm` — apply loudness normalization
- `echo` — perform echo cancellation

Use `--help` on the tool or on any subcommand for details.

### a) Denoise
**Synopsis**
```bash
spench-tool denoise -i INPUT.wav -o OUTPUT.wav -c denoiser.yaml [--verbose]
```

**What it does**
- Loads the denoiser with `-c denoiser.yaml`.
- Uses the model’s frame size and sample rate.
- Processes the **first channel only** and writes a **mono** WAV.

**Sample-rate note**
- This build writes the output at the model sample rate (`AudioWriter(..., model_fs, 1, 16)`).
- Resampler objects are constructed in code but not applied here. If your model supports internal sample-rate conversion, it can accept inputs at other rates; the resulting WAV will still be written at `model_fs` unless you change the code.

**Example**
```bash
spench-tool denoise -i audio/noisy.wav -o out/denoised.wav -c models/denoiser.yaml --verbose
```

### b) Normalize loudness (`norm`)
**Synopsis**
```bash
spench-tool norm -i INPUT.wav -o OUTPUT.wav [--target LUFS] [--bypass_time SECONDS]
```

**Options**
- `-i, --input` — input audio file (required)
- `-o, --output` — result audio file (required)
- `--target` — target loudness in LUFS (default: `-20.0`)

**Behavior**
- Writes a **two‑channel** output where **channel 0 = original** and **channel 1 = normalized** (convenient for A/B comparison).

**Note**
- The code references a `--bypass_time` parameter inside the loudness controller call. If your build doesn’t define it in the parser, either add the argument or remove its usage to avoid a runtime error.

**Example**
```bash
spench-tool norm -i audio/voice.wav -o out/voice_norm.wav --target -20 --bypass_time 0.5
```

### c) Echo cancellation (`echo`)
**Synopsis**
```bash
spench-tool echo -i INPUT.wav -o OUTPUT.wav -c aec.yaml [-m CH0 CH1 ...] [--verbose]
```

**Options**
- `-i, --input` — input audio file (required)
- `-o, --output` — result audio file (required)
- `-c, --config` — AEC config file (required)
- `-m, --micmap` — channel map (default: `0 1`; minimum 2 channels, up to 8)
- `--verbose` — print additional information

**Behavior**
- Requires at least **two channels** (mic + reference/speaker).
- Uses frame length, filter length, and sample rate from the AEC object.
- Writes a **mono** WAV at the AEC sample rate (16‑bit).
- **Sample-rate note**: there is no implicit resampling in this CLI path; if your AEC/model supports internal SRC, non-matching inputs may work; otherwise ensure the input matches the AEC sample rate.

**Examples**
```bash
# Use ch0 as mic and ch1 as speaker/ref
spench-tool echo -i audio/meeting.wav -o out/meeting_aec.wav -c cfg/aec.yaml -m 0 1

# Verbose
spench-tool echo -i audio/meeting.wav -o out/meeting_aec.wav -c cfg/aec.yaml -m 0 1 --verbose
```
