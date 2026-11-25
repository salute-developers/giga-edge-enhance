#pragma once
#include <ctpl_stl.h>

#include <memory>
#include <string>
#include <vector>

#include "src/feature_extractor.h"
#include "src/feature_params.h"
#include "src/task.h"

namespace giga_edge::enhance {

class FeatureExtractorRunner {
 public:
  FeatureExtractorRunner() = delete;
  FeatureExtractorRunner& operator=(const FeatureExtractorRunner&) = delete;
  FeatureExtractorRunner(const FeatureExtractorRunner&) = delete;

  explicit FeatureExtractorRunner(std::shared_ptr<giga_edge::enhance::NetworkConfig> config) {
    read_config(config);
    feature_extractor_ptr_ = std::unique_ptr<giga_edge::enhance::fe::FeatureExtractor<float>>(
        new giga_edge::enhance::fe::FeatureExtractor<float>(params_));
    warm_up();
  }
  void read_config(std::shared_ptr<giga_edge::enhance::NetworkConfig> config);

  ~FeatureExtractorRunner() /*override*/ = default;

  void run(giga_edge::enhance::Task* denoise_task) /*override*/;
  void warm_up();

  [[nodiscard]] uint64_t get_frame_size_ms() const;
  [[nodiscard]] uint64_t get_sampling_freq() const;
  [[nodiscard]] uint64_t get_frame_size_samples() const;
  [[nodiscard]] uint64_t get_frame_hop_size_samples() const;
  [[nodiscard]] uint64_t get_pow_spec_feat_size_samples() const;

 private:
  void handle_waveform(giga_edge::enhance::Task* task);
  void handle_model_output(giga_edge::enhance::Task* task);
  void fill_warming_up_output(giga_edge::enhance::Task* task);

  // Concatenates frames from input_spectrogram into a single ComplexNumber (frames laid out sequentially).
  static giga_edge::enhance::ComplexNumber<float> merge_input_spectrogram(
      const std::vector<giga_edge::enhance::ComplexNumber<float>>& frames);

  // Applies the predicted mask / enhancement in place.
  void calc_enhanced_spectrogram(const giga_edge::enhance::ComplexNumber<float>& merged_spec,
                                 giga_edge::enhance::ComplexNumber<float>& out_new_spec);

  // Validates new_spectrogram dimensions and returns feat_num (bins per frame).
  static uint32_t validate_and_get_feat_num(uint32_t input_chunk_num,
                                            const giga_edge::enhance::ComplexNumber<float>& new_spec);

  static bool has_prev_spectrogram(const giga_edge::enhance::Context<float>& ctx);

  // Builds concat spectrogram: [prev][new_0..new_{n-1}]
  static giga_edge::enhance::ComplexNumber<float> build_concat_with_prev(
      const giga_edge::enhance::ComplexNumber<float>& prev_spec,
      const giga_edge::enhance::ComplexNumber<float>& new_spec, uint32_t input_chunk_num, uint32_t feat_num);

  static void update_prev_from_new_spec(giga_edge::enhance::Context<float>& ctx,
                                        const giga_edge::enhance::ComplexNumber<float>& new_spec,
                                        uint32_t input_chunk_num, uint32_t feat_num);

  bool model_output_is_spectrum_ = false;
  bool use_complex_mask_ = false;
  giga_edge::enhance::fe::FeatureParams params_;
  std::unique_ptr<giga_edge::enhance::fe::FeatureExtractor<float>> feature_extractor_ptr_;
};
}  // namespace giga_edge::enhance
