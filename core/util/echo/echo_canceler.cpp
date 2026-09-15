#include "util/echo/echo_canceler.h"

#include <cassert>
#include <memory>

#include "util/echo/speex_echo_canceler.h"

namespace util::echo {

EchoCancelerBuilder::EchoCancelerBuilder(const EchoCancelerConfig& config) : cfg_{config} {}

EchoCancelerBuilder& EchoCancelerBuilder::set_sample_rate(uint32_t sample_rate) {
  cfg_.sample_rate = sample_rate;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_type(EchoCancelerType type) {
  cfg_.type = type;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_mic_channels(uint8_t num) {
  cfg_.num_mic = num;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_lbk_channels(uint8_t num) {
  cfg_.num_lbk = num;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_filter_len(uint16_t len) {
  cfg_.filter_len = len;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_frame_len(uint16_t len) {
  cfg_.frame_len = len;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_delay_compensator(bool enabled) {
  cfg_.delay_compensator = enabled;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_residual_echo_suppression(bool enabled) {
  cfg_.echo_suppression = enabled;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::set_echo_premute(float premute_sec) {
  cfg_.echo_premute = premute_sec;
  return *this;
}

std::unique_ptr<IEchoCanceler> EchoCancelerBuilder::create() {
  // TODO(yulis): add more config checks

  if (cfg_.type == EchoCancelerType::speex) {
    return std::make_unique<SpeexEchoCanceler>(cfg_);
  }

  return nullptr;
}

}  // namespace util::echo
