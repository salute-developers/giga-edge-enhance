#include "spench/util/echo/echo_canceler.h"

#include <cassert>
#include <memory>

namespace util::echo {

EchoCancelerBuilder::EchoCancelerBuilder(const EchoCancelerConfig& config) : m_cfg{config} {}

EchoCancelerBuilder& EchoCancelerBuilder::SetSampleRate(uint32_t sample_rate) {
  m_cfg.sample_rate = sample_rate;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetType(EchoCancelerType type) {
  m_cfg.type = type;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetMicChannels(uint8_t num) {
  m_cfg.num_mic = num;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetLbkChannels(uint8_t num) {
  m_cfg.num_lbk = num;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetFilterLen(uint16_t len) {
  m_cfg.filter_len = len;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetFrameLen(uint16_t len) {
  m_cfg.frame_len = len;
  return *this;
}

std::unique_ptr<IEchoCanceler> EchoCancelerBuilder::Create() {
  // TODO: add more config checks

  // TODO: add implementations
  //   if (cfg_.type == EchoCancelerType::SPEEX) {
  //     return std::move(std::make_unique<SpeexEchoCanceler>(cfg_));
  //   }

  return nullptr;
}

}  // namespace util::echo
