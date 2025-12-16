#pragma once

#include <dsplib/resample.h>
#include <memory>

#include "util/sound/common.h"

typedef struct DenoiseState DenoiseState;

namespace util::sound {

// rnnoise wrapper
class RNNoise {
 public:
  explicit RNNoise(size_t in_fs);

  struct Result {
    dsplib::arr_real out;  ///< denoised signal
    float vad{0.0f};       ///< voice activity score [0, 1]
  };

  Result process(const dsplib::arr_real& x);

  size_t stride() const noexcept { return m_frame_len; }

 private:
  size_t m_frame_len;
  FIFOBuffer<float> m_rsbuf;
  FIFOBuffer<float> m_rnbuf;
  dsplib::FIRResampler m_in_rsmp;
  dsplib::FIRResampler m_out_rsmp;
  std::shared_ptr<DenoiseState> m_rnnoise;
  float m_vad{0.0};
};

}  // namespace util::sound
