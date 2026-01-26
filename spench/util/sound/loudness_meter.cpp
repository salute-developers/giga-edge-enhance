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

  [[nodiscard]] int32_t stride() const noexcept { return m_stride; }

 private:
  int32_t m_step{0};
  int32_t m_idx{0};
  const int32_t m_winlen;
  const int32_t m_stride;
  dsplib::arr_real m_buf;
};

LoudnessMeter::LoudnessMeter(int32_t sample_rate) {
  // see EBU R 128
  m_mm_meter = std::make_shared<PowerMeter>(sample_rate * 0.4, sample_rate * 0.35);  ///< 0.4s, 0.35 overlap
  m_st_meter = std::make_shared<PowerMeter>(sample_rate * 3.0, sample_rate * 2.95);  ///< 3.0s, 2.95 overlap
  m_wflt = std::make_shared<WeightingFilter>(sample_rate, WeightingFilter::Type::K);
}

size_t LoudnessMeter::stride() const noexcept { return m_st_meter->stride(); }

void LoudnessMeter::update(const std::vector<float>& x) {
  // k-weightning
  const auto kx = m_wflt->process(x);

  // momentary power
  auto mm = m_mm_meter->process(kx);
  if (mm.has_value()) {
    auto mP = -0.691F + dsplib::pow2db(*mm + eps);
    m_state.momentary = mP;
  }

  // short-term power
  auto st = m_st_meter->process(kx);
  if (st.has_value()) {
    auto sP = -0.691F + dsplib::pow2db(*st + eps);
    m_state.short_term = sP;
  }

  // true peak level (for current frame)
  // TODO(vayulis): interpolate signal
  auto peak = dsplib::mag2db(_max_abs(x));
  m_state.true_peak = peak;
}

}  // namespace util::sound
