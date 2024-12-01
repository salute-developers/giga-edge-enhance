#pragma once

#include <string>
#include <vector>
#include <valarray>
#include <queue>
#include <set>
#include <tuple>
#include <memory>
#include <utility>
#include "spench/util/sound/sound_stream.h"
#include "spench/include/stream_stat.h"

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
  std::vector<T> getSpec() {
    std::vector<float> result;
    result.reserve(real.size() + imag.size());

    // Copy real part
    result.insert(result.end(), std::begin(real), std::end(real));
    // Copy imag part
    result.insert(result.end(), std::begin(imag), std::end(imag));

    return result;
  }
};

template <typename T>
struct StreamCurrentMetrics {
  T m_speech_energy_avg{1e-3};
  T m_noise_energy_avg{1e-3};
  T m_e1_max{0.0};
};

template <typename T>
struct SpenchContext {
  uint64_t m_id;
  StreamCurrentMetrics<T> m_metrics;
  StatInfo m_stat;
  std::vector<T> m_prev_input_chunk;
  std::queue<ComplexNumber<T>> m_prev_features;  // params.real_buf and params.imag_buf in python
  ComplexNumber<T> m_prev_spectrogram;  // params.prev_out in python
  std::unique_ptr<util::sound::ISoundStream<float, float>> m_input_sound_stream_ptr;
  std::unique_ptr<util::sound::ISoundStream<float, float>> m_output_sound_stream_ptr;
  virtual ~SpenchContext() = default;
};

template <typename T>
struct DenoiseContextQ: public SpenchContext<T> {
  std::vector<T> m_fullband_hidden;
  std::vector<T> m_fullband_cell_state;
  std::vector<T> m_subband_hidden;
  std::vector<T> m_subband_cell_state;
};

template <typename T>
struct DenoiseContextF: public SpenchContext<T> {
  std::vector<T> m_fullband_hidden;
  std::vector<T> m_fullband_cell_state;
  std::vector<T> m_noisy_mag;
  std::vector<T> m_prev_fb_out;
};

template <typename T>
struct DereverbContext: public SpenchContext<T> {
  std::vector<T> m_noisy_spec;
  std::vector<T> m_prev_out;
  std::vector<T> phase;
};
}  // namespace spench
