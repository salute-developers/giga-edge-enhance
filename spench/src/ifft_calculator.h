#pragma once

#include <complex>
#include <vector>

#include "spench/src/feature_params.h"

namespace pocketfft {
using shape_t  = std::vector<std::size_t>;
using stride_t = std::vector<std::ptrdiff_t>;
}  // namespace pocketfft

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

  void ValidateIfftArgs(uint32_t n_feats) const;

  pocketfft::shape_t MakeShape(size_t frames) const;

  void MakeStrides(pocketfft::stride_t& stride_in,
                   pocketfft::stride_t& stride_out) const;

  void BuildFullSpectrum(const T* real_ptr, const T* imag_ptr, size_t frames);

  void CopyOutputToVector(std::vector<T>& idft) const;

  void CalculateIFft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);
};

}  // namespace fe
}  // namespace spench
