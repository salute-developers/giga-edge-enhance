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
void FftCalculator<T>::CalculateFft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft) {
  std::copy(begin, begin + n_samples, m_in_fft_buf.begin());

  pocketfft::shape_t shape{1, m_nfft};
  std::ptrdiff_t sz = sizeof(fft::buffer_type);
  pocketfft::stride_t stride{static_cast<int32_t>(sz * m_nfft), sz};
  pocketfft::stride_t stride_out{static_cast<int32_t>(2 * sz * m_feature_size), 2 * sz};
  size_t axes = 1;
  pocketfft::r2c<fft::buffer_type>(shape, stride,
                                   stride_out, axes,
                                   pocketfft::FORWARD,
                                   m_in_fft_buf.data(),
                                   m_out_fft_buf.data(),
                                   1.0);

  for (size_t i = 0; i < m_feature_size; ++i) {
    dft.real[i] = m_out_fft_buf[i].real();
    dft.imag[i] = m_out_fft_buf[i].imag();
  }
}

template <typename T>
FftCalculator<T>::~FftCalculator() {}

template class FftCalculator<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
