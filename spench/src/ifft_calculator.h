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
#include "spench/src/spench_context.h"

namespace spench {
namespace fe {

template <typename T>
class iFftCalculator {
 public:
  explicit iFftCalculator(const uint32_t feature_size,
                          const uint32_t nfft,
                          const uint32_t frames_num_for_istft);
  ~iFftCalculator();

  void CalculateISTFT(const uint32_t n_feats,
                      const T* real_ptr,
                      const T* imag_ptr,
                      std::vector<T>& idft);

 private:
  const uint32_t m_nfft;
  const uint32_t m_feature_size;
  const uint32_t m_frames_num_for_istft;

#ifdef BUILD_FFTW3_FE
  fftw_plan m_ifft_plan;
  std::vector<fft::buffer_type> m_in_fft_buf;
#elif BUILD_POCKETFFT_FE
  std::vector<std::complex<fft::buffer_type>> m_in_fft_buf;
#endif
  std::vector<fft::buffer_type> m_out_fft_buf;

  void CalculateIFftw3(const uint32_t n_feats,
                       const T* real_ptr,
                       const T* imag_ptr,
                       std::vector<T>& idft);
};

}  // namespace fe
}  // namespace spench
