#pragma once

#include <dsplib/math.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

namespace util {
namespace sound {

// ----------------------------------------------------------------------
// Sliding peak detector
// Memory size "in samples" defined as `frame_size * frame_count`
template <typename T>
class PeakDetector {
 private:
  T frame_peak_{0};
  T best_peak_{0};
  uint32_t frame_size_;
  uint32_t frame_step_{0};
  std::vector<T> stat_;
  uint32_t istat_{0};

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

  const T& operator()(const T& x) noexcept { return this->process(x); }

  const T& process(const T& x) noexcept;
};

//----------------------------------------------------------------------
class RangeControl {
 private:
  PeakDetector<float> detector_;
  float eps_{1e-5};
  float target_{-3};
  float max_gain_{0};
  float peak_{-3};
  float wa_;
  float wr_;

 public:
  /**
   * @brief RangeControl constructor
   *
   * @param sample_rate An audio sample rate (Hz)
   * @param target_db A target peak value (db)
   * @param max_gain A max output signal value (db)
   * @param attack_time An attack time for peak value estimation (sec)
   * @param release_time A release time for peak value estimation (sec)
   * @param hold_time A hold time for peak value estimation (sec)
   */
  RangeControl(uint32_t sample_rate = 48000, float target_db = -3, float max_gain_db = 10, float attack_time = 1e-5,
               float release_time = 300, float hold_time = 5);
  RangeControl() = delete;
  RangeControl& operator=(const RangeControl&) = delete;

  struct Result {
    explicit Result(uint32_t size) : out(size), gain(size) {}

    std::vector<float> out;
    std::vector<float> gain;
  };

  Result operator()(const std::vector<float>& x) { return this->process(x); }

  Result process(const std::vector<float>& x);
};

}  // namespace sound
}  // namespace util
