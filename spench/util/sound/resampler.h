#pragma once

#include <spench/util/sound/resampler_fwd.h>
#include <memory>
#include <vector>

namespace util {
namespace sound {

struct ResamplerDeleter {
  void operator()(soxr_t resampler) const;
};

using ResampleWrapper = std::unique_ptr<soxr, ResamplerDeleter>;

ResampleWrapper BuildResampler(size_t in_rate, size_t out_rate, size_t channels);

template <typename T>
std::vector<T> ResampleVec(ResampleWrapper& resampler_state, const std::vector<T>& v, size_t in_rate,
                           size_t out_rate);

template <typename T>
std::vector<T> ResampleVecRT(const std::vector<T>& v, size_t in_rate, size_t out_rate);

template <typename T>
std::vector<T> FlushVec(ResampleWrapper& resampler_state);
}  // namespace sound
}  // namespace util
