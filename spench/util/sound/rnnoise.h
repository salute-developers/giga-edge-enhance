#pragma once

#include <dsplib/resample.h>

#include <memory>
#include <vector>

#include "util/sound/common.h"

typedef struct DenoiseState DenoiseState;

namespace util::sound {

constexpr int32_t RNNOISE_FS = 48000;  //< native rnnoise sample rate

// rnnoise wrapper
class RNNoise {
 public:
  explicit RNNoise(size_t in_fs, bool only_vad = false);

  struct Result {
    dsplib::arr_real out;  ///< denoised signal
    float vad{0.0f};       ///< voice activity score [0, 1]
  };

  Result process(const std::vector<float>& x);

  size_t stride() const noexcept { return m_in_stride; }

  size_t sample_rate() const noexcept { return m_fs; }

 private:
  size_t m_fs;
  size_t m_frame_len;
  size_t m_in_stride;
  FIFOBuffer<float> m_rsbuf;
  FIFOBuffer<float> m_rnbuf;
  dsplib::FIRResampler m_in_rsmp;
  dsplib::FIRResampler m_out_rsmp;
  std::shared_ptr<DenoiseState> m_rnnoise;
  float m_vad{0.0};
  bool m_only_vad{false};
};

}  // namespace util::sound
