#pragma once

#include <memory>
#include <vector>

namespace util::sound {

class LoudnessMeter;
class LoudnessStat;

/**
 * @brief Detect short-term loudness and normalize to target level
 * @details Accumulates volume statistics, so a reset may be required when the audio path changes
 */
class LoudnessControl {
 public:
  /**
   * @param sample_rate signal sample rate (Hz)
   * @param target_level target level in LUFS (dB)
   * @param bypass_time time (sec) to skip gain apply
   * @param max_gain max gain limit (db)
   */
  explicit LoudnessControl(int32_t sample_rate, float target_level = -23, float bypass_time = 3.0, float max_gain = 40);

  [[nodiscard]] std::vector<float> process(const std::vector<float>& x);

  [[nodiscard]] float current_gain() const noexcept { return m_gain; }

  void reset();

 private:
  std::shared_ptr<LoudnessMeter> m_mtr;
  std::shared_ptr<LoudnessStat> m_stat;
  int64_t m_bypass_counter{0};
  const float m_target_db;
  const float m_max_gain_db;
  float m_gain_db{0.0F};
  float m_gain{1.0};
};

}  // namespace util::sound
