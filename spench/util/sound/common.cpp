#include "util/sound/common.h"

#include <dsplib/math.h>
#include <algorithm>

namespace util::sound {

float quantile(const dsplib::arr_real& x, float q, bool sorted) {
  const int n = x.size();
  int idx = std::round((n - 1) * q);
  idx = dsplib::min(idx, n - 1);
  idx = dsplib::max(idx, 0);
  if (!sorted) {
    const auto p = dsplib::sort(x).first;
    return p[idx];
  }
  return x[idx];
}

}  // namespace util::sound
