#include "spench/src/fft_calculator.h"

#include <algorithm>
#include <cmath>

#include "contrib/pocketfft/pocketfft_hdronly.h"
#include "spench/src/spench_context.h"

namespace spench {
namespace fe {
template <typename T>
FftCalculator<T>::FftCalculator(const uint32_t nfft, const uint32_t feature_size, const uint32_t frames_num_for_stft)
    : m_nfft(nfft),
      m_feature_size(feature_size),
      m_frames_num_for_stft(frames_num_for_stft),
      m_out_fft_buf(feature_size, 0.0),
      m_in_fft_buf(m_nfft, 0.0) {}

template <typename T>
void FftCalculator<T>::CalculateSTFT(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft) {
  CalculateFft(n_samples, begin, dft);
}

template <typename T>
void FftCalculator<T>::PrepareInputBuffer(size_t n_samples, const T* begin) {
  if (n_samples > m_nfft) {
    throw std::runtime_error("PrepareInputBuffer: n_samples > nfft");
  }

  // Zero-pad up to nfft.
  std::fill(m_in_fft_buf.begin(), m_in_fft_buf.end(), fft::buffer_type(0));

  // Copy input samples into FFT input buffer.
  std::copy(begin, begin + n_samples, m_in_fft_buf.begin());
}

template <typename T>
void FftCalculator<T>::RunPocketFft() {
  pocketfft::shape_t shape{1, m_nfft};

  const std::ptrdiff_t sz = sizeof(fft::buffer_type);

  // Input: real [1, nfft]
  pocketfft::stride_t stride_in{
      static_cast<std::ptrdiff_t>(sz * m_nfft),
      static_cast<std::ptrdiff_t>(sz)
  };

  // Output: complex interleaved [1, feature_size]
  pocketfft::stride_t stride_out{
      static_cast<std::ptrdiff_t>(2 * sz * m_feature_size),
      static_cast<std::ptrdiff_t>(2 * sz)
  };

  constexpr size_t axes = 1;
  pocketfft::r2c<fft::buffer_type>(shape,
                                  stride_in,
                                  stride_out,
                                  axes,
                                  pocketfft::FORWARD,
                                  m_in_fft_buf.data(),
                                  m_out_fft_buf.data(),
                                  1.0);
}

template <typename T>
void FftCalculator<T>::CopyOutputTo(ComplexNumber<T>& dft) const {
  for (size_t i = 0; i < m_feature_size; ++i) {
    dft.real[i] = static_cast<T>(m_out_fft_buf[i].real());
    dft.imag[i] = static_cast<T>(m_out_fft_buf[i].imag());
  }
}

template <typename T>
void FftCalculator<T>::CalculateFft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft) {
  if (dft.size() != m_feature_size) {
    throw std::runtime_error("CalculateFft: dft.size() != feature_size");
  }
  PrepareInputBuffer(static_cast<size_t>(n_samples), begin);
  RunPocketFft();
  CopyOutputTo(dft);
}

template <typename T>
FftCalculator<T>::~FftCalculator() {}

template class FftCalculator<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
