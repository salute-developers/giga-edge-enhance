#pragma once

#ifdef BUILD_FFTW3_FE
#include <fftw3.h>
#elif BUILD_POCKETFFT_FE
#include <complex>
#include "spench/contrib/pocketfft/pocketfft_hdronly.h"
#endif

#include <vector>
#include <mutex>  // NOLINT
#include <tuple>

#include "spench/src/feature_params.h"
#include "spench/src/denoise_context.h"

namespace spench {
namespace fe {

template <typename T>
class FftCalculator {
 public:
  explicit FftCalculator(const uint32_t nfft,
                         const uint32_t feature_size,
                         const uint32_t frames_num_for_stft = 2);
  ~FftCalculator();
  void CalculateSTFT(const size_t& n_samples,
                     const T* begin,
                     ComplexNumber<T>& dft);

 private:
  const uint32_t m_nfft;
  const uint32_t m_feature_size;
  const uint32_t m_frames_num_for_stft;

#ifdef BUILD_FFTW3_FE
  fftw_plan m_fft_plan;
  std::vector<fft::buffer_type> m_out_fft_buf;
#elif BUILD_POCKETFFT_FE
  std::vector<std::complex<fft::buffer_type>> m_out_fft_buf;
#endif
  std::vector<fft::buffer_type> m_in_fft_buf;

  void CalculateFftw3(const size_t& n_samples,
                      const T* begin,
                      ComplexNumber<T>& output);
};
}  // namespace fe
}  // namespace spench
