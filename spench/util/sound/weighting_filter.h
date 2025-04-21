#pragma once

#include <dsplib/assert.h>

#include <array>
#include <memory>
#include <vector>

namespace util::sound {

// second-order section (form II transposed)
class BiquadSection {
 public:
  explicit BiquadSection(float b0, float b1, float b2, float a1, float a2) noexcept
      : m_b0{b0}, m_b1{b1}, m_b2{b2}, m_a1{a1}, m_a2{a2} {}

  explicit BiquadSection(const std::array<float, 3>& num, const std::array<float, 3>& den) noexcept
      : BiquadSection{num[0], num[1], num[2], den[1], den[2]}  ///< den[0] == 1, not used
  {}

  void process(std::vector<float>& x) noexcept {
    const size_t n = x.size();
    for (size_t i = 0; i < n; ++i) {
      x[i] = process(x[i]);
    }
  }

  float process(float x) noexcept {
    // form II transposed (more stable)
    const float u0 = m_u1 + (m_b0 * x);
    m_u1 = (m_b1 * x) - (m_a1 * u0) + m_u2;
    m_u2 = (m_b2 * x) - (m_a2 * u0);
    return u0;
  }

 private:
  const float m_b0;
  const float m_b1;
  const float m_b2;
  const float m_a1;
  const float m_a2;
  float m_u1{0};
  float m_u2{0};
};

// second-order section biquadratic IIR filter structures
class SOSFilter {
 public:
  explicit SOSFilter(const std::vector<std::array<float, 3>>& num, const std::vector<std::array<float, 3>>& den,
                     const std::vector<float>& scale)
      : m_scale{scale} {
    DSPLIB_ASSERT((num.size() == den.size()) && (num[0].size() == den[0].size()), "filter size error");
    DSPLIB_ASSERT(scale.size() == num.size() + 1, "filter size error");
    for (size_t i = 0; i < num.size(); ++i) {
      m_iirs.emplace_back(num[i], den[i]);
    }
  }

  explicit SOSFilter(const std::vector<std::array<float, 3>>& num, const std::vector<std::array<float, 3>>& den,
                     float scale = 1.0)
      : SOSFilter{num, den, m_scale_array(num.size(), scale)} {}

  explicit SOSFilter(const std::array<float, 3>& num, const std::array<float, 3>& den, float scale = 1.0)
      : SOSFilter{{num}, {den}, m_scale_array(1, scale)} {}

  float process(float x) noexcept {
    x *= m_scale[0];
    for (size_t k = 0; k < m_iirs.size(); ++k) {
      x = m_iirs[k].process(x);
      x *= m_scale[k + 1];
    }
    return x;
  }

  std::vector<float> process(const std::vector<float>& x) noexcept {
    std::vector<float> y(x.size());
    const size_t n = x.size();
    for (size_t i = 0; i < n; ++i) {
      y[i] = process(x[i]);
    }
    return y;
  }

 private:
  const std::vector<float> m_scale;
  std::vector<BiquadSection> m_iirs;

  static std::vector<float> m_scale_array(size_t nsect, float scale) {
    std::vector<float> r = std::vector<float>(nsect, 1);
    r[0] = scale;
    return r;
  }
};

// see matlab weightingFilter
class WeightingFilter {
 public:
  // TODO(vayulis): add "A", "C" filters
  enum class Type { Raw, K };

  explicit WeightingFilter(size_t sample_rate, Type type = Type::K) : m_type{type} {
    switch (sample_rate) {
      case 16000:
        if (type == Type::K) {
          // K-filter coeffs from matlab
          const std::vector<std::array<float, 3>> num = {{1.4433, -1.8316, 0.6817}, {0.9901, -1.9802, 0.9901}};
          const std::vector<std::array<float, 3>> den = {{1.0000, -1.1015, 0.3949}, {1.0000, -1.9703, 0.9705}};
          const std::vector<float> scale = {1, 1, 1};
          m_filter = std::make_shared<SOSFilter>(num, den, scale);
        }
        break;

      case 24000:
        if (type == Type::K) {
          const std::vector<std::array<float, 3>> num = {{1.4879, -2.2462, 0.9049}, {0.9950, -1.9901, 0.9950}};
          const std::vector<std::array<float, 3>> den = {{1.0000, -1.3902, 0.5368}, {1.0000, -1.9801, 0.9802}};
          const std::vector<float> scale = {1, 1, 1};
          m_filter = std::make_shared<SOSFilter>(num, den, scale);
        }
        break;

      case 44100:
        if (type == Type::K) {
          const std::vector<std::array<float, 3>> num = {{1.5308, -2.6510, 1.1691}, {0.9996, -1.9991, 0.9996}};
          const std::vector<std::array<float, 3>> den = {{1.0000, -1.6637, 0.7126}, {1.0000, -1.9892, 0.9892}};
          const std::vector<float> scale = {1, 1, 1};
          m_filter = std::make_shared<SOSFilter>(num, den, scale);
        }
        break;

      case 48000:
        if (type == Type::K) {
          const std::vector<std::array<float, 3>> num = {{1.5351, -2.6917, 1.1984}, {1, -2, 1}};
          const std::vector<std::array<float, 3>> den = {{1.0000, -1.6907, 0.7325}, {1.0000, -1.9900, 0.9901}};
          const std::vector<float> scale = {1, 1, 1};
          m_filter = std::make_shared<SOSFilter>(num, den, scale);
        }
        break;

      default:
        // need to recalculate for another sample rate
        m_type = Type::Raw;
        break;
    }
  }

  [[nodiscard]] Type type() const noexcept { return m_type; }

  std::vector<float> process(const std::vector<float>& x) noexcept {
    if (!m_filter || m_type == Type::Raw) {
      // don't modify the input signal
      return x;
    }
    return m_filter->process(x);
  }

 private:
  std::shared_ptr<SOSFilter> m_filter;
  Type m_type{Type::Raw};
};

}  // namespace util::sound
