#include "util/sound/rnnoise.h"

#include <vector>
#include <utility>

#include <rnnoise.h>

namespace util::sound {

namespace {
constexpr float RNNOISE_SCALE = 32767.0F;  //< rnnoise input must be float in 16-bit range (-32767 : 32767)
}  // namespace

RNNoise::RNNoise(size_t in_fs, bool only_vad)
    : m_fs{in_fs},
      m_in_rsmp{RNNOISE_FS, static_cast<int32_t>(in_fs)},
      m_out_rsmp{only_vad ? 1 : static_cast<int32_t>(in_fs), only_vad ? 1 : RNNOISE_FS},
      m_only_vad{only_vad} {
  m_frame_len = rnnoise_get_frame_size();
  m_in_stride = in_fs / (RNNOISE_FS / m_frame_len);  // usually 10 ms
  auto ptr = rnnoise_create(nullptr);
  m_rnnoise = std::shared_ptr<DenoiseState>(ptr, rnnoise_destroy);
}

RNNoise::Result RNNoise::process(const std::vector<float>& x) {
  m_rsbuf.write(x);
  auto rs_len = m_in_rsmp.prev_size(m_rsbuf.size());
  if (rs_len != 0) {
    auto s = m_rsbuf.read(rs_len);
    auto rs = m_in_rsmp.process(s);
    m_rnbuf.write(rs.to_vec());
  }

  dsplib::arr_real res;
  dsplib::arr_real out(m_frame_len);
  while (m_rnbuf.size() >= m_frame_len) {
    dsplib::arr_real in = m_rnbuf.read(m_frame_len);
    in *= RNNOISE_SCALE;
    m_vad = rnnoise_process_frame(m_rnnoise.get(), out.data(), in.data());

    if (!m_only_vad) {
      out /= RNNOISE_SCALE;
      res |= m_out_rsmp.process(out);
    }
  }
  return Result{ std::move(res), m_vad };
}

}  // namespace util::sound
