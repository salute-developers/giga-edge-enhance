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

bool SetResamplerIoRatio(const ResampleWrapper& resampler_state, double io_ratio);

std::vector<float> ResampleVec(const ResampleWrapper& resampler_state, const std::vector<float>& v, size_t in_rate,
                               size_t out_rate);

std::vector<float> ResampleVecRT(const std::vector<float>& v, size_t in_rate, size_t out_rate);

std::vector<float> FlushVec(const ResampleWrapper& resampler_state);

}  // namespace sound
}  // namespace util
