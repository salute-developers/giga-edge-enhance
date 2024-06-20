#include "spench/util/sound/range_control.h"

#include <algorithm>
#include <stdexcept>

namespace util {
namespace sound {

template<typename T>
PeakDetector<T>::PeakDetector(uint32_t frame_size,
                              uint32_t frame_count,
                              const T& init_value):
                              m_frame_size{frame_size},
                              m_stat(frame_count, init_value) {
  static_assert(std::is_arithmetic_v<T>);
}

template<typename T>
const T& PeakDetector<T>::process(const T& x) noexcept {
  m_frame_peak = std::max(m_frame_peak, std::abs(x));
  m_frame_step += 1;
  if (m_frame_step == m_frame_size) {
    m_stat[m_istat] = m_frame_peak;
    m_istat = (m_istat + 1) % m_stat.size();
    m_frame_step = 0;
    m_frame_peak = 0;
    m_best_peak = *std::max_element(m_stat.begin(), m_stat.end());
  }
  return std::max(m_best_peak, m_frame_peak);
}

RangeControl::RangeControl(uint32_t sample_rate,
                           float target_db,
                           float max_gain_db,
                           float attack_time,
                           float release_time,
                           float hold_time) : m_detector{uint32_t(sample_rate * 0.01),
                                                         hold_time < 0 ? 0 : uint32_t(hold_time / 0.01)},
                                              m_target{target_db},
                                              m_max_gain{max_gain_db} {
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
  m_wa = std::exp(-std::log(9) / (sample_rate * attack_time));
  m_wr = std::exp(-std::log(9) / (sample_rate * release_time));
}

RangeControl::Result RangeControl::process(const std::vector<float>& x) noexcept {
  Result res(x.size());
  for (uint32_t i = 0; i < x.size(); ++i) {
    const auto peak = m_detector(x[i]);
    const auto peakdb = dsplib::mag2db(peak + m_eps);
    if (m_peak < peakdb) {
      //attack
      m_peak = (m_peak * m_wa) + (1 - m_wa) * peakdb;
    } else {
      //release
      m_peak = (m_peak * m_wr) + (1 - m_wr) * peakdb;
    }

    auto gain = m_target - m_peak;
    gain = std::min(gain, m_max_gain);
    gain = std::max(gain, -m_max_gain);
    gain = dsplib::db2mag(gain);
    res.gain[i] = gain;
    res.out[i] = x[i] * gain;
  }
  return res;
}

}  // namespace sound
}  // namespace util
