#include "spench/src/ifft_calculator.h"

#include <cmath>

#include "contrib/pocketfft/pocketfft_hdronly.h"

namespace spench {
namespace fe {
template <typename T>
iFftCalculator<T>::iFftCalculator(const uint32_t feature_size, const uint32_t nfft, const uint32_t frames_num_for_istft)
    : nfft_(nfft),
      feature_size_(feature_size),
      frames_num_for_istft_(frames_num_for_istft),
      in_fft_buf_(frames_num_for_istft_ * nfft_, 0.0),
      out_fft_buf_(frames_num_for_istft_ * nfft_, 0.0) {}

template <typename T>
void iFftCalculator<T>::calculate_istft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr,
                                       std::vector<T>& idft) {
  calculate_ifft(n_feats, real_ptr, imag_ptr, idft);
}

template <typename T>
void iFftCalculator<T>::validate_ifft_args(uint32_t n_feats) const {
  if (n_feats != feature_size_) {
    throw std::runtime_error("calculate_ifft: n_feats != feature_size");
  }
  // Expecting half-spectrum: nfft/2 + 1
  const uint32_t expected_half = nfft_ / 2 + 1;
  if (feature_size_ != expected_half) {
    throw std::runtime_error("calculate_ifft: feature_size != nfft/2+1");
  }
}

template <typename T>
pocketfft::shape_t iFftCalculator<T>::MakeShape(size_t frames) const {
  return pocketfft::shape_t{frames, nfft_};
}

template <typename T>
void iFftCalculator<T>::make_strides(pocketfft::stride_t& stride_in,
                                    pocketfft::stride_t& stride_out) const {
  const std::ptrdiff_t sz = sizeof(fft::buffer_type);

  // Input: complex interleaved [frames, nfft]
  stride_in = pocketfft::stride_t{
      static_cast<std::ptrdiff_t>(2 * sz * nfft_),  // step between frames
      static_cast<std::ptrdiff_t>(2 * sz)            // step between bins
  };

  // Output: real [frames, nfft]
  stride_out = pocketfft::stride_t{
      static_cast<std::ptrdiff_t>(sz * nfft_),
      static_cast<std::ptrdiff_t>(sz)
  };
}

template <typename T>
void iFftCalculator<T>::build_full_spectrum(const T* real_ptr, const T* imag_ptr, size_t frames) {
  // Build full complex spectrum per frame from half-spectrum.
  for (size_t fr = 0; fr < frames; ++fr) {
    std::complex<fft::buffer_type>* X = &in_fft_buf_[fr * nfft_];

    // 1) Copy half-spectrum [0..nfft/2]
    for (size_t k = 0; k < feature_size_; ++k) {
      const size_t idx = fr * feature_size_ + k;
      X[k].real(static_cast<fft::buffer_type>(real_ptr[idx]));
      X[k].imag(static_cast<fft::buffer_type>(imag_ptr[idx]));
    }

    // 2) Enforce DC and Nyquist to be real
    X[0].imag(0);
    if ((nfft_ % 2) == 0) {
      X[nfft_ / 2].imag(0);
    }

    // 3) Mirror the spectrum: X[nfft-k] = conj(X[k]) for k=1..nfft/2-1
    for (size_t k = 1; k + 1 < feature_size_; ++k) {
      X[nfft_ - k] = std::conj(X[k]);
    }
  }
}

template <typename T>
void iFftCalculator<T>::copy_output_to_vector(std::vector<T>& idft) const {
  idft.resize(out_fft_buf_.size());
  for (size_t i = 0; i < out_fft_buf_.size(); ++i) {
    idft[i] = static_cast<T>(out_fft_buf_[i]);
  }
}

template <typename T>
void iFftCalculator<T>::calculate_ifft(const uint32_t n_feats,
                                      const T* real_ptr,
                                      const T* imag_ptr,
                                      std::vector<T>& idft) {
  validate_ifft_args(n_feats);

  const size_t frames = frames_num_for_istft_;
  const auto shape = MakeShape(frames);

  pocketfft::stride_t stride_in;
  pocketfft::stride_t stride_out;
  make_strides(stride_in, stride_out);

  // Prepare full spectra for pocketfft::c2r
  build_full_spectrum(real_ptr, imag_ptr, frames);

  constexpr size_t axes = 1;  // transform along last axis (nfft)
  pocketfft::c2r<fft::buffer_type>(
      shape,
      stride_in,
      stride_out,
      axes,
      pocketfft::BACKWARD,
      in_fft_buf_.data(),
      out_fft_buf_.data(),
      1.0 / nfft_);

  copy_output_to_vector(idft);
}

template <typename T>
iFftCalculator<T>::~iFftCalculator() {}

template class iFftCalculator<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
