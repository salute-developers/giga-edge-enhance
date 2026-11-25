#pragma once

#include <complex>
#include <vector>

#include "src/feature_params.h"

namespace pocketfft {
using ShapeT = std::vector<std::size_t>;
using StrideT = std::vector<std::ptrdiff_t>;
}  // namespace pocketfft

namespace giga_edge::enhance {
namespace fe {

template <typename T>
class IFftCalculator {
 public:
  explicit IFftCalculator(const uint32_t feature_size, const uint32_t nfft, const uint32_t frames_num_for_istft);
  ~IFftCalculator();

  void calculate_istft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);

 private:
  const uint32_t nfft;
  const uint32_t feature_size;
  const uint32_t frames_num_for_istft;

  std::vector<std::complex<fft::BufferType>> in_fft_buf_;
  std::vector<fft::BufferType> out_fft_buf_;

  void validate_ifft_args(uint32_t n_feats) const;

  pocketfft::ShapeT make_shape(size_t frames) const;

  void make_strides(pocketfft::StrideT& stride_in, pocketfft::StrideT& stride_out) const;

  void build_full_spectrum(const T* real_ptr, const T* imag_ptr, size_t frames);

  void copy_output_to_vector(std::vector<T>& idft) const;

  void calculate_ifft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr, std::vector<T>& idft);
};

}  // namespace fe
}  // namespace giga_edge::enhance
