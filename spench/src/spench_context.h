#pragma once

#include <string>
#include <vector>
#include <valarray>
#include <queue>
#include <set>
#include <tuple>
#include <memory>
#include <utility>
#include <algorithm>
#include "spench/util/sound/sound_stream.h"
#include "spench/stream_stat.h"

namespace spench {

template <typename T>
struct ComplexNumber {
  std::valarray<T> real;
  std::valarray<T> imag;

  ComplexNumber() = default;

  ComplexNumber(uint32_t real_size, uint32_t imag_size):
    real(real_size), imag(imag_size)
  {}

  ComplexNumber(T real_vals, uint32_t real_count, T imag_vals, uint32_t imag_count):
    real(real_vals, real_count), imag(imag_vals, imag_count)
  {}

  ComplexNumber(const std::valarray<T>& init_real, const std::valarray<T>& init_imag):
    real(init_real), imag(init_imag)
  {}

  ComplexNumber(const std::vector<T>& init_real, const std::vector<T>& init_imag):
    real(init_real.data(), init_real.size()), imag(init_imag.data(), init_imag.size())
  {}

  std::vector<T> getMagnitude() {
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

    std::copy(std::begin(real),  std::end(real),  &new_real[0]);
    std::copy(std::begin(imag),  std::end(imag),  &new_imag[0]);

    std::copy(std::begin(other.real), std::end(other.real), &new_real[0] + n0);
    std::copy(std::begin(other.imag), std::end(other.imag), &new_imag[0] + n0);

    real = std::move(new_real);
    imag = std::move(new_imag);
  }
  // Writes magnitude into dst in (freq, time) layout:
  // dst[f * num_frames + frame_idx] = magnitude[f]
  void writeMagnitudeInterleaved(std::vector<float>& dst,
                                std::size_t frame_idx,
                                std::size_t num_frames) const {
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
  std::vector<T> getSpec() {
    std::vector<float> result;
    result.reserve(real.size() + imag.size());

    // Copy real part
    result.insert(result.end(), std::begin(real), std::end(real));
    // Copy imag part
    result.insert(result.end(), std::begin(imag), std::end(imag));

    return result;
  }
  size_t size() const {
    return real.size();
  }
  bool empty() const {
    return real.size() == 0 &&
           imag.size() == 0;
  }
};

template <typename T>
static inline void UnpackComplexMask(
    const T* src,                     // ORT buffer: [F, T, 2]
    std::size_t num_feats,             // F
    std::size_t num_frames,            // T
    spench::ComplexNumber<float>& dst  // stores time-major: [t * F + f]
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
struct SpenchContext {
  uint64_t id;
  StreamCurrentMetrics<T> metrics;
  StatInfo stat;
  std::vector<T> prev_input_chunk;
  std::queue<ComplexNumber<T>> prev_features;  // params.real_buf and params.imag_buf in python
  ComplexNumber<T> prev_spectrogram;  // params.prev_out in python
  std::unique_ptr<util::sound::ISoundStream> input_sound_stream_ptr;
  std::unique_ptr<util::sound::ISoundStream> output_sound_stream_ptr;
  virtual ~SpenchContext() = default;
};

template <typename T>
struct DenoiseContextQ: public SpenchContext<T> {
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
struct DenoiseContextF: public SpenchContext<T> {
  std::vector<T> fullband_hidden;
  std::vector<T> fullband_cell_state;
  std::vector<T> noisy_mag;
  std::vector<T> prev_fb_out;
};

template <typename T>
struct DereverbContext: public SpenchContext<T> {
  std::vector<T> noisy_spec;
  std::vector<T> prev_out;
  std::vector<T> phase;
};
}  // namespace spench
