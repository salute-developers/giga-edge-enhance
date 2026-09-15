#include "util/sound/range_control.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace util {
namespace sound {

namespace {

uint32_t detector_frame_size(uint32_t sample_rate) {
  if (sample_rate == 0 || sample_rate > 100000) {
    throw std::runtime_error("RangeControl: `sample_rate` must be in (0, 100000].");
  }
  return std::max<uint32_t>(1, static_cast<uint32_t>(sample_rate * 0.01));
}

uint32_t positive_size(uint32_t size, const char* name) {
  if (size == 0) {
    throw std::invalid_argument(std::string("PeakDetector: `") + name + "` must be positive.");
  }
  return size;
}

uint32_t detector_frame_count(float hold_time) {
  if (!std::isfinite(hold_time) || hold_time < 0.0F) {
    throw std::runtime_error("RangeControl: `hold_time` must be non-negative.");
  }
  const double frames = hold_time / 0.01;
  if (frames > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("RangeControl: `hold_time` is too large.");
  }
  return std::max<uint32_t>(1, static_cast<uint32_t>(frames));
}

float finite_value(float value, const char* name) {
  if (!std::isfinite(value)) {
    throw std::runtime_error(std::string("RangeControl: `") + name + "` must be finite.");
  }
  return value;
}

}  // namespace

template <typename T>
PeakDetector<T>::PeakDetector(uint32_t frame_size, uint32_t frame_count, const T& init_value)
    : frame_size_{positive_size(frame_size, "frame_size")},
      stat_(positive_size(frame_count, "frame_count"), init_value) {
  static_assert(std::is_arithmetic_v<T>);
}

template <typename T>
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

RangeControl::RangeControl(uint32_t sample_rate, float target_db, float max_gain_db, float attack_time,
                           float release_time, float hold_time)
    : detector_{detector_frame_size(sample_rate), detector_frame_count(hold_time)},
      target_{finite_value(target_db, "target_db")},
      max_gain_{finite_value(max_gain_db, "max_gain_db")} {
  if (max_gain_db < 0.0F) {
    throw std::runtime_error("RangeControl: `max_gain_db` must be non-negative.");
  }
  if (!std::isfinite(attack_time) || attack_time < 0.0F) {
    throw std::runtime_error("RangeControl: `attack_time` must be non-negative.");
  }

  if (!std::isfinite(release_time) || release_time < 0.0F) {
    throw std::runtime_error("RangeControl: `release_time` must be non-negative.");
  }
  wa_ = attack_time == 0.0F ? 0.0F : std::exp(-std::log(9) / (sample_rate * attack_time));
  wr_ = release_time == 0.0F ? 0.0F : std::exp(-std::log(9) / (sample_rate * release_time));
}

RangeControl::Result RangeControl::process(const std::vector<float>& x) {
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
