#pragma once

#include <complex>
#include <vector>

#include "spench/src/feature_params.h"

namespace spench {
namespace fe {

template <typename T>
class iFftCalculator {
 public:
  explicit iFftCalculator(const uint32_t feature_size, const uint32_t nfft, const uint32_t frames_num_for_istft);
  ~iFftCalculator();

  void CalculateISTFT(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);

 private:
  const uint32_t m_nfft;
  const uint32_t m_feature_size;
  const uint32_t m_frames_num_for_istft;

  std::vector<std::complex<fft::buffer_type>> m_in_fft_buf;
  std::vector<fft::buffer_type> m_out_fft_buf;

  void CalculateIFft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);
};

}  // namespace fe
}  // namespace spench
