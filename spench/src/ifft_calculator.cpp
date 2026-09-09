#include "spench/src/ifft_calculator.h"

#include <pocketfft_hdronly.h>

#include <cmath>

namespace spench {
namespace fe {
template <typename T>
IFftCalculator<T>::IFftCalculator(const uint32_t feature_size, const uint32_t nfft,
                                  const uint32_t frames_num_for_istft)
    : nfft(nfft),
      feature_size(feature_size),
      frames_num_for_istft(frames_num_for_istft),
      in_fft_buf_(frames_num_for_istft * nfft, 0.0),
      out_fft_buf_(frames_num_for_istft * nfft, 0.0) {}

template <typename T>
void IFftCalculator<T>::calculate_istft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr,
                                       std::vector<T>& idft) {
  calculate_ifft(n_feats, real_ptr, imag_ptr, idft);
}

template <typename T>
void IFftCalculator<T>::validate_ifft_args(uint32_t n_feats) const {
  if (n_feats != feature_size) {
    throw std::runtime_error("calculate_ifft: n_feats != feature_size");
  }
  // Expecting half-spectrum: nfft/2 + 1
  const uint32_t expected_half = nfft / 2 + 1;
  if (feature_size != expected_half) {
    throw std::runtime_error("calculate_ifft: feature_size != nfft/2+1");
  }
}

template <typename T>
pocketfft::ShapeT IFftCalculator<T>::make_shape(size_t frames) const {
  return pocketfft::ShapeT{frames, nfft};
}

template <typename T>
void IFftCalculator<T>::make_strides(pocketfft::StrideT& stride_in,
                                    pocketfft::StrideT& stride_out) const {
  const std::ptrdiff_t sz = sizeof(fft::BufferType);

  // Input: complex interleaved [frames, nfft]
  stride_in = pocketfft::StrideT{
      static_cast<std::ptrdiff_t>(2 * sz * nfft),  // step between frames
      static_cast<std::ptrdiff_t>(2 * sz)          // step between bins
  };

  // Output: real [frames, nfft]
  stride_out = pocketfft::StrideT{
      static_cast<std::ptrdiff_t>(sz * nfft),
      static_cast<std::ptrdiff_t>(sz)
  };
}

template <typename T>
void IFftCalculator<T>::build_full_spectrum(const T* real_ptr, const T* imag_ptr, size_t frames) {
  // Build full complex spectrum per frame from half-spectrum.
  for (size_t fr = 0; fr < frames; ++fr) {
    std::complex<fft::BufferType>* spectrum = &in_fft_buf_[fr * nfft];

    // 1) Copy half-spectrum [0..nfft/2]
    for (size_t k = 0; k < feature_size; ++k) {
      const size_t idx = fr * feature_size + k;
      spectrum[k].real(static_cast<fft::BufferType>(real_ptr[idx]));
      spectrum[k].imag(static_cast<fft::BufferType>(imag_ptr[idx]));
    }

    // 2) Enforce DC and Nyquist to be real
    spectrum[0].imag(0);
    if ((nfft % 2) == 0) {
      spectrum[nfft / 2].imag(0);
    }

    // 3) Mirror the spectrum: spectrum[nfft-k] = conj(spectrum[k]) for k=1..nfft/2-1
    for (size_t k = 1; k + 1 < feature_size; ++k) {
      spectrum[nfft - k] = std::conj(spectrum[k]);
    }
  }
}

template <typename T>
void IFftCalculator<T>::copy_output_to_vector(std::vector<T>& idft) const {
  idft.resize(out_fft_buf_.size());
  for (size_t i = 0; i < out_fft_buf_.size(); ++i) {
    idft[i] = static_cast<T>(out_fft_buf_[i]);
  }
}

template <typename T>
void IFftCalculator<T>::calculate_ifft(const uint32_t n_feats,
                                      const T* real_ptr,
                                      const T* imag_ptr,
                                      std::vector<T>& idft) {
  validate_ifft_args(n_feats);

  const size_t frames = frames_num_for_istft;
  const auto shape = make_shape(frames);

  pocketfft::StrideT stride_in;
  pocketfft::StrideT stride_out;
  make_strides(stride_in, stride_out);

  // Prepare full spectra for pocketfft::c2r
  build_full_spectrum(real_ptr, imag_ptr, frames);

  constexpr size_t axes = 1;  // transform along last axis (nfft)
  pocketfft::c2r<fft::BufferType>(
      shape,
      stride_in,
      stride_out,
      axes,
      pocketfft::BACKWARD,
      in_fft_buf_.data(),
      out_fft_buf_.data(),
      1.0 / nfft);

  copy_output_to_vector(idft);
}

template <typename T>
IFftCalculator<T>::~IFftCalculator() {}

template class IFftCalculator<fft::BufferType>;
}  // namespace fe
}  // namespace spench
