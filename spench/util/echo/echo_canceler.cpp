#include "spench/util/echo/echo_canceler.h"

#include <cassert>
#include <memory>

#include "spench/util/echo/speex_echo_canceler.h"

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

EchoCancelerBuilder& EchoCancelerBuilder::SetDelayCompensator(bool enabled) {
  m_cfg.delay_compensator = enabled;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetResidualEchoSuppression(bool enabled) {
  m_cfg.echo_suppression = enabled;
  return *this;
}

EchoCancelerBuilder& EchoCancelerBuilder::SetEchoPreMute(float premute_sec) {
  m_cfg.echo_premute = premute_sec;
  return *this;
}

std::unique_ptr<IEchoCanceler> EchoCancelerBuilder::Create() {
  // TODO(yulis): add more config checks

  if (m_cfg.type == EchoCancelerType::SPEEX) {
    return std::make_unique<SpeexEchoCanceler>(m_cfg);
  }

  return nullptr;
}

}  // namespace util::echo
