#include "util/sound/voice_activity.h"

#include <dsplib.h>

#include <memory>
#include <string>

#include "util/sound/rnnoise_vad.h"

namespace util::sound {

VadBuilder::VadBuilder(const VadConfig& config) : cfg_{config} {}

VadBuilder& VadBuilder::set_sample_rate(uint32_t sample_rate) {
  cfg_.sample_rate = sample_rate;
  return *this;
}

VadBuilder& VadBuilder::set_type(VadType type) {
  cfg_.type = type;
  return *this;
}

VadBuilder& VadBuilder::set_threshold(float threshold) {
  cfg_.threshold = threshold;
  return *this;
}

VadBuilder& VadBuilder::set_frame_len(uint32_t frame_len) {
  cfg_.frame_len = frame_len;
  return *this;
}

VadBuilder& VadBuilder::set_model_file(const std::string& model_file) {
  cfg_.model_file = model_file;
  return *this;
}

std::shared_ptr<IVad> VadBuilder::create() {
  if (cfg_.type == VadType::SIMPLE) {
    // TODO(vayulis): add implementation
    return nullptr;
  }

  if (cfg_.type == VadType::RNNOISE) {
    return std::make_shared<RNNoiseVad>(cfg_);
  }

  return nullptr;
}

}  // namespace util::sound
