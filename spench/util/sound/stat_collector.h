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
    static constexpr Options all() noexcept { return Options{true, true, true, true, true, true}; }
  };

  enum class ResetMode { keep_overrun, hard };

  explicit StatCollector(std::string name, uint32_t sample_rate,
                         double window_seconds, Options opts = Options::all())
      : name_(std::move(name)), sr_(sample_rate), win_sec_(window_seconds), opt_(opts) {
    if (win_sec_ <= 0.0) { win_sec_ = 1.0; }
    if (sr_ == 0) { sr_ = 1; }
    win_samples_ = to_samples(win_sec_, sr_);
    if (win_samples_ == 0) { win_samples_ = 1; }
  }

  std::optional<Stat> update(float value, size_t samples_in_value) noexcept {
    if (opt_.need_min)  { min_ = min_ ? std::min(*min_, value) : value; }
    if (opt_.need_max)  { max_ = max_ ? std::max(*max_, value) : value; }
    if (opt_.need_mean) { sum_ += value; ++count_; }
    if (needs_order_stats()) {
      maybe_reserve(samples_in_value);
      vals_.push_back(value);
    }
    elapsed_samples_ += samples_in_value;
    return try_flush();
  }

  // NEW: advance time only (no value)
  std::optional<Stat> advance(size_t samples_in_value) noexcept {
    elapsed_samples_ += samples_in_value;
    return try_flush();
  }

  Stat current() const { return compute_stat_current_copy(); }

  std::optional<Stat> last() const noexcept { return last_; }

  std::string name() const noexcept { return name_; }
  double window_seconds() const noexcept { return win_sec_; }

  double progress_seconds() const noexcept {
    if (sr_ == 0) { return 0.0; }
    return static_cast<double>(elapsed_samples_) / static_cast<double>(sr_);
  }

  void set_window_seconds(double win_sec) {
    win_sec_ = (win_sec > 0.0 ? win_sec : 1.0);
    if (sr_ == 0) { sr_ = 1; }
    win_samples_ = to_samples(win_sec_, sr_);
    if (win_samples_ == 0) { win_samples_ = 1; }
    reset_window(ResetMode::hard);
  }

 private:
  std::optional<Stat> try_flush() noexcept {
    if (elapsed_samples_ >= win_samples_) {
      Stat s = compute_stat_flush_inplace();     // O(n) via nth_element
      last_ = s;
      reset_window(ResetMode::keep_overrun);
      return s;
    }
    return std::nullopt;
  }

  static size_t to_samples(double seconds, uint32_t sr) noexcept {
    const double s = std::ceil(seconds * static_cast<double>(sr));
    return static_cast<size_t>(s > 0.0 ? s : 0.0);
  }

  bool needs_order_stats() const noexcept {
    return opt_.need_median || opt_.need_q5 || opt_.need_q95;
  }

  void maybe_reserve(size_t samples_in_value) {
    if (vals_.capacity() == 0 && samples_in_value > 0) {
      const size_t est = static_cast<size_t>((win_samples_ + samples_in_value - 1) / samples_in_value);
      vals_.reserve(std::max<size_t>(est, 16));
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
    if (opt_.need_min) { s.min = min_; }
    if (opt_.need_max) { s.max = max_; }
    if (opt_.need_mean && count_ > 0) {
      s.mean = static_cast<float>(sum_ / static_cast<double>(count_));
    }
    if (needs_order_stats() && !vals_.empty()) {
      std::vector<float> vals = vals_;  // copy for non-destructive selection
      if (opt_.need_median) { s.median = select_median_linear(vals); }
      if (opt_.need_q95) { s.q95 = select_quantile_linear(vals, 0.95f); }
      if (opt_.need_q5) { s.q5 = select_quantile_linear(vals, 0.05f); }
    }
    return s;
  }

  Stat compute_stat_flush_inplace() {
    Stat s;
    if (opt_.need_min) { s.min = min_; }
    if (opt_.need_max) { s.max = max_; }
    if (opt_.need_mean && count_ > 0) {
      s.mean = static_cast<float>(sum_ / static_cast<double>(count_));
    }
    if (needs_order_stats() && !vals_.empty()) {
      if (opt_.need_median) { s.median = select_median_linear(vals_); }
      if (opt_.need_q95) { s.q95 = select_quantile_linear(vals_, 0.95f); }
      if (opt_.need_q5) { s.q5 = select_quantile_linear(vals_, 0.05f); }
    }
    return s;
  }

  void reset_window(ResetMode mode) noexcept {
    min_.reset();
    max_.reset();
    sum_ = 0.0;
    count_ = 0;
    vals_.clear();

    if (mode == ResetMode::keep_overrun) {
      if (win_samples_ == 0) { elapsed_samples_ = 0; return; }
      if (elapsed_samples_ >= win_samples_) {
        elapsed_samples_ -= win_samples_;
        if (elapsed_samples_ >= win_samples_) { elapsed_samples_ %= win_samples_; }
      }
    } else {
      elapsed_samples_ = 0;
    }
  }

  // State
  std::string name_;
  uint32_t sr_;
  double win_sec_;
  Options opt_;

  size_t win_samples_{0};
  size_t elapsed_samples_{0};

  std::optional<float> min_;
  std::optional<float> max_;
  double sum_{0.0};
  uint32_t count_{0};
  std::vector<float> vals_;

  std::optional<Stat> last_;
};

}  // namespace util::sound
