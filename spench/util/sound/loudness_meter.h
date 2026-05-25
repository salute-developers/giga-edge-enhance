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

  [[nodiscard]] const State& state() const noexcept { return state_; }

  size_t stride() const noexcept;

 private:
  std::shared_ptr<WeightingFilter> wflt_;
  std::shared_ptr<PowerMeter> mm_meter_;
  std::shared_ptr<PowerMeter> st_meter_;
  State state_;
};

}  // namespace util::sound
