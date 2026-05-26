#include "util/sound/loudness_meter.h"

#include <dsplib/math.h>
#include <dsplib/utils.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "util/sound/weighting_filter.h"


namespace util::sound {

namespace {

constexpr float eps = 1e-8;

float max_abs(const std::vector<float>& x) noexcept {
  float max = std::abs(x[0]);
  for (size_t i = 1; i < x.size(); ++i) {
    max = std::max(std::abs(x[i]), max);
  }
  return max;
}
}  // namespace

class PowerMeter {
 public:
  explicit PowerMeter(int32_t winlen, int32_t overlap) : winlen_{winlen}, stride_{winlen - overlap} {
    DSPLIB_ASSERT(winlen > overlap, "window size must be greater than overlap");
    buf_ = dsplib::zeros(winlen);
  }

  std::optional<float> process(const std::vector<float>& x) {
    std::optional<float> result;
    for (const auto& v : x) {
      buf_[idx_] = (v * v);
      idx_ = (idx_ < (winlen_ - 1)) ? (idx_ + 1) : 0;
      step_ += 1;
      if (step_ == stride_) {
        step_ = 0;
        result = dsplib::mean(buf_);
      }
    }
    return result;
  }

  [[nodiscard]] int32_t stride() const noexcept { return stride_; }

 private:
  int32_t step_{0};
  int32_t idx_{0};
  const int32_t winlen_;
  const int32_t stride_;
  dsplib::arr_real buf_;
};

LoudnessMeter::LoudnessMeter(int32_t sample_rate) {
  // see EBU R 128
  mm_meter_ = std::make_shared<PowerMeter>(sample_rate * 0.4, sample_rate * 0.35);  ///< 0.4s, 0.35 overlap
  st_meter_ = std::make_shared<PowerMeter>(sample_rate * 3.0, sample_rate * 2.95);  ///< 3.0s, 2.95 overlap
  wflt_ = std::make_shared<WeightingFilter>(sample_rate, WeightingFilter::Type::k);
}

size_t LoudnessMeter::stride() const noexcept { return st_meter_->stride(); }

void LoudnessMeter::update(const std::vector<float>& x) {
  // k-weightning
  const auto kx = wflt_->process(x);

  // momentary power
  auto mm = mm_meter_->process(kx);
  if (mm.has_value()) {
    auto p_ = -0.691F + dsplib::pow2db(*mm + eps);
    state_.momentary = p_;
  }

  // short-term power
  auto st = st_meter_->process(kx);
  if (st.has_value()) {
    auto s_p = -0.691F + dsplib::pow2db(*st + eps);
    state_.short_term = s_p;
  }

  // true peak level (for current frame)
  // TODO(vayulis): interpolate signal
  auto peak = dsplib::mag2db(max_abs(x));
  state_.true_peak = peak;
}

}  // namespace util::sound
