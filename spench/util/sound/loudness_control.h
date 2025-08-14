#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <cstdint>

namespace util::sound {

class LoudnessMeter;
class LoudnessStat;
class StatCollector;

/**
 * @brief Detect short-term loudness and normalize to target level
 * @details Accumulates volume statistics, so a reset may be required when the audio path changes
 */
class LoudnessControl {
 public:
  struct State {
    // Instantaneous (current process() results)
    float current_gain_db_smoothed{0.0f};   // smoothed commanded gain, dB
    float current_gain_linear{1.0f};        // applied linear gain
    std::optional<float> last_short_term_lufs;  // short-term LUFS before applying gain

    // Long-term extrema since last reset (from LoudnessStat)
    float longterm_max_lufs{0.0f};
    float longterm_min_lufs{-80.0f};

    // Meta
    bool bypass_active{false};              // true while initial bypass is in effect
  };

   /**
   * @param sample_rate signal sample rate (Hz)
   * @param target_level target level in LUFS (dB)
   * @param bypass_time time (sec) to skip gain apply
   * @param max_gain max gain limit (db)
   */
explicit LoudnessControl(uint32_t sample_rate, float target_level = -23.0f,
                         float bypass_time = 3.0f, float max_gain = 40.0f);


  [[nodiscard]] std::vector<float> process(const std::vector<float>& x);

  [[nodiscard]] float current_gain() const noexcept { return m_gain; }

  void reset();

  [[nodiscard]] State state() const noexcept;

 private:
  std::shared_ptr<LoudnessMeter> m_mtr;
  std::shared_ptr<LoudnessStat> m_stat;
  int64_t m_bypass_counter{0};
  const float m_target_db;
  const float m_max_gain_db;
  float m_gain_db{0.0f};
  float m_gain{1.0f};

  std::optional<float> m_last_short_term;  // cached from meter
};

}  // namespace util::sound
