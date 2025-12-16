#include "util/sound/rnnoise.h"

#include <memory>
#include <rnnoise.h>

namespace util::sound {

namespace {
constexpr int RNNOISE_FS = 48000;          //< rnnoise input sample rate
constexpr float RNNOISE_SCALE = 32767.0F;  //< rnnoise input must be float in 16-bit range (-32767 : 32767)
}  // namespace

RNNoise::RNNoise(size_t in_fs)
    : m_in_rsmp{RNNOISE_FS, static_cast<int>(in_fs)}, m_out_rsmp{static_cast<int>(in_fs), RNNOISE_FS} {
  m_frame_len = rnnoise_get_frame_size();
  auto ptr = rnnoise_create(nullptr);
  m_rnnoise = std::shared_ptr<DenoiseState>(ptr, rnnoise_destroy);
}

RNNoise::Result RNNoise::process(const dsplib::arr_real& x) {
  dsplib::arr_real res;

  m_rsbuf.write(x.to_vec());
  auto rs_len = m_in_rsmp.prev_size(m_rsbuf.size());
  if (rs_len != 0) {
    auto s = m_rsbuf.read(rs_len);
    auto rs = m_in_rsmp.process(s);
    m_rnbuf.write(rs.to_vec());
  }

  dsplib::arr_real out(m_frame_len);
  while (static_cast<size_t>(m_rnbuf.size()) >= m_frame_len) {
    dsplib::arr_real in = m_rnbuf.read(m_frame_len);
    in *= RNNOISE_SCALE;
    m_vad = rnnoise_process_frame(m_rnnoise.get(), out.data(), in.data());
    out /= RNNOISE_SCALE;
    res |= m_out_rsmp.process(out);
  }
  return Result{.out = res, .vad = m_vad};
}

}  // namespace util::sound
