#include "spench/src/feature_params.h"
#include <memory>
#include <stdexcept>
#include "spench/src/spench_config_reader.h"

namespace spench {
namespace fe {
void FeatureParams::ReadFeatureParamsNode(std::shared_ptr<spench::NetworkConfig> config) {
  sampling_freq = config->samplingFreq();
  frame_size_ms = config->frameSizeMs();
  frame_stride_ms = config->frameStrideMs();
  window_type = StringToWindowTypeEnum(config->windowType());
}

WindowType FeatureParams::StringToWindowTypeEnum(const std::string& window) {
  if (window == "hanning") {
    return WindowType::HANNING;
  }
  if (window == "hamming") {
    return WindowType::HAMMING;
  }
  throw std::invalid_argument("Window type must be in [\"hanning\", \"hamming\"]");
}

}  // namespace fe
}  // namespace spench
