#pragma once

#include <dsplib/assert.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

namespace util::sound {

// ──────────────────────────────────────────────────────────────────────────────
// Second-order section (Direct-Form II Transposed) that runs in double precision
// ──────────────────────────────────────────────────────────────────────────────
class BiquadSection {
 public:
  // explicit numeric constructor
  explicit BiquadSection(double b0, double b1, double b2,
                         double a1, double a2) noexcept
      : b0_{b0}, b1_{b1}, b2_{b2}, a1_{a1}, a2_{a2} {}

  // constructor from single-precision coeff arrays (cast → double once)
  explicit BiquadSection(const std::array<float, 3>& num,
                         const std::array<float, 3>& den) noexcept
      : BiquadSection{static_cast<double>(num[0]),
                      static_cast<double>(num[1]),
                      static_cast<double>(num[2]),
                      static_cast<double>(den[1]),   //  den[0] == 1
                      static_cast<double>(den[2])} {}

  // in-place buffer processing helper
  void process(std::vector<float>& x) noexcept {
    for (float& v : x) v = process(v);
  }

  // sample-by-sample processing
  float process(float x) noexcept {
    const double xd = static_cast<double>(x);

    // Direct-Form II T in *double*
    const double y  = u1_ + b0_ * xd;
    u1_            = b1_ * xd - a1_ * y + u2_;
    u2_            = b2_ * xd - a2_ * y;

    return static_cast<float>(y);     // cast back for the caller
  }

 private:
  // coeffs (double precision)
  const double b0_;
  const double b1_;
  const double b2_;
  const double a1_;
  const double a2_;

  // state (double precision)
  double u1_{0};
  double u2_{0};
};

// ──────────────────────────────────────────────────────────────────────────────
// SOS filter: a cascade of BiquadSection + per-section gain scalers
// ──────────────────────────────────────────────────────────────────────────────
class SOSFilter {
 public:
  SOSFilter(const std::vector<std::array<float, 3>>& num,
            const std::vector<std::array<float, 3>>& den,
            const std::vector<float>&                 scale)
      : scale_{scale} {
    if (num.empty() || num.size() != den.size()) {
      throw std::invalid_argument("Filter coefficients must contain matching non-empty sections.");
    }
    if (scale.size() != num.size() + 1) {
      throw std::invalid_argument("Filter scale count does not match its sections.");
    }

    for (size_t i = 0; i < num.size(); ++i)
      iirs_.emplace_back(num[i], den[i]);   // uses new double-precision ctor
  }

  SOSFilter(const std::vector<std::array<float, 3>>& num,
            const std::vector<std::array<float, 3>>& den,
            float scale = 1.0f)
      : SOSFilter{num, den, scale_array(num.size(), scale)} {}

  SOSFilter(const std::array<float, 3>& num,
            const std::array<float, 3>& den,
            float scale = 1.0f)
      : SOSFilter{{num}, {den}, scale_array(1, scale)} {}

  // single-sample processing
  float process(float x) noexcept {
    double y = static_cast<double>(x) * static_cast<double>(scale_[0]);

    for (size_t k = 0; k < iirs_.size(); ++k) {
      y = static_cast<double>(iirs_[k].process(static_cast<float>(y)));
      y *= static_cast<double>(scale_[k + 1]);
    }
    return static_cast<float>(y);
  }

  // whole-buffer helper
  std::vector<float> process(const std::vector<float>& x) {
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = process(x[i]);
    return y;
  }

 private:
  std::vector<float>          scale_;   // keep as float, fine here
  std::vector<BiquadSection>        iirs_;

  static std::vector<float> scale_array(size_t nsect, float scale) {
    std::vector<float> r(nsect + 1U, 1.0f);
    r[0] = scale;
    return r;
  }
};

// ──────────────────────────────────────────────────────────────────────────────
// Convenience wrapper that selects K-weighting (or raw passthrough)
// ──────────────────────────────────────────────────────────────────────────────
class WeightingFilter {
 public:
  enum class Type { raw, k };

  explicit WeightingFilter(size_t sample_rate,
                           Type   type = Type::k) : type_{type} {
    if (type != Type::k) return;   // Raw: leave filter_ nullptr

    switch (sample_rate) {
      case 16000:  make_k_filter_16k();  break;
      case 24000:  make_k_filter_24k();  break;
      case 44100:  make_k_filter_44k1(); break;
      case 48000:  make_k_filter_48k();  break;
      default:     type_ = Type::raw;   // unsupported Fs
    }
  }

  [[nodiscard]] Type type() const noexcept { return type_; }

  std::vector<float> process(const std::vector<float>& x) {
    if (!filter_) return x;      // Raw or unsupported Fs
    return filter_->process(x);
  }

 private:
  std::shared_ptr<SOSFilter> filter_;
  Type                       type_{Type::raw};

  /* ===== K-weighting tables (Matlab’s weightingFilter) =============== */
  void make_k_filter_16k() {
    const std::vector<std::array<float, 3>> num = {
      {1.44329522349136, -1.83157538126046, 0.681658757411971},
      {0.990120978871932, -1.98024195774386, 0.990120978871932}};
    const std::vector<std::array<float, 3>> den = {
      {1, -1.10153376910700, 0.394912368749864},
      {1, -1.97028952800443, 0.970510490535844}};
    filter_ = std::make_shared<SOSFilter>(num, den, std::vector<float>{1, 1, 1});
  }

  void make_k_filter_24k() {
    const std::vector<std::array<float, 3>> num = {
      {1.48790022096228, -2.24620546814114, 0.904909123246438},
      {0.995042168936226, -1.99008433787245, 0.995042168936226}};
    const std::vector<std::array<float, 3>> den = {
      {1, -1.39023460519282, 0.536838481260398},
      {1, -1.98014412622893, 0.980242817859277}};
    filter_ = std::make_shared<SOSFilter>(num, den, std::vector<float>{1, 1, 1});
  }

  void make_k_filter_44k1() {
    const std::vector<std::array<float, 3>> num = {
      {1.53084123005035, -2.65097999515473, 1.16907907992159},
      {0.999560064542514, -1.99912012908503, 0.999560064542514}};
    const std::vector<std::array<float, 3>> den = {
      {1, -1.66365511325602, 0.712595428073225},
      {1, -1.98916967362980, 0.989199035787039}};
    filter_ = std::make_shared<SOSFilter>(num, den, std::vector<float>{1, 1, 1});
  }

  void make_k_filter_48k() {
    const std::vector<std::array<float, 3>> num = {
      {1.53512485958697, -2.69169618940638, 1.19839281085285},
      {1, -2, 1}};
    const std::vector<std::array<float, 3>> den = {
      {1, -1.69065929318241, 0.732480774215850},
      {1, -1.99004745483398, 0.990072250366210}};
    filter_ = std::make_shared<SOSFilter>(num, den, std::vector<float>{1, 1, 1});
  }
};

}  // namespace util::sound
