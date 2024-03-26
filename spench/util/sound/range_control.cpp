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
  if (frame_size % 2 == 0) {
    throw std::runtime_error("PeakDetector: `frame_size` should be odd.");
  }
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

RangeControl::RangeControl(uint32_t average_len,
                           float max_level,
                           float step_size):
                           m_detector{(average_len / 16 + 1), 16, max_level},
                           m_max_level{max_level},
                           m_step_size{step_size} {
  if (max_level < 0.0) {
    throw std::runtime_error("RangeControl: `max_level` must be positive.");
  }

  if ((step_size < 0) || (step_size > 1)) {
    throw std::runtime_error("RangeControl: `step_size` must be in range (0:1).");
  }
}

RangeControl::Result RangeControl::process(const std::vector<float>& x) noexcept {
  RangeControl::Result res(x.size());
  for (uint32_t i = 0; i < x.size(); ++i) {
    const float dpeak = m_detector(x[i]);
    if (m_peak < dpeak) {
      // as quick as thought we update our estimation)
      m_peak = dpeak;
    } else {
      // but peak decreasing we are updating slowly
      m_peak = m_peak * (1 - m_step_size) + dpeak * m_step_size;
    }
    // we react on signal suppression only and because of that our minimal gain = 1.0
    res.gain[i] = std::min(static_cast<float>(m_max_level / (m_peak + this->eps)), 1.0f);
    res.out[i] = x[i] * res.gain[i];
  }
  return res;
}

}  // namespace sound
}  // namespace util
