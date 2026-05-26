#include "spench/src/fft_calculator.h"

#include <algorithm>
#include <cmath>

#include "contrib/pocketfft/pocketfft_hdronly.h"
#include "spench/src/spench_context.h"

namespace spench {
namespace fe {
template <typename T>
FftCalculator<T>::FftCalculator(const uint32_t nfft, const uint32_t feature_size, const uint32_t frames_num_for_stft)
    : nfft_(nfft),
      feature_size_(feature_size),
      frames_num_for_stft_(frames_num_for_stft),
      out_fft_buf_(feature_size, 0.0),
      in_fft_buf_(nfft_, 0.0) {}

template <typename T>
void FftCalculator<T>::calculate_stft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft) {
  calculate_fft(n_samples, begin, dft);
}

template <typename T>
void FftCalculator<T>::prepare_input_buffer(size_t n_samples, const T* begin) {
  if (n_samples > nfft_) {
    throw std::runtime_error("prepare_input_buffer: n_samples > nfft");
  }

  // Zero-pad up to nfft.
  std::fill(in_fft_buf_.begin(), in_fft_buf_.end(), fft::buffer_type(0));

  // Copy input samples into FFT input buffer.
  std::copy(begin, begin + n_samples, in_fft_buf_.begin());
}

template <typename T>
void FftCalculator<T>::run_pocket_fft() {
  pocketfft::shape_t shape{1, nfft_};

  const std::ptrdiff_t sz = sizeof(fft::buffer_type);

  // Input: real [1, nfft]
  pocketfft::stride_t stride_in{
      static_cast<std::ptrdiff_t>(sz * nfft_),
      static_cast<std::ptrdiff_t>(sz)
  };

  // Output: complex interleaved [1, feature_size]
  pocketfft::stride_t stride_out{
      static_cast<std::ptrdiff_t>(2 * sz * feature_size_),
      static_cast<std::ptrdiff_t>(2 * sz)
  };

  constexpr size_t axes = 1;
  pocketfft::r2c<fft::buffer_type>(shape,
                                  stride_in,
                                  stride_out,
                                  axes,
                                  pocketfft::FORWARD,
                                  in_fft_buf_.data(),
                                  out_fft_buf_.data(),
                                  1.0);
}

template <typename T>
void FftCalculator<T>::copy_output_to(ComplexNumber<T>& dft) const {
  for (size_t i = 0; i < feature_size_; ++i) {
    dft.real[i] = static_cast<T>(out_fft_buf_[i].real());
    dft.imag[i] = static_cast<T>(out_fft_buf_[i].imag());
  }
}

template <typename T>
void FftCalculator<T>::calculate_fft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft) {
  if (dft.size() != feature_size_) {
    throw std::runtime_error("calculate_fft: dft.size() != feature_size");
  }
  prepare_input_buffer(static_cast<size_t>(n_samples), begin);
  run_pocket_fft();
  copy_output_to(dft);
}

template <typename T>
FftCalculator<T>::~FftCalculator() {}

template class FftCalculator<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
