#include "src/feature_params.h"

#include <memory>
#include <stdexcept>

#include "src/config_reader.h"

namespace gigaedge::enhance {
namespace fe {
void FeatureParams::read_feature_params_node(std::shared_ptr<gigaedge::enhance::NetworkConfig> config) {
  sampling_freq = config->sampling_freq();
  frame_size_ms = config->frame_size_ms();
  frame_stride_ms = config->frame_stride_ms();
  window_type = string_to_window_type_enum(config->window_type());
}

WindowType FeatureParams::string_to_window_type_enum(const std::string& window) {
  if (window == "hanning") {
    return WindowType::hanning;
  }
  if (window == "hamming") {
    return WindowType::hamming;
  }
  throw std::invalid_argument("Window type must be in [\"hanning\", \"hamming\"]");
}

}  // namespace fe
}  // namespace gigaedge::enhance
