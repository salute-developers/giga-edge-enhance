#pragma once

#include <dsplib/awgn.h>
#include <dsplib/delay.h>
#include <dsplib/gccphat.h>
#include <dsplib/math.h>

#include <algorithm>
#include <cassert>

namespace util::echo {

// the length of the median filter for smoothing the estimate
// convergence time after delay change: (MEDIAN_FLT_LEN / 2 * max_delay)
constexpr int MEDIAN_FLT_LEN = 7;

/**
 * @brief Estimation of the shift between the microphone and the speaker signal
 * @details Outputs an aligned speaker signal (relative to the microphone) with a small advance tolerance.
 * @todo Disable processing for weak signals below the noise floor
 */
class DelayCompensator {
 public:
  /**
   * @brief Construct a new Delay Compensator object
   *
   * @param max_delay Max value for finddelay(mic, spk)
   * @param quantize Step to update the time offset
   * @param threshold The acceptable range of the estimate, within which there is no delay correction
   * @param dither_db Added noise level for better stability in silence
   */
  explicit DelayCompensator(int max_delay = 16000, int quantize = 256, float threshold = 0.1f, float dither_db = -80)
      : m_win_len(1L << dsplib::nextpow2(max_delay)),
        m_mic_buf(m_win_len),
        m_spk_buf(m_win_len),
        m_quantize{quantize},
        m_threshold{threshold},
        m_dither_lvl{dsplib::db2mag(dither_db)} {
    DSPLIB_ASSERT(m_win_len <= (1L << 16), "`max_delay` must be less or equal 65536");
  }

  dsplib::arr_real Process(const dsplib::arr_real& mic, const dsplib::arr_real& spk) {
    DSPLIB_ASSERT(mic.size() == spk.size(), "input arrays must be equal size");
    const int len = mic.size();
    DSPLIB_ASSERT(len <= m_win_len, "input frame size must be less or equal `max_delay`");

    // update buffers (add `len` samples)
    const int upd_pos = m_win_len - len;
    m_mic_buf.slice(0, upd_pos) = m_mic_buf.slice(len, m_win_len);
    m_mic_buf.slice(upd_pos, m_win_len) = mic;
    m_spk_buf.slice(0, upd_pos) = m_spk_buf.slice(len, m_win_len);
    m_spk_buf.slice(upd_pos, m_win_len) = spk;

    m_update_cnt += len;

    _UpdateSync();

    return _AlignedFrame(len);
  }

  int GetCurrentOffset() const noexcept { return m_offset; }

 private:
  void _UpdateSync() {
    // TODO(vayulis): another overlap?
    if (m_update_cnt < m_win_len / 2) {
      return;
    }

    m_update_cnt = 0;

    // simulate `noise floor` (more stability on silence)
    const auto mic_n = m_mic_buf + dsplib::randn(m_mic_buf.size()) * m_dither_lvl;
    const auto spk_n = m_spk_buf + dsplib::randn(m_spk_buf.size()) * m_dither_lvl;
    auto [tau, corr] = dsplib::gccphat(mic_n, spk_n);

    // delay must be positive
    if (tau < 0 || std::isnan(tau)) {
      return;
    }

    // update the estimate only if there is a strong correlation
    auto peak = dsplib::abs(dsplib::max(corr));
    if (peak < m_threshold) {
      return;
    }

    // fix the value at the first trigger so that the filter converges quickly.
    if (!m_offset_flt) {
      m_offset = std::round(tau);
      m_offset_flt.emplace(dsplib::MedianFilter(MEDIAN_FLT_LEN, m_offset));
    }

    // the filter output is a smoothed estimate of the delay
    auto est_offset = m_offset_flt->process(dsplib::arr_real{tau})[0];

    // don't change anything if the delay delta hasn't changed much.
    if (std::abs(est_offset - m_offset) > m_quantize) {
      m_offset = std::round(est_offset);
    }
  }

  dsplib::arr_real _AlignedFrame(int frame_len) {
    assert(m_offset >= 0);
    // WARN: it is extremely important to have a tolerance for alignment error
    // if the `spk` signal is ahead of the `mic`, the AEC will not work properly
    int offset = m_offset - (m_quantize / 2);
    offset = (offset < 0) ? (0) : (offset);  // защита на случай нулевой задержки
    const int t1 = m_win_len - frame_len - offset;
    return dsplib::arr_real(m_spk_buf.data() + t1, frame_len);
  }

  const int m_win_len;
  dsplib::arr_real m_mic_buf;
  dsplib::arr_real m_spk_buf;
  int m_update_cnt{0};                               ///< samples counter to update estimate
  int m_offset{0};                                   ///< current time offset
  const int m_quantize{1};                           ///< error tolerance for synchronization
  const dsplib::real_t m_threshold{0.1};             ///< gccphat threshold to update offset estimate
  std::optional<dsplib::MedianFilter> m_offset_flt;  ///< median filter to smooth out the jitter of the estimate
  const dsplib::real_t m_dither_lvl{0};
};

}  // namespace util::echo
