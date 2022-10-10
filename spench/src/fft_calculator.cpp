#include "spench/src/fft_calculator.h"

#include <cmath>
#include <algorithm>
#include <iostream>

#include "spench/src/denoise_context.h"

namespace spench {
namespace fe {
template <typename T>
FftCalculator<T>::FftCalculator(const uint32_t nfft,
                                const uint32_t feature_size,
                                const uint32_t frames_num_for_stft)
    : m_nfft(nfft)
     , m_feature_size(feature_size)
     , m_frames_num_for_stft(frames_num_for_stft)
#ifdef BUILD_FFTW3_FE
     , m_out_fft_buf(2 * feature_size, 0.0)
#elif BUILD_POCKETFFT_FE
     , m_out_fft_buf(feature_size, 0.0)
#endif
     , m_in_fft_buf(m_nfft, 0.0) {
#ifdef BUILD_FFTW3_FE
  m_fft_plan = fftw_plan_dft_r2c_1d(m_nfft,
                                    m_in_fft_buf.data(),
                                    reinterpret_cast<fftw_complex *>(m_out_fft_buf.data()),
                                    FFTW_MEASURE);
#endif
}

template<typename T>
void FftCalculator<T>::CalculateSTFT(const size_t& n_samples,
                                     const T* begin,
                                     ComplexNumber<T>& dft) {
  CalculateFftw3(n_samples, begin, dft);
}

template<typename T>
void FftCalculator<T>::CalculateFftw3(const size_t& n_samples,
                                      const T* begin,
                                      ComplexNumber<T>& dft) {
  std::copy(begin, begin + n_samples, m_in_fft_buf.begin());

#ifdef BUILD_FFTW3_FE
  std::fill(m_out_fft_buf.begin(), m_out_fft_buf.end(), 0.0);
  fftw_execute(m_fft_plan);

#elif BUILD_POCKETFFT_FE
  pocketfft::shape_t shape{1, m_nfft};
  std::ptrdiff_t sz = sizeof(fft::buffer_type);
  pocketfft::stride_t stride{static_cast<int32_t>(sz * m_nfft), sz};
  pocketfft::stride_t stride_out{static_cast<int32_t>(2 * sz * m_feature_size), 2 * sz};
  size_t axes = 1;
  pocketfft::r2c(shape, stride, stride_out, axes, pocketfft::FORWARD,
                      m_in_fft_buf.data(), m_out_fft_buf.data(), 1.0);
#endif

  for (size_t i = 0; i < m_feature_size; ++i) {
#ifdef BUILD_FFTW3_FE
  dft.real[i] = m_out_fft_buf[2 * i];
  dft.imag[i] = m_out_fft_buf[2 * i + 1];
#elif BUILD_POCKETFFT_FE
  dft.real[i] = m_out_fft_buf[i].real();
  dft.imag[i] = m_out_fft_buf[i].imag();
#endif
  }
}

template <typename T>
FftCalculator<T>::~FftCalculator() {
#ifdef BUILD_FFTW3_FE
  fftw_cleanup();
  fftw_destroy_plan(m_fft_plan);
#endif
}

template class FftCalculator<float>;
template class FftCalculator<double>;
}  // namespace fe
}  // namespace spench
