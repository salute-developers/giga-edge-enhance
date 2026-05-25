#include "spench/util/sound/range_control.h"

#include <algorithm>
#include <stdexcept>

namespace util {
namespace sound {

template<typename T>
PeakDetector<T>::PeakDetector(uint32_t frame_size,
                              uint32_t frame_count,
                              const T& init_value):
                              frame_size_{frame_size},
                              stat_(frame_count, init_value) {
  static_assert(std::is_arithmetic_v<T>);
}

template<typename T>
const T& PeakDetector<T>::process(const T& x) noexcept {
  frame_peak_ = std::max(frame_peak_, std::abs(x));
  frame_step_ += 1;
  if (frame_step_ == frame_size_) {
    stat_[istat_] = frame_peak_;
    istat_ = (istat_ + 1) % stat_.size();
    frame_step_ = 0;
    frame_peak_ = 0;
    best_peak_ = *std::max_element(stat_.begin(), stat_.end());
  }
  return std::max(best_peak_, frame_peak_);
}

RangeControl::RangeControl(uint32_t sample_rate,
                           float target_db,
                           float max_gain_db,
                           float attack_time,
                           float release_time,
                           float hold_time) : detector_{uint32_t(sample_rate * 0.01),
                                                         hold_time < 0 ? 0 : uint32_t(hold_time / 0.01)},
                                              target_{target_db},
                                              max_gain_{max_gain_db} {
  if (sample_rate > 100000) {
    throw std::runtime_error("RangeControl: `sample_rate` must be < 100.000.");
  }

  if (attack_time < 0.0) {
    throw std::runtime_error("RangeControl: `attack_time` must be positive.");
  }

  if (release_time < 0.0) {
    throw std::runtime_error("RangeControl: `release_time` must be positive.");
  }

  if (hold_time < 0.0) {
    throw std::runtime_error("RangeControl: `hold_time` must be positive.");
  }
  wa_ = std::exp(-std::log(9) / (sample_rate * attack_time));
  wr_ = std::exp(-std::log(9) / (sample_rate * release_time));
}

RangeControl::Result RangeControl::process(const std::vector<float>& x) noexcept {
  Result res(x.size());
  for (uint32_t i = 0; i < x.size(); ++i) {
    const auto peak = detector_(x[i]);
    const auto peakdb = dsplib::mag2db(peak + eps_);
    if (peak_ < peakdb) {
      // attack
      peak_ = (peak_ * wa_) + (1 - wa_) * peakdb;
    } else {
      // release
      peak_ = (peak_ * wr_) + (1 - wr_) * peakdb;
    }

    auto gain = target_ - peak_;
    gain = std::min(gain, max_gain_);
    gain = std::max(gain, -max_gain_);
    gain = dsplib::db2mag(gain);
    res.gain[i] = gain;
    res.out[i] = x[i] * gain;
  }
  return res;
}

}  // namespace sound
}  // namespace util
