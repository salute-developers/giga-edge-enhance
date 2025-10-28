#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace util::echo {

enum class EchoCancelerType {
  SPEEX  ///< default speex implementation
};

struct EchoCancelerConfig {
  uint32_t sample_rate{16000};  ///< input/output sample rate (Hz)
  uint8_t num_mic{1};           ///< num microphone channels
  uint8_t num_lbk{1};           ///< num loopback channels
  uint16_t filter_len{4096};    ///< internal AEC filter length (samples)
  uint16_t frame_len{512};      ///< process frame len (samples)
  EchoCancelerType type{EchoCancelerType::SPEEX};
  bool delay_compensator{false};  ///< delay compensator between microphone and speaker
  bool echo_suppression{false};   ///< residual echo suppression status
  float echo_premute{0};          ///< mute first seconds of echo signal in mic (waiting AEC adaptation)
};

class IEchoCanceler {
 public:
  virtual ~IEchoCanceler() = default;

  // mic - multichannel mic frame [num_mic * frame_len], interleaved
  // lbk - multichannel lbk frame [num_lbk * frame_len], interleaved
  // out - result frame [frame_len]
  virtual std::vector<float> Process(const std::vector<float>& mic, const std::vector<float>& lbk) {
    std::vector<float> out(this->Config().frame_len);
    this->Process(mic, lbk, out);
    return out;
  }

  virtual void Process(const std::vector<float>& mic, const std::vector<float>& lbk, std::vector<float>& out) = 0;

  // current config
  virtual const EchoCancelerConfig& Config() const noexcept = 0;

  // delay (samples) between input and output
  virtual int DelayLen() const noexcept { return 0; }

  // control filter adaptation
  virtual bool LockFilterSupport() const noexcept { return false; }
  virtual void LockFilter() { /*nothing to do*/ }
  virtual void UnlockFilter() { /*nothing to do*/ }
  virtual bool LockedFilter() const noexcept { return false; }
};

class EchoCancelerBuilder {
 public:
  EchoCancelerBuilder() = default;
  explicit EchoCancelerBuilder(const EchoCancelerConfig& config);
  EchoCancelerBuilder& SetSampleRate(uint32_t sample_rate);
  EchoCancelerBuilder& SetType(EchoCancelerType type);
  EchoCancelerBuilder& SetMicChannels(uint8_t num);
  EchoCancelerBuilder& SetLbkChannels(uint8_t num);
  EchoCancelerBuilder& SetFilterLen(uint16_t len);
  EchoCancelerBuilder& SetFrameLen(uint16_t len);
  EchoCancelerBuilder& SetDelayCompensator(bool enabled);
  EchoCancelerBuilder& SetResidualEchoSuppression(bool enabled);
  EchoCancelerBuilder& SetEchoPreMute(float premute_sec);
  std::unique_ptr<IEchoCanceler> Create();

 private:
  EchoCancelerConfig m_cfg;
};

}  // namespace util::echo
