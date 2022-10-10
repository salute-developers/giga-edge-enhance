#pragma once

#include <cstdint>
#include <vector>

#include "spench/src/feature_params.h"

namespace spench {
namespace fe {
// Applies a given window on input
//    s'(n) = w(n) * s(n) where w(n) are the window coefficients

// we always have Windowing(n, periodic=True) equal to Windowing(n + 1, periodic=False)[:-1].
template <typename T>
class Windowing {
 public:
  Windowing(int64_t n, WindowType window, uint32_t hop_size, bool is_periodic = false);

  std::vector<T> Apply(const std::vector<T>& input) const;
  void ApplyInPlace(std::vector<T>& input) const;

  void sumSquareInPlace(std::vector<T>& input) const;

 private:
  int64_t m_window_length;
  WindowType m_window_type;
  std::vector<T> m_coefs;
  std::vector<T> m_coefs_pow_2;
  uint32_t m_hop_size;
  bool m_window_periodic;
};

}  // namespace fe
}  // namespace spench
