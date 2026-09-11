#include "util/sound/common.h"

#include <dsplib/math.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace util::sound {

float quantile(const dsplib::arr_real& x, float q, bool sorted) {
  if (x.empty()) {
    throw std::invalid_argument("Cannot calculate a quantile of empty data.");
  }
  if (!std::isfinite(q) || q < 0.0F || q > 1.0F) {
    throw std::invalid_argument("Quantile must be finite and in [0, 1].");
  }
  const size_t idx = static_cast<size_t>(std::round((x.size() - 1) * q));
  if (!sorted) {
    const auto p = dsplib::sort(x).first;
    return p[idx];
  }
  return x[idx];
}

}  // namespace util::sound
