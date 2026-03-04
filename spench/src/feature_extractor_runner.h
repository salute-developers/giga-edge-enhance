#pragma once
#include <string>
#include <vector>
#include <memory>

#include "contrib/ctpl/ctpl_stl.h"
#include "spench/src/feature_params.h"
#include "spench/src/spench_task.h"
#include "spench/src/feature_extractor.h"

namespace spench {

class FeatureExtractorRunner {
 public:
  FeatureExtractorRunner() = delete;
  FeatureExtractorRunner& operator=(const FeatureExtractorRunner&) = delete;
  FeatureExtractorRunner(const FeatureExtractorRunner&) = delete;

  explicit FeatureExtractorRunner(std::shared_ptr<spench::NetworkConfig> config) {
    ReadConfig(config);
    m_feature_extractor_ptr =
          std::unique_ptr<spench::fe::FeatureExtractor<float>>(new spench::fe::FeatureExtractor<float>(m_params));
    WarmUp();
  }
  void ReadConfig(std::shared_ptr<spench::NetworkConfig> config);

  ~FeatureExtractorRunner() /*override*/ = default;

  void Run(spench::SpenchTask* denoise_task) /*override*/;
  void WarmUp();

  [[nodiscard]] uint64_t GetFrameSizeMs() const;
  [[nodiscard]] uint64_t GetSamplingFreq() const;
  [[nodiscard]] uint64_t GetFrameSizeSamples() const;
  [[nodiscard]] uint64_t GetFrameHopSizeSamples() const;
  [[nodiscard]] uint64_t GetPowSpecFeatSizeSamples() const;

 private:
  // Handles PipelineStage::PREDICTED_MASK.
  void HandlePredictedMask(spench::SpenchTask* task);

  // Concatenates frames from input_spectrogram into a single ComplexNumber (frames laid out sequentially).
  static spench::ComplexNumber<float>
  MergeInputSpectrogram(const std::vector<spench::ComplexNumber<float>>& frames);

  // Applies the predicted mask / enhancement in place.
  void CalcEnhancedSpectrogram(const spench::ComplexNumber<float>& merged_spec,
                               spench::ComplexNumber<float>& out_new_spec);

  // Validates new_spectrogram dimensions and returns feat_num (bins per frame).
  static uint32_t ValidateAndGetFeatNum(uint32_t input_chunk_num,
                                       const spench::ComplexNumber<float>& new_spec);

  static bool HasPrevSpectrogram(const spench::SpenchContext<float>& ctx);

  // Extracts a single frame (frame_idx) from a concatenated spectrogram.
  static spench::ComplexNumber<float>
  ExtractFrame(const spench::ComplexNumber<float>& spec,
               uint32_t feat_num,
               uint32_t frame_idx);

  // Builds concat spectrogram: [prev][new_0..new_{n-1}]
  static spench::ComplexNumber<float>
  BuildConcatWithPrev(const spench::ComplexNumber<float>& prev_spec,
                      const spench::ComplexNumber<float>& new_spec,
                      uint32_t input_chunk_num,
                      uint32_t feat_num);

  static void UpdatePrevFromNewSpec(spench::SpenchContext<float>& ctx,
                                   const spench::ComplexNumber<float>& new_spec,
                                   uint32_t input_chunk_num,
                                   uint32_t feat_num);

  spench::fe::FeatureParams m_params;
  std::unique_ptr<spench::fe::FeatureExtractor<float>> m_feature_extractor_ptr;
};
}  // namespace spench
