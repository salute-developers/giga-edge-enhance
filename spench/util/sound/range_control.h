#pragma once

#include <memory>
#include <vector>

namespace util {
namespace sound {

// ----------------------------------------------------------------------
// Sliding peak detector
// Memory size "in samples" defined as `frame_size * frame_count`
// Memory экономия по памяти в `frame_size` за счет сокращения точности во времени
// по сути это эквивалент линии задержки в `frame_size * frame_count` с оценкой максиума каждые `frame_size` отсчетов
template<typename T>
class PeakDetector {
 private:
  T m_frame_peak{0};
  T m_best_peak{0};
  uint32_t m_frame_size;
  uint32_t m_frame_step{0};
  std::vector<T> m_stat;
  uint32_t m_istat{0};

 public:
 /**
 * @brief PeakDetector constructor
 *
 * @param frame_size Subframe size for bufferisation
 * @param frame_count Number of frames
 * @param init_value Initial peak value in subframes
 */
  explicit PeakDetector(uint32_t frame_size, uint32_t frame_count, const T& init_value = 0);
  PeakDetector() = delete;
  PeakDetector& operator=(const PeakDetector&) = delete;

  const T& operator()(const T& x) noexcept {
    return this->process(x);
  }

  const T& process(const T& x) noexcept;
};

//----------------------------------------------------------------------
// smooth signal dynamic squeezing up to `max_level`
class RangeControl {
 private:
  PeakDetector<float> m_detector;
  float m_max_level{0.75};
  float m_step_size{1e-3};
  float m_peak{1e-5};
  float eps{1e-5};

 public:
 /**
 * @brief RangeControl constructor
 *
 * @param average_len Window size to store last peak estimations
 * @param max_level Maximum level of output signal
 * @param step_size Constant required for peak estimation update (for decreasing only)
 */
  explicit RangeControl(uint32_t average_len, float max_level = 0.75, float step_size = 1e-3);
  RangeControl() = delete;
  RangeControl& operator=(const RangeControl&) = delete;

  struct Result {
    explicit Result(uint32_t size):
                        out(size),
                        gain(size) {}

    std::vector<float> out;
    std::vector<float> gain;
  };

  Result operator()(const std::vector<float>& x) noexcept {
    return this->process(x);
  }

  Result process(const std::vector<float>& x) noexcept;
};

}  // namespace sound
}  // namespace util
