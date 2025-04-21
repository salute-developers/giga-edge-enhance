#include "util/sound/loudness_meter.h"

#include <dsplib/math.h>
#include <dsplib/utils.h>

#include <algorithm>
#include <memory>
#include <optional>

#include "util/sound/weighting_filter.h"


namespace util::sound {

namespace {

constexpr float eps = 1e-8;

float _max_abs(const std::vector<float>& x) noexcept {
  float max = std::abs(x[0]);
  for (size_t i = 1; i < x.size(); ++i) {
    max = std::max(std::abs(x[i]), max);
  }
  return max;
}
}  // namespace

class PowerMeter {
 public:
  explicit PowerMeter(int32_t winlen, int32_t overlap) : m_winlen{winlen}, m_stride{winlen - overlap} {
    DSPLIB_ASSERT(winlen > overlap, "window size must be greater than overlap");
    m_buf = dsplib::zeros(m_winlen);
  }

  std::optional<float> process(const std::vector<float>& x) {
    std::optional<float> result;
    for (const auto& v : x) {
      m_buf[m_idx] = (v * v);
      m_idx = (m_idx < (m_winlen - 1)) ? (m_idx + 1) : 0;
      m_step += 1;
      if (m_step == m_stride) {
        m_step = 0;
        result = dsplib::mean(m_buf);
      }
    }
    return result;
  }

 private:
  int32_t m_step{0};
  int32_t m_idx{0};
  const int32_t m_winlen;
  const int32_t m_stride;
  dsplib::arr_real m_buf;
};

LoudnessMeter::LoudnessMeter(int32_t sample_rate) {
  const int32_t wlen = static_cast<int32_t>(sample_rate * 0.4);
  const int32_t overlap = static_cast<int32_t>(sample_rate * 0.3);
  m_mm_meter = std::make_shared<PowerMeter>(wlen, overlap);
  m_st_meter = std::make_shared<PowerMeter>(wlen, overlap);
  m_wflt = std::make_shared<WeightingFilter>(sample_rate, WeightingFilter::Type::K);
}

LoudnessMeter::Result LoudnessMeter::update(const std::vector<float>& x) {
  Result res;

  // k-weightning
  const auto kx = m_wflt->process(x);

  // momentary power
  auto mm_pow = m_mm_meter->process(kx);
  if (mm_pow.has_value()) {
    m_mm_value = -0.691F + dsplib::pow2db(*mm_pow + eps);
  }

  // short-term power
  auto st_pow = m_st_meter->process(kx);
  if (st_pow.has_value()) {
    m_st_value = -0.691F + dsplib::pow2db(*st_pow + eps);
  }

  // true peak level (for current frame)
  // TODO(vayulis): interpolate signal
  auto peak = dsplib::mag2db(_max_abs(x));

  res.momentary = m_mm_value.value_or(-80);
  res.short_term = m_st_value.value_or(-80);
  res.true_peak = peak;

  return res;
}

bool LoudnessMeter::is_ready() const noexcept { return m_mm_value.has_value() && m_st_value.has_value(); }

}  // namespace util::sound
