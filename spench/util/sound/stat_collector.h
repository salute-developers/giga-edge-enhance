#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace util::sound {

class StatCollector {
 public:
  struct Stat {
    std::optional<float> max;
    std::optional<float> min;
    std::optional<float> mean;
    std::optional<float> median;
    std::optional<float> q95;
    std::optional<float> q5;
  };

  struct Options {
    bool need_min = true;
    bool need_max = true;
    bool need_mean = true;
    bool need_median = true;
    bool need_q95 = true;
    bool need_q5 = true;
    static constexpr Options All() noexcept { return Options{true, true, true, true, true, true}; }
  };

  enum class ResetMode { KeepOverrun, Hard };

  explicit StatCollector(std::string name, uint32_t sample_rate,
                         double window_seconds, Options opts = Options::All())
      : m_name(std::move(name)), m_sr(sample_rate), m_win_sec(window_seconds), m_opt(opts) {
    if (m_win_sec <= 0.0) { m_win_sec = 1.0; }
    if (m_sr == 0) { m_sr = 1; }
    m_win_samples = to_samples(m_win_sec, m_sr);
    if (m_win_samples == 0) { m_win_samples = 1; }
  }

  std::optional<Stat> update(float value, size_t samples_in_value) noexcept {
    if (m_opt.need_min)  { m_min = m_min ? std::min(*m_min, value) : value; }
    if (m_opt.need_max)  { m_max = m_max ? std::max(*m_max, value) : value; }
    if (m_opt.need_mean) { m_sum += value; ++m_count; }
    if (needs_order_stats()) {
      maybe_reserve(samples_in_value);
      m_vals.push_back(value);
    }
    m_elapsed_samples += samples_in_value;
    return try_flush_();
  }

  // NEW: advance time only (no value)
  std::optional<Stat> advance(size_t samples_in_value) noexcept {
    m_elapsed_samples += samples_in_value;
    return try_flush_();
  }

  Stat current() const { return compute_stat_current_copy(); }

  std::optional<Stat> last() const noexcept { return m_last; }

  std::string name() const noexcept { return m_name; }
  double window_seconds() const noexcept { return m_win_sec; }

  double progress_seconds() const noexcept {
    if (m_sr == 0) { return 0.0; }
    return static_cast<double>(m_elapsed_samples) / static_cast<double>(m_sr);
  }

  void set_window_seconds(double win_sec) {
    m_win_sec = (win_sec > 0.0 ? win_sec : 1.0);
    if (m_sr == 0) { m_sr = 1; }
    m_win_samples = to_samples(m_win_sec, m_sr);
    if (m_win_samples == 0) { m_win_samples = 1; }
    reset_window(ResetMode::Hard);
  }

 private:
  std::optional<Stat> try_flush_() noexcept {
    if (m_elapsed_samples >= m_win_samples) {
      Stat s = compute_stat_flush_inplace();     // O(n) via nth_element
      m_last = s;
      reset_window(ResetMode::KeepOverrun);
      return s;
    }
    return std::nullopt;
  }

  static size_t to_samples(double seconds, uint32_t sr) noexcept {
    const double s = std::ceil(seconds * static_cast<double>(sr));
    return static_cast<size_t>(s > 0.0 ? s : 0.0);
  }

  bool needs_order_stats() const noexcept {
    return m_opt.need_median || m_opt.need_q5 || m_opt.need_q95;
  }

  void maybe_reserve(size_t samples_in_value) {
    if (m_vals.capacity() == 0 && samples_in_value > 0) {
      const size_t est = static_cast<size_t>((m_win_samples + samples_in_value - 1) / samples_in_value);
      m_vals.reserve(std::max<size_t>(est, 16));
    }
  }

  static float select_quantile_linear(std::vector<float>& a, float q) {
    const size_t n = a.size();
    if (n == 0) { return std::numeric_limits<float>::quiet_NaN(); }
    if (q <= 0.0f) { return *std::min_element(a.begin(), a.end()); }
    if (q >= 1.0f) { return *std::max_element(a.begin(), a.end()); }

    const float pos = q * static_cast<float>(n - 1);
    const size_t lo = static_cast<size_t>(std::floor(pos));
    const size_t hi = std::min(lo + 1, n - 1);
    const float frac = pos - static_cast<float>(lo);

    std::nth_element(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(lo), a.end());
    const float lo_val = a[lo];
    if (hi == lo) { return lo_val; }
    std::nth_element(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(hi), a.end());
    const float hi_val = a[hi];
    return lo_val + frac * (hi_val - lo_val);
  }

  static float select_median_linear(std::vector<float>& a) {
    const size_t n = a.size();
    if (n == 0) { return std::numeric_limits<float>::quiet_NaN(); }
    if (n % 2 == 1) {
      const size_t mid = n / 2;
      std::nth_element(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(mid), a.end());
      return a[mid];
    }
    const size_t r = n / 2;          // upper index
    const size_t l = r - 1;          // lower index
    std::nth_element(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(l), a.end());
    const float lo_val = a[l];
    std::nth_element(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(r), a.end());
    const float hi_val = a[r];
    return (lo_val + hi_val) * 0.5f;
  }

  Stat compute_stat_current_copy() const {
    Stat s;
    if (m_opt.need_min) { s.min = m_min; }
    if (m_opt.need_max) { s.max = m_max; }
    if (m_opt.need_mean && m_count > 0) {
      s.mean = static_cast<float>(m_sum / static_cast<double>(m_count));
    }
    if (needs_order_stats() && !m_vals.empty()) {
      std::vector<float> vals = m_vals;  // copy for non-destructive selection
      if (m_opt.need_median) { s.median = select_median_linear(vals); }
      if (m_opt.need_q95) { s.q95 = select_quantile_linear(vals, 0.95f); }
      if (m_opt.need_q5) { s.q5 = select_quantile_linear(vals, 0.05f); }
    }
    return s;
  }

  Stat compute_stat_flush_inplace() {
    Stat s;
    if (m_opt.need_min) { s.min = m_min; }
    if (m_opt.need_max) { s.max = m_max; }
    if (m_opt.need_mean && m_count > 0) {
      s.mean = static_cast<float>(m_sum / static_cast<double>(m_count));
    }
    if (needs_order_stats() && !m_vals.empty()) {
      if (m_opt.need_median) { s.median = select_median_linear(m_vals); }
      if (m_opt.need_q95) { s.q95 = select_quantile_linear(m_vals, 0.95f); }
      if (m_opt.need_q5) { s.q5 = select_quantile_linear(m_vals, 0.05f); }
    }
    return s;
  }

  void reset_window(ResetMode mode) noexcept {
    m_min.reset();
    m_max.reset();
    m_sum = 0.0;
    m_count = 0;
    m_vals.clear();

    if (mode == ResetMode::KeepOverrun) {
      if (m_win_samples == 0) { m_elapsed_samples = 0; return; }
      if (m_elapsed_samples >= m_win_samples) {
        m_elapsed_samples -= m_win_samples;
        if (m_elapsed_samples >= m_win_samples) { m_elapsed_samples %= m_win_samples; }
      }
    } else {
      m_elapsed_samples = 0;
    }
  }

  // State
  std::string m_name;
  uint32_t m_sr;
  double m_win_sec;
  Options m_opt;

  size_t m_win_samples{0};
  size_t m_elapsed_samples{0};

  std::optional<float> m_min;
  std::optional<float> m_max;
  double m_sum{0.0};
  uint32_t m_count{0};
  std::vector<float> m_vals;

  std::optional<Stat> m_last;
};

}  // namespace util::sound
