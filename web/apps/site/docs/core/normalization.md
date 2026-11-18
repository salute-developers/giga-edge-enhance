---
title: Audio normalization
---

# Audio normalization

The native C++ SDK's `AudioProcessor` runs DSP modules in the order you add them.
It can run independently of the denoiser and needs no denoising model.

- **Loudness Control (`AddLC`)** adjusts gain toward a target in LUFS to keep
  perceived loudness consistent across speakers and recordings.
- **Range Control (`AddRC`)** adjusts gain based on estimated peaks to level
  signals against a target peak with configurable attack, hold, and release.
- **Automatic Gain Control (`AddAGC`)** tracks signal power and adjusts gain
  toward a target power as the input level changes.
- **Noise Gate (`AddNG`)** attenuates signals below a threshold to reduce quiet
  background noise between phrases.
- **Dynamic Range Compressor (`AddDRC`)** reduces gain above a threshold to
  narrow the gap between quiet and loud passages.
- **Limiter (`AddLim`)** controls peaks at the end of the chain to help prevent clipping.

Start with one gain control stage. Choose `AddLC` for perceived loudness, `AddRC`
for peak-based leveling, or `AddAGC` to stabilize signal power. Add a gate,
compressor, or limiter where the input or output needs it.

## Normalize speech to a LUFS target

Set `use_vad` to `true` to enable voice activity detection (VAD). This costs
additional CPU time because the processor also runs speech detection. VAD gates
updates to the loudness estimate around detected speech, so noise-only passages
do not drive gain adaptation. The resulting gain still applies to the full audio
stream. VAD is off by default.

```cpp
#include <spench.h>

#include <vector>

// Create once per stream, before processing 48 kHz audio.
struct SpeechNormalizer {
  gigaedge::spench::AudioProcessor speech;

  SpeechNormalizer() {
    speech.AddLC(48000, /*target LUFS=*/-23.0f, /*bypass seconds=*/3.0f,
                 /*max gain dB=*/12.0f, /*use_vad=*/true)
          .AddDRC(48000, /*threshold dB=*/-15.0f, /*ratio=*/3)
          .AddLim(48000, /*threshold dB=*/-3.0f);
  }

  // Call for each new block of mono float PCM in [-1, 1].
  GigaEdgeSpenchRetCode process(const std::vector<float>& input, std::vector<float>& output) {
    return speech.ProcessChunk(input, output);
  }
};
```

The `-23 LUFS` target applies to the loudness control stage. The gain cap,
compressor, and limiter can all affect the final output loudness. To use the
loudness module's built-in compressor and limiter,
pass `true` as the sixth argument (`post_processing`) to `AddLC` and omit the
separate `AddDRC` and `AddLim` calls.

## Gate quiet input before automatic gain control

```cpp
#include <spench.h>

gigaedge::spench::AudioProcessor levels;
levels.AddNG(48000, /*threshold dB=*/-45.0f)
      .AddAGC(48000)
      .AddLim(48000, /*threshold dB=*/-3.0f);
```

The gate attenuates quiet background audio before gain control, and the limiter
controls the resulting peaks. For either chain, pass each mono PCM block to
`ProcessChunk(input, output)` on the same processor instance so the modules
retain their state. Set each module's sample rate to the rate of those blocks.
See the [native configuration guide](configuration.md#normalization)
for YAML options.
