#pragma once

#include <cstdint>
#include <vector>

#include "src/feature_params.h"

namespace gigaedge::spench {
namespace fe {
// Applies a given window on input
//    s'(n) = w(n) * s(n) where w(n) are the window coefficients

// we always have Windowing(n, periodic=True) equal to Windowing(n + 1, periodic=False)[:-1].
template <typename T>
class Windowing {
 public:
  Windowing(int64_t n, WindowType window, uint32_t hop_size, bool is_periodic = false);

  std::vector<T> apply(const std::vector<T>& input) const;
  void apply_in_place(std::vector<T>& input) const;
  void apply_frame_in_place(T* frame, size_t frame_size) const;
  void sum_square_in_place(std::vector<T>& input) const;

 private:
  int64_t window_length_;
  WindowType window_type_;
  std::vector<T> coefs_;
  std::vector<T> coefs_pow_2_;
  uint32_t hop_size_;
  bool window_periodic_;
};

}  // namespace fe
}  // namespace gigaedge::spench
