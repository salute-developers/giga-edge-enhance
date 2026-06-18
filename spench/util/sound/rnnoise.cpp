#include "util/sound/rnnoise.h"

#include <rnnoise.h>

#include <memory>
#include <utility>
#include <vector>

namespace util::sound {

namespace {
constexpr float rnnoise_scale = 32767.0F;  //< rnnoise input must be float in 16-bit range (-32767 : 32767)
}  // namespace

RNNoise::RNNoise(size_t in_fs, bool only_vad)
    : fs_{in_fs},
      in_rsmp_{rnnoise_fs, static_cast<int32_t>(in_fs)},
      out_rsmp_{only_vad ? 1 : static_cast<int32_t>(in_fs), only_vad ? 1 : rnnoise_fs},
      only_vad_{only_vad} {
  frame_len_ = rnnoise_get_frame_size();
  in_stride_ = in_fs / (rnnoise_fs / frame_len_);  // usually 10 ms
  auto ptr = rnnoise_create(nullptr);
  rnnoise_ = std::shared_ptr<DenoiseState>(ptr, rnnoise_destroy);
}

RNNoise::Result RNNoise::process(const std::vector<float>& x) {
  rsbuf_.write(x);
  auto rs_len = in_rsmp_.prev_size(rsbuf_.size());
  if (rs_len != 0) {
    auto s = rsbuf_.read(rs_len);
    auto rs = in_rsmp_.process(s);
    rnbuf_.write(rs.to_vec());
  }

  dsplib::arr_real res;
  dsplib::arr_real out(frame_len_);
  while (rnbuf_.size() >= frame_len_) {
    dsplib::arr_real in = rnbuf_.read(frame_len_);
    in *= rnnoise_scale;
    vad_ = rnnoise_process_frame(rnnoise_.get(), out.data(), in.data());

    if (!only_vad_) {
      out /= rnnoise_scale;
      res |= out_rsmp_.process(out);
    }
  }
  return Result{std::move(res), vad_};
}

}  // namespace util::sound
