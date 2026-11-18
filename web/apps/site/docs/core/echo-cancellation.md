---
title: Acoustic echo cancellation
---

# Acoustic echo cancellation (AEC)

AEC reduces the sound from a device's speakers that leaks back into its
microphone, such as the remote participant's voice in a speakerphone call.
The native C++ module, `EchoCanceller`, uses the far-end playback signal as
a reference to estimate and remove its contribution from the microphone signal.

- **Adaptive echo filter** estimates how playback reaches the microphone.
  `filter_len` sets the length of that filter in samples.
- **Delay compensation** helps align the playback reference with the captured
  echo when the playback and capture paths have different delays. Enable it
  with `delay_compensator_enabled`.
- **Residual echo suppression** reduces echo that remains after adaptive
  cancellation. Enable it with `residual_echo_suppression_enabled`.

## Cancel echo and normalize the microphone output

This example configures Speex AEC for 16 kHz mono audio and then applies LUFS
normalization and a limiter to the cleaned microphone signal. Create the
pipeline once per call to reuse the adaptive filter and normalization state.
Use the audio sent to the speakers as the playback reference. Feed the
microphone and playback reference as synchronized mono float PCM frames at
the configured sample rate. Each input vector must contain exactly
`GetFrameLength()` samples (256 in this example). If the audio device delivers
blocks of another size, buffer both streams and extract paired frames of that
length. The native Speex implementation throws on a frame size mismatch.

```cpp
#include <spench.h>

#include <string>
#include <vector>

struct CallAudio {
  gigaedge::spench::EchoCanceller aec{std::string{R"yaml(
echo_cancellation:
  sample_rate: 16000
  filter_len: 1024
  chunk_len_ms: 16
  residual_echo_suppression_enabled: true
  delay_compensator_enabled: true
  type: speex
)yaml"}};
  gigaedge::spench::AudioProcessor levels;
  std::vector<float> cleaned;

  CallAudio() {
    levels.AddLC(16000, /*target LUFS=*/-23.0f, /*bypass seconds=*/3.0f,
                 /*max gain dB=*/12.0f, /*use_vad=*/true)
          .AddLim(16000, /*threshold dB=*/-3.0f);
  }

  GigaEdgeSpenchRetCode process(const std::vector<float>& microphone,
                                const std::vector<float>& playback_reference,
                                std::vector<float>& output) {
    const auto frame_length = aec.GetFrameLength();
    if (microphone.size() != frame_length || playback_reference.size() != frame_length) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    const auto status = aec.ProcessChunk(microphone, playback_reference, cleaned);
    if (status != GigaEdgeSpenchRetCode::SUCCESS) {
      return status;
    }
    return levels.ProcessChunk(cleaned, output);
  }
};
```

For AEC alone, use `cleaned` directly as the output. When combining the stages,
run AEC first so it can compare microphone capture with the playback reference
before the microphone signal is modified by gain control.

See [audio normalization](normalization.md) for LUFS and VAD options, and the
[capability matrix](../capabilities.md) for AEC support by SDK.
