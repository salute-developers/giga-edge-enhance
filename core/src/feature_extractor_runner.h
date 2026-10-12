#pragma once
#include <ctpl_stl.h>

#include <memory>
#include <string>
#include <vector>

#include "src/feature_extractor.h"
#include "src/feature_params.h"
#include "src/task.h"

namespace gigaedge::spench {

class FeatureExtractorRunner {
 public:
  FeatureExtractorRunner() = delete;
  FeatureExtractorRunner& operator=(const FeatureExtractorRunner&) = delete;
  FeatureExtractorRunner(const FeatureExtractorRunner&) = delete;

  explicit FeatureExtractorRunner(std::shared_ptr<gigaedge::spench::NetworkConfig> config) {
    read_config(config);
    feature_extractor_ptr_ = std::unique_ptr<gigaedge::spench::fe::FeatureExtractor<float>>(
        new gigaedge::spench::fe::FeatureExtractor<float>(params_));
    warm_up();
  }
  void read_config(std::shared_ptr<gigaedge::spench::NetworkConfig> config);

  ~FeatureExtractorRunner() /*override*/ = default;

  void run(gigaedge::spench::Task* denoise_task) /*override*/;
  void warm_up();

  [[nodiscard]] uint64_t get_frame_size_ms() const;
  [[nodiscard]] uint64_t get_sampling_freq() const;
  [[nodiscard]] uint64_t get_frame_size_samples() const;
  [[nodiscard]] uint64_t get_frame_hop_size_samples() const;
  [[nodiscard]] uint64_t get_pow_spec_feat_size_samples() const;

 private:
  void handle_waveform(gigaedge::spench::Task* task);
  void handle_model_output(gigaedge::spench::Task* task);
  void fill_warming_up_output(gigaedge::spench::Task* task);

  // Concatenates frames from input_spectrogram into a single ComplexNumber (frames laid out sequentially).
  static gigaedge::spench::ComplexNumber<float> merge_input_spectrogram(
      const std::vector<gigaedge::spench::ComplexNumber<float>>& frames);

  // Applies the predicted mask / enhancement in place.
  void calc_enhanced_spectrogram(const gigaedge::spench::ComplexNumber<float>& merged_spec,
                                 gigaedge::spench::ComplexNumber<float>& out_new_spec);

  // Validates new_spectrogram dimensions and returns feat_num (bins per frame).
  static uint32_t validate_and_get_feat_num(uint32_t input_chunk_num,
                                            const gigaedge::spench::ComplexNumber<float>& new_spec);

  static bool has_prev_spectrogram(const gigaedge::spench::Context<float>& ctx);

  // Builds concat spectrogram: [prev][new_0..new_{n-1}]
  static gigaedge::spench::ComplexNumber<float> build_concat_with_prev(
      const gigaedge::spench::ComplexNumber<float>& prev_spec, const gigaedge::spench::ComplexNumber<float>& new_spec,
      uint32_t input_chunk_num, uint32_t feat_num);

  static void update_prev_from_new_spec(gigaedge::spench::Context<float>& ctx,
                                        const gigaedge::spench::ComplexNumber<float>& new_spec,
                                        uint32_t input_chunk_num, uint32_t feat_num);

  bool model_output_is_spectrum_ = false;
  gigaedge::spench::fe::FeatureParams params_;
  std::unique_ptr<gigaedge::spench::fe::FeatureExtractor<float>> feature_extractor_ptr_;
};
}  // namespace gigaedge::spench
