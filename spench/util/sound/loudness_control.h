#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace util::sound {

class ILoudnessControl {
 public:
  virtual ~ILoudnessControl() {}

  enum Method { SIMPLE, VADBASE };

  struct State {
    float current_gain_db_smoothed{0.0f};       // smoothed commanded gain, dB
    std::optional<float> last_short_term_lufs;  // short-term LUFS before applying gain
    float longterm_max_lufs{0.0f};
    float longterm_min_lufs{-80.0f};
    bool bypass_active{false};
  };

  virtual std::vector<float> process(const std::vector<float>& x) = 0;

  virtual State state() const noexcept = 0;

  virtual Method method() const noexcept = 0;
};

/**
 * @brief Detect short-term loudness and normalize to target level
 * @details Accumulates volume statistics, so a reset may be required when the audio path changes
 */
class LoudnessControlBuilder {
 public:
  LoudnessControlBuilder() = default;

  LoudnessControlBuilder& set_method(ILoudnessControl::Method method) {
    m_method = method;
    return *this;
  }

  LoudnessControlBuilder& set_sample_rate(uint32_t sample_rate) {
    m_sample_rate = sample_rate;
    return *this;
  }

  LoudnessControlBuilder& set_target_level(float target_level_db) {
    m_target_level_db = target_level_db;
    return *this;
  }

  LoudnessControlBuilder& set_max_gain(float max_gain_db) {
    m_max_gain_db = max_gain_db;
    return *this;
  }

  LoudnessControlBuilder& set_post_processing(bool enabled) {
    m_postprocessing = enabled;
    return *this;
  }

  LoudnessControlBuilder& set_bypass_time(float bypass_time) {
    m_bypass_time = bypass_time;
    return *this;
  }

  std::unique_ptr<ILoudnessControl> create();

 private:
  ILoudnessControl::Method m_method{ILoudnessControl::SIMPLE};
  std::optional<uint32_t> m_sample_rate;
  std::optional<float> m_target_level_db{-15.0F};
  std::optional<float> m_max_gain_db{30.0F};
  std::optional<bool> m_postprocessing{false};
  std::optional<float> m_bypass_time{0};
};

}  // namespace util::sound
