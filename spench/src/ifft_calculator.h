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

  void calculate_istft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);

 private:
  const uint32_t nfft_;
  const uint32_t feature_size_;
  const uint32_t frames_num_for_istft_;

  std::vector<std::complex<fft::buffer_type>> in_fft_buf_;
  std::vector<fft::buffer_type> out_fft_buf_;

  void validate_ifft_args(uint32_t n_feats) const;

  pocketfft::shape_t MakeShape(size_t frames) const;

  void make_strides(pocketfft::stride_t& stride_in,
                   pocketfft::stride_t& stride_out) const;

  void build_full_spectrum(const T* real_ptr, const T* imag_ptr, size_t frames);

  void copy_output_to_vector(std::vector<T>& idft) const;

  void calculate_ifft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);
};

}  // namespace fe
}  // namespace spench
