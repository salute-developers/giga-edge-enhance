#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace util::sound {

class WeightingFilter;
class PowerMeter;

/**
 * @brief Standard-compliant loudness measurements
 * @see https://www.mathworks.com/help/audio/ref/loudnessmeter-system-object.html
 */
class LoudnessMeter {
 public:
  explicit LoudnessMeter(int32_t sample_rate);

  struct State {
    std::optional<float> momentary;
    std::optional<float> short_term;
    float true_peak{-80};
  };

  /**
   * @brief Update meter state
   * @todo use span instead vector
   * @param x [in] signle channel audio frame
   */
  void update(const std::vector<float>& x);

  [[nodiscard]] const State& state() const noexcept { return m_state; }

  size_t stride() const noexcept;

 private:
  std::shared_ptr<WeightingFilter> m_wflt;
  std::shared_ptr<PowerMeter> m_mm_meter;
  std::shared_ptr<PowerMeter> m_st_meter;
  State m_state;
};

}  // namespace util::sound
