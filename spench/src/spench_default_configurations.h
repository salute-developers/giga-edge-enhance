#pragma once

namespace spench {
// Denoiser defaults
#ifdef BUILD_TFLITE_ENGINE
static constexpr char DefaultDenoiserConfigYAML[] = R"(
model:
  model_path: ""
  model_inputs:  ["noisy_mag", "fb_h", "fb_c", "prev_fb_out"]
  model_outputs: ["fb_h", "fb_c", "updated_fb_out", "out_mask"]
feature_params:
  sampling_freq:    24000
  frame_size_ms:    32
  frame_stride_ms:  16
  window_type:      hanning
inference:
  gpu:                false
  engine_num_threads: 1
  model_type:        best_speed
)";

static constexpr char DefaultDereverbConfigYAML[] = R"(
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
)";

#else
static constexpr char DefaultDenoiserConfigYAML[] = R"(
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
)";

// Dereverb defaults
static constexpr char DefaultDereverbConfigYAML[] = R"(
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
)";
#endif

// AEC defaults (as above)
static constexpr char DefaultAecConfigYAML[] = R"(
echo_cancellation:
  sample_rate:                       48000
  filter_len:                        4096
  chunk_len_ms:                      16
  residual_echo_suppression_enabled: true
  delay_compensator_enabled:         true
  type:                              speex
)";

// Normalizer defaults
static constexpr char DefaultNormalizerConfigYAML[] = R"(
normalization:
)";

}  // namespace spench

