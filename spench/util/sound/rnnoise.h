#pragma once

#include <dsplib/resample.h>

#include <memory>
#include <vector>

#include "util/sound/common.h"

typedef struct DenoiseState DenoiseState;

namespace util::sound {

constexpr int32_t rnnoise_fs = 48000;  //< native rnnoise sample rate

// rnnoise wrapper
class RNNoise {
 public:
  explicit RNNoise(size_t in_fs, bool only_vad = false);

  struct Result {
    dsplib::arr_real out;  ///< denoised signal
    float vad{0.0f};       ///< voice activity score [0, 1]
  };

  Result process(const std::vector<float>& x);

  size_t stride() const noexcept { return in_stride_; }

  size_t sample_rate() const noexcept { return fs_; }

 private:
  size_t fs_;
  size_t frame_len_;
  size_t in_stride_;
  FIFOBuffer<float> rsbuf_;
  FIFOBuffer<float> rnbuf_;
  dsplib::FIRResampler in_rsmp_;
  dsplib::FIRResampler out_rsmp_;
  std::shared_ptr<DenoiseState> rnnoise_;
  float vad_{0.0};
  bool only_vad_{false};
};

}  // namespace util::sound
