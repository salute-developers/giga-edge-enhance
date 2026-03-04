#pragma once

#include <complex>
#include <vector>

#include "spench/src/spench_context.h"
#include "spench/src/feature_params.h"

namespace pocketfft {
using shape_t  = std::vector<std::size_t>;
using stride_t = std::vector<std::ptrdiff_t>;
}  // namespace pocketfft

namespace spench {
namespace fe {

template <typename T>
class FftCalculator {
 public:
  explicit FftCalculator(const uint32_t nfft, const uint32_t feature_size, const uint32_t frames_num_for_stft = 2);
  ~FftCalculator();
  void CalculateSTFT(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft);

 private:
  const uint32_t m_nfft;
  const uint32_t m_feature_size;
  const uint32_t m_frames_num_for_stft;

  std::vector<std::complex<fft::buffer_type>> m_out_fft_buf;
  std::vector<fft::buffer_type> m_in_fft_buf;

  void CalculateFft(const size_t& n_samples, const T* begin, ComplexNumber<T>& dft);
  void PrepareInputBuffer(size_t n_samples, const T* begin);
  void RunPocketFft();
  void CopyOutputTo(ComplexNumber<T>& dft) const;
};
}  // namespace fe
}  // namespace spench
