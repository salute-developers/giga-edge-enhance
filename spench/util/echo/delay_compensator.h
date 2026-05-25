#pragma once

#include <dsplib/awgn.h>
#include <dsplib/delay.h>
#include <dsplib/gccphat.h>
#include <dsplib/math.h>

#include <algorithm>
#include <cassert>

namespace util::echo {

// the length of the median filter for smoothing the estimate
// convergence time after delay change: (median_flt_len / 2 * max_delay)
constexpr int median_flt_len = 7;

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
      : win_len(1L << dsplib::nextpow2(max_delay)),
        mic_buf_(win_len),
        spk_buf_(win_len),
        quantize{quantize},
        threshold{threshold},
        dither_lvl{dsplib::db2mag(dither_db)} {
    DSPLIB_ASSERT(win_len <= (1L << 16), "`max_delay` must be less or equal 65536");
  }

  dsplib::arr_real process(const dsplib::arr_real& mic, const dsplib::arr_real& spk) {
    DSPLIB_ASSERT(mic.size() == spk.size(), "input arrays must be equal size");
    const int len = mic.size();
    DSPLIB_ASSERT(len <= win_len, "input frame size must be less or equal `max_delay`");

    // update buffers (add `len` samples)
    const int upd_pos = win_len - len;
    mic_buf_.slice(0, upd_pos) = mic_buf_.slice(len, win_len);
    mic_buf_.slice(upd_pos, win_len) = mic;
    spk_buf_.slice(0, upd_pos) = spk_buf_.slice(len, win_len);
    spk_buf_.slice(upd_pos, win_len) = spk;

    update_cnt_ += len;

    update_sync();

    return aligned_frame(len);
  }

  int get_current_offset() const noexcept { return offset_; }

 private:
  void update_sync() {
    // TODO(vayulis): another overlap?
    if (update_cnt_ < win_len / 2) {
      return;
    }

    update_cnt_ = 0;

    // simulate `noise floor` (more stability on silence)
    const auto mic_n = mic_buf_ + dsplib::randn(mic_buf_.size()) * dither_lvl;
    const auto spk_n = spk_buf_ + dsplib::randn(spk_buf_.size()) * dither_lvl;
    auto [tau, corr] = dsplib::gccphat(mic_n, spk_n);

    // delay must be positive
    if (tau < 0 || std::isnan(tau)) {
      return;
    }

    // update the estimate only if there is a strong correlation
    auto peak = dsplib::abs(dsplib::max(corr));
    if (peak < threshold) {
      return;
    }

    // fix the value at the first trigger so that the filter converges quickly.
    if (!offset_flt_) {
      offset_ = std::round(tau);
      offset_flt_.emplace(dsplib::MedianFilter(median_flt_len, offset_));
    }

    // the filter output is a smoothed estimate of the delay
    auto est_offset = offset_flt_->process(dsplib::arr_real{tau})[0];

    // don't change anything if the delay delta hasn't changed much.
    if (std::abs(est_offset - offset_) > quantize) {
      offset_ = std::round(est_offset);
    }
  }

  dsplib::arr_real aligned_frame(int frame_len) {
    assert(offset_ >= 0);
    // WARN: it is extremely important to have a tolerance for alignment error
    // if the `spk` signal is ahead of the `mic`, the AEC will not work properly
    int offset = offset_ - (quantize / 2);
    offset = (offset < 0) ? (0) : (offset);  // защита на случай нулевой задержки
    const int t1 = win_len - frame_len - offset;
    return dsplib::arr_real(spk_buf_.data() + t1, frame_len);
  }

  const int win_len;
  dsplib::arr_real mic_buf_;
  dsplib::arr_real spk_buf_;
  int update_cnt_{0};                               ///< samples counter to update estimate
  int offset_{0};                                   ///< current time offset
  const int quantize{1};                           ///< error tolerance for synchronization
  const dsplib::real_t threshold{0.1};             ///< gccphat threshold to update offset estimate
  std::optional<dsplib::MedianFilter> offset_flt_;  ///< median filter to smooth out the jitter of the estimate
  const dsplib::real_t dither_lvl{0};
};

}  // namespace util::echo
