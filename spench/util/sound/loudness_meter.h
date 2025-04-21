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

  struct Result {
    float momentary{-80};
    float short_term{-80};
    float true_peak{-80};
  };

  /**
   * @brief Update meter state
   * @todo use span instead vector
   * @param x [in] signle channel audio frame
   * @return updated loudness info
   */
  Result update(const std::vector<float>& x);

  /**
   * @return meter has valid loudness data
   */
  bool is_ready() const noexcept;

 private:
  std::shared_ptr<WeightingFilter> m_wflt;
  std::shared_ptr<PowerMeter> m_mm_meter;
  std::shared_ptr<PowerMeter> m_st_meter;
  std::optional<float> m_mm_value;
  std::optional<float> m_st_value;
};

}  // namespace util::sound
