#pragma once

#include "src/fft_calculator.h"
#include "src/ifft_calculator.h"

namespace gigaedge::enhance::fe {

// Mutable transform buffers belong to one audio stream. Model weights and
// window coefficients can still be shared across streams.
template <typename T>
struct FeatureWorkspace {
  explicit FeatureWorkspace(const FeatureParams& params)
      : fft(params.nfft(), params.pow_spec_feat_sz(), params.frames_num_for_istft),
        ifft(params.pow_spec_feat_sz(), params.nfft(), params.frames_num_for_istft) {}

  FftCalculator<T> fft;
  IFftCalculator<T> ifft;
};

}  // namespace gigaedge::enhance::fe
