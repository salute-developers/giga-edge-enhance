#include "spench/util/metrics/mute_frame_detector.h"

namespace util {
namespace metrics {

uint32_t MuteFrameDetector::process(float x) noexcept {
    assert(buf_.size() % 3 == 0);

    // Let's move on to integer arithmetic to make it easier to implement a moving average filter
    x = x * scale_;
    const auto xx = int64_t(x * x) / n_;

    acc_l_ -= buf_[index(0)];
    acc_c_ -= buf_[index(n_)];
    acc_r_ -= buf_[index(2 * n_)];

    buf_[ibuf_] = xx;

    acc_l_ += buf_[index(n_)];
    acc_c_ += buf_[index(2 * n_)];
    acc_r_ += buf_[index(3 * n_)];

    ibuf_ = (ibuf_ + 1) % n3_;

    const float pl = pow2db(acc_l_) - scale_db_;
    const float pc = pow2db(acc_c_) - scale_db_;
    const float pr = pow2db(acc_r_) - scale_db_;
    const float pm = pow2db((acc_l_ + acc_r_) / 2) - scale_db_;

    // don’t make decisions on a weak signal
    if (pm < min_rms_) {
        return 0;
    }

    // the difference between left/right frame should not exceed a threshold
    if (std::abs(pl - pr) > lr_range_) {
        return 0;
    }

    // local minimum must be greater than threshold
    if (pm - pc < threshold_) {
        return 0;
    }

    return 1;
}

}  // namespace metrics
}  // namespace util
