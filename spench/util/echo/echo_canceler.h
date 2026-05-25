#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace util::echo {

enum class EchoCancelerType {
  speex  ///< default speex implementation
};

struct EchoCancelerConfig {
  uint32_t sample_rate{16000};  ///< input/output sample rate (Hz)
  uint8_t num_mic{1};           ///< num microphone channels
  uint8_t num_lbk{1};           ///< num loopback channels
  uint16_t filter_len{4096};    ///< internal AEC filter length (samples)
  uint16_t frame_len{512};      ///< process frame len (samples)
  EchoCancelerType type{EchoCancelerType::speex};
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
  virtual std::vector<float> process(const std::vector<float>& mic, const std::vector<float>& lbk) {
    std::vector<float> out(this->config().frame_len);
    this->process(mic, lbk, out);
    return out;
  }

  virtual void process(const std::vector<float>& mic, const std::vector<float>& lbk, std::vector<float>& out) = 0;

  // current config
  virtual const EchoCancelerConfig& config() const noexcept = 0;

  // delay (samples) between input and output
  virtual int delay_len() const noexcept { return 0; }

  // control filter adaptation
  virtual bool lock_filter_support() const noexcept { return false; }
  virtual void lock_filter() { /*nothing to do*/ }
  virtual void unlock_filter() { /*nothing to do*/ }
  virtual bool locked_filter() const noexcept { return false; }
};

class EchoCancelerBuilder {
 public:
  EchoCancelerBuilder() = default;
  explicit EchoCancelerBuilder(const EchoCancelerConfig& config);
  EchoCancelerBuilder& set_sample_rate(uint32_t sample_rate);
  EchoCancelerBuilder& set_type(EchoCancelerType type);
  EchoCancelerBuilder& set_mic_channels(uint8_t num);
  EchoCancelerBuilder& set_lbk_channels(uint8_t num);
  EchoCancelerBuilder& set_filter_len(uint16_t len);
  EchoCancelerBuilder& set_frame_len(uint16_t len);
  EchoCancelerBuilder& set_delay_compensator(bool enabled);
  EchoCancelerBuilder& set_residual_echo_suppression(bool enabled);
  EchoCancelerBuilder& set_echo_premute(float premute_sec);
  std::unique_ptr<IEchoCanceler> create();

 private:
  EchoCancelerConfig cfg_;
};

}  // namespace util::echo
