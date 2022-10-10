#pragma once
#include <string>
#include <vector>
#include <memory>
#include "contrib/ctpl/ctpl_stl.h"
#include "contrib/yaml-cpp/yaml.h"
#include "spench/src/feature_params.h"
#include "spench/src/task.h"
#include "spench/src/feature_extractor.h"

namespace spench {

class FeatureExtractorRunner /*: public ASRDAGRunner*/ {
 public:
  FeatureExtractorRunner() = delete;
  FeatureExtractorRunner& operator=(const FeatureExtractorRunner&) = delete;
  FeatureExtractorRunner(const FeatureExtractorRunner&) = delete;

  explicit FeatureExtractorRunner(const YAML::Node& yaml_config) {
    ReadConfig(yaml_config);
    m_feature_extractor_ptr =
          std::unique_ptr<spench::fe::FeatureExtractor<float>>(new spench::fe::FeatureExtractor<float>(m_params));
    WarmUp();
  }
  void ReadConfig(const YAML::Node& yaml_config);

  ~FeatureExtractorRunner() /*override*/ = default;

  void Run(spench::DenoiseTask* denoise_task) /*override*/;
  void WarmUp();

  [[nodiscard]] uint64_t GetFrameSizeMs() const;
  [[nodiscard]] uint64_t GetSamplingFreq() const;
  [[nodiscard]] uint64_t GetFrameSizeSamples() const;
  [[nodiscard]] uint64_t GetFrameHopSizeSamples() const;
  [[nodiscard]] uint64_t GetPowSpecFeatSizeSamples() const;

 private:
  spench::fe::FeatureParams m_params;
  std::unique_ptr<spench::fe::FeatureExtractor<float>> m_feature_extractor_ptr;
};
}  // namespace spench
