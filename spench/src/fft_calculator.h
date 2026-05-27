#pragma once

#include <complex>
#include <vector>

#include "spench/src/spench_context.h"
#include "spench/src/feature_params.h"

namespace pocketfft {
using ShapeT  = std::vector<std::size_t>;
using StrideT = std::vector<std::ptrdiff_t>;
}  // namespace pocketfft

namespace spench {
namespace fe {

template <typename T>
class FftCalculator {
 public:
  explicit FftCalculator(const uint32_t nfft, const uint32_t feature_size, const uint32_t frames_num_for_stft = 2);
  ~FftCalculator();
  void calculate_stft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft);

 private:
  const uint32_t nfft;
  const uint32_t feature_size;
  const uint32_t frames_num_for_stft;

  std::vector<std::complex<fft::BufferType>> out_fft_buf_;
  std::vector<fft::BufferType> in_fft_buf_;

  void calculate_fft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft);
  void prepare_input_buffer(size_t n_samples, const T* begin);
  void run_pocket_fft();
  void copy_output_to(ComplexNumber<T>& dft) const;
};
}  // namespace fe
}  // namespace spench
