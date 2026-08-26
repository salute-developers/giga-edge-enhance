#pragma once

namespace spench {
// Denoiser defaults
#ifdef BUILD_TFLITE_ENGINE
static constexpr char default_denoiser_config_yaml[] = R"(
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
  skip_zeroed_chunks: false
  engine_num_threads: 1
  model_type:        best_speed
)";

#else
static constexpr char default_denoiser_config_yaml[] = R"(
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
  skip_zeroed_chunks: false
  engine_num_threads: 1
  model_type:        best_speed
)";

#endif

// AEC defaults (as above)
static constexpr char default_aec_config_yaml[] = R"(
echo_cancellation:
  sample_rate:                       48000
  filter_len:                        4096
  chunk_len_ms:                      16
  residual_echo_suppression_enabled: true
  delay_compensator_enabled:         true
  echo_premute_ms:                   3
  type:                              speex
)";

// Normalizer defaults
static constexpr char default_audio_processing_config_yaml[] = R"(
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
    attack_time: 0.1
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
    bypass_time: 0.1
    max_gain_db: 30
    use_vad: false
    post_processing: true
  limiter_ctrl:
    sample_rate: 48000
    threshold: -3.0
    knee_width: 0.0
    attack_time: 0.0
    release_time: 0.2
)";

}  // namespace spench
