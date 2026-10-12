#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <valarray>
#include <vector>

#include "spench/stream_stat.h"
#include "src/denoiser_model_adviser.h"
#include "util/sound/sound_stream.h"

namespace gigaedge::spench {

template <typename T>
struct ComplexNumber {
  std::valarray<T> real;
  std::valarray<T> imag;

  ComplexNumber() = default;

  ComplexNumber(uint32_t real_size, uint32_t imag_size) : real(real_size), imag(imag_size) {}

  ComplexNumber(T real_vals, uint32_t real_count, T imag_vals, uint32_t imag_count)
      : real(real_vals, real_count), imag(imag_vals, imag_count) {}

  ComplexNumber(const std::valarray<T>& init_real, const std::valarray<T>& init_imag)
      : real(init_real), imag(init_imag) {}

  ComplexNumber(const std::vector<T>& init_real, const std::vector<T>& init_imag)
      : real(init_real.data(), init_real.size()), imag(init_imag.data(), init_imag.size()) {}

  std::vector<T> get_magnitude() {
    std::valarray<T> exp(2, real.size());
    std::valarray<T> valarray_mag = std::sqrt(std::pow(real, exp) + std::pow(imag, exp));
    std::vector<T> vector_mag(std::begin(valarray_mag), std::end(valarray_mag));
    return vector_mag;
  }
  void append(const ComplexNumber<T>& other) {
    const std::size_t n0 = real.size();
    const std::size_t n1 = other.real.size();

    std::valarray<T> new_real(n0 + n1);
    std::valarray<T> new_imag(n0 + n1);

    std::copy(std::begin(real), std::end(real), &new_real[0]);
    std::copy(std::begin(imag), std::end(imag), &new_imag[0]);

    std::copy(std::begin(other.real), std::end(other.real), &new_real[0] + n0);
    std::copy(std::begin(other.imag), std::end(other.imag), &new_imag[0] + n0);

    real = std::move(new_real);
    imag = std::move(new_imag);
  }

  void resize(std::size_t size) {
    real.resize(size);
    imag.resize(size);
  }

  void copy_to(ComplexNumber<T>& dst, std::size_t offset) const {
    if (real.size() != imag.size()) {
      throw std::runtime_error("ComplexNumber::CopyTo: real/imag size mismatch");
    }
    if (dst.real.size() != dst.imag.size()) {
      throw std::runtime_error("ComplexNumber::CopyTo: destination real/imag size mismatch");
    }
    if (offset + real.size() > dst.real.size()) {
      throw std::runtime_error("ComplexNumber::CopyTo: destination is too small");
    }

    std::copy(std::begin(real), std::end(real), &dst.real[0] + offset);
    std::copy(std::begin(imag), std::end(imag), &dst.imag[0] + offset);
  }

  void copy_frame_to(ComplexNumber<T>& dst, std::size_t feat_num, std::size_t frame_idx) const {
    if (real.size() != imag.size()) {
      throw std::runtime_error("ComplexNumber::CopyFrameTo: real/imag size mismatch");
    }
    if (dst.real.size() != dst.imag.size()) {
      throw std::runtime_error("ComplexNumber::CopyFrameTo: destination real/imag size mismatch");
    }
    if (feat_num == 0) {
      throw std::runtime_error("ComplexNumber::CopyFrameTo: feat_num == 0");
    }
    if (dst.real.size() != feat_num) {
      throw std::runtime_error("ComplexNumber::CopyFrameTo: destination size mismatch");
    }

    const std::size_t src_offset = frame_idx * feat_num;
    if (src_offset + feat_num > real.size()) {
      throw std::runtime_error("ComplexNumber::CopyFrameTo: frame index is out of range");
    }

    std::copy_n(&real[0] + src_offset, feat_num, &dst.real[0]);
    std::copy_n(&imag[0] + src_offset, feat_num, &dst.imag[0]);
  }

  static ComplexNumber<T> concat_parts(const std::vector<ComplexNumber<T>>& parts) {
    if (parts.empty()) {
      return {};
    }

    std::size_t total_size = 0;
    for (const auto& part : parts) {
      if (part.real.size() != part.imag.size()) {
        throw std::runtime_error("ComplexNumber::Concat: real/imag size mismatch in part");
      }
      total_size += part.real.size();
    }

    ComplexNumber<T> result(static_cast<uint32_t>(total_size), static_cast<uint32_t>(total_size));

    std::size_t offset = 0;
    for (const auto& part : parts) {
      part.copy_to(result, offset);
      offset += part.real.size();
    }

    return result;
  }

  static ComplexNumber<T> concat_with_head(const ComplexNumber<T>& head, const ComplexNumber<T>& tail) {
    if (head.real.size() != head.imag.size()) {
      throw std::runtime_error("ComplexNumber::ConcatWithHead: head real/imag size mismatch");
    }
    if (tail.real.size() != tail.imag.size()) {
      throw std::runtime_error("ComplexNumber::ConcatWithHead: tail real/imag size mismatch");
    }

    const std::size_t total_size = head.real.size() + tail.real.size();
    ComplexNumber<T> result(static_cast<uint32_t>(total_size), static_cast<uint32_t>(total_size));

    head.copy_to(result, 0);
    tail.copy_to(result, head.real.size());

    return result;
  }
  // Writes magnitude into dst in (freq, time) layout:
  // dst[f * num_frames + frame_idx] = magnitude[f]
  void write_magnitude_interleaved(std::vector<float>& dst, std::size_t frame_idx, std::size_t num_frames) const {
    const std::size_t n = real.size();

    // dst must be pre-sized to n * num_frames
    float* out = dst.data() + frame_idx;

    for (std::size_t f = 0; f < n; ++f) {
      const float re = static_cast<float>(real[f]);
      const float im = static_cast<float>(imag[f]);
      *out = std::sqrt(re * re + im * im);
      out += num_frames;  // move to next freq bin, keep time as fastest axis
    }
  }
  std::vector<T> get_spec() {
    std::vector<float> result;
    result.reserve(real.size() + imag.size());

    // Copy real part
    result.insert(result.end(), std::begin(real), std::end(real));
    // Copy imag part
    result.insert(result.end(), std::begin(imag), std::end(imag));

    return result;
  }
  size_t size() const { return real.size(); }
  bool empty() const { return real.size() == 0 && imag.size() == 0; }
};

template <typename T>
static inline void unpack_complex_mask(const T* src,                                // ORT buffer: [F, T, 2]
                                       std::size_t num_feats,                       // F
                                       std::size_t num_frames,                      // T
                                       gigaedge::spench::ComplexNumber<float>& dst  // stores time-major: [t * F + f]
) {
  const std::size_t total = num_feats * num_frames;
  dst.real.resize(total);
  dst.imag.resize(total);

  for (std::size_t t = 0; t < num_frames; ++t) {
    const std::size_t dst_base = t * num_feats;

    for (std::size_t f = 0; f < num_feats; ++f) {
      const std::size_t src_idx = (f * num_frames + t) * 2;  // (f,t,0)
      dst.real[dst_base + f] = static_cast<float>(src[src_idx]);
      dst.imag[dst_base + f] = static_cast<float>(src[src_idx + 1]);
    }
  }
}

template <typename T>
struct StreamCurrentMetrics {
  T speech_energy_avg{1e-3};
  T noise_energy_avg{1e-3};
  T e1_max{0.0};
};

template <typename T>
struct Context {
  uint64_t id;
  StreamCurrentMetrics<T> metrics;
  std::unique_ptr<DenoiserModelAdviser<T>> snr_adviser;
  StatInfo stat;
  std::vector<T> prev_input_chunk;
  std::queue<ComplexNumber<T>> prev_features;  // params.real_buf and params.imag_buf in python
  ComplexNumber<T> prev_spectrogram;           // params.prev_out in python
  std::unique_ptr<::util::sound::ISoundStream> input_sound_stream_ptr;
  std::unique_ptr<::util::sound::ISoundStream> output_sound_stream_ptr;
  virtual ~Context() = default;
};

template <typename T>
struct DenoiseContextDiscriminativeHq : public Context<T> {
  std::vector<T> fullband_hidden;
  std::vector<T> fb_snr_noise_psd;
  std::vector<T> fb_snr_noise_pg;
  std::vector<T> sb_snr_noise_psd;
  std::vector<T> sb_snr_noise_pg;
  std::vector<T> fb_conv1_buf;
  std::vector<T> fb_conv3_buf;
  std::vector<float> noisy_mag;
};

template <typename T>
struct DenoiseContextHybrid : public DenoiseContextDiscriminativeHq<T> {
  std::vector<T> stage2_hidden;
  std::vector<T> noisy_ri;
};

template <typename T>
struct DenoiseContextDiscriminativeNb : public Context<T> {
  std::vector<T> fullband_hidden_state;
  std::vector<T> fullband_cell_state;
  std::vector<T> fb_conv1_buf;
  std::vector<T> fb_conv3_buf;
  std::vector<float> noisy_mag;
};

}  // namespace gigaedge::spench
