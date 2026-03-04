#include "spench/src/ifft_calculator.h"

#include <cmath>

#include "contrib/pocketfft/pocketfft_hdronly.h"

namespace spench {
namespace fe {
template <typename T>
iFftCalculator<T>::iFftCalculator(const uint32_t feature_size, const uint32_t nfft, const uint32_t frames_num_for_istft)
    : m_nfft(nfft),
      m_feature_size(feature_size),
      m_frames_num_for_istft(frames_num_for_istft),
      m_in_fft_buf(m_frames_num_for_istft * m_nfft, 0.0),
      m_out_fft_buf(m_frames_num_for_istft * m_nfft, 0.0) {}

template <typename T>
void iFftCalculator<T>::CalculateISTFT(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr,
                                       std::vector<T>& idft) {
  CalculateIFft(n_feats, real_ptr, imag_ptr, idft);
}

template <typename T>
void iFftCalculator<T>::ValidateIfftArgs(uint32_t n_feats) const {
  if (n_feats != m_feature_size) {
    throw std::runtime_error("CalculateIFft: n_feats != feature_size");
  }
  // Expecting half-spectrum: nfft/2 + 1
  const uint32_t expected_half = m_nfft / 2 + 1;
  if (m_feature_size != expected_half) {
    throw std::runtime_error("CalculateIFft: feature_size != nfft/2+1");
  }
}

template <typename T>
pocketfft::shape_t iFftCalculator<T>::MakeShape(size_t frames) const {
  return pocketfft::shape_t{frames, m_nfft};
}

template <typename T>
void iFftCalculator<T>::MakeStrides(pocketfft::stride_t& stride_in,
                                    pocketfft::stride_t& stride_out) const {
  const std::ptrdiff_t sz = sizeof(fft::buffer_type);

  // Input: complex interleaved [frames, nfft]
  stride_in = pocketfft::stride_t{
      static_cast<std::ptrdiff_t>(2 * sz * m_nfft),  // step between frames
      static_cast<std::ptrdiff_t>(2 * sz)            // step between bins
  };

  // Output: real [frames, nfft]
  stride_out = pocketfft::stride_t{
      static_cast<std::ptrdiff_t>(sz * m_nfft),
      static_cast<std::ptrdiff_t>(sz)
  };
}

template <typename T>
void iFftCalculator<T>::BuildFullSpectrum(const T* real_ptr, const T* imag_ptr, size_t frames) {
  // Build full complex spectrum per frame from half-spectrum.
  for (size_t fr = 0; fr < frames; ++fr) {
    std::complex<fft::buffer_type>* X = &m_in_fft_buf[fr * m_nfft];

    // 1) Copy half-spectrum [0..nfft/2]
    for (size_t k = 0; k < m_feature_size; ++k) {
      const size_t idx = fr * m_feature_size + k;
      X[k].real(static_cast<fft::buffer_type>(real_ptr[idx]));
      X[k].imag(static_cast<fft::buffer_type>(imag_ptr[idx]));
    }

    // 2) Enforce DC and Nyquist to be real
    X[0].imag(0);
    if ((m_nfft % 2) == 0) {
      X[m_nfft / 2].imag(0);
    }

    // 3) Mirror the spectrum: X[nfft-k] = conj(X[k]) for k=1..nfft/2-1
    for (size_t k = 1; k + 1 < m_feature_size; ++k) {
      X[m_nfft - k] = std::conj(X[k]);
    }
  }
}

template <typename T>
void iFftCalculator<T>::CopyOutputToVector(std::vector<T>& idft) const {
  idft.resize(m_out_fft_buf.size());
  for (size_t i = 0; i < m_out_fft_buf.size(); ++i) {
    idft[i] = static_cast<T>(m_out_fft_buf[i]);
  }
}

template <typename T>
void iFftCalculator<T>::CalculateIFft(const uint32_t n_feats,
                                      const T* real_ptr,
                                      const T* imag_ptr,
                                      std::vector<T>& idft) {
  ValidateIfftArgs(n_feats);

  const size_t frames = m_frames_num_for_istft;
  const auto shape = MakeShape(frames);

  pocketfft::stride_t stride_in;
  pocketfft::stride_t stride_out;
  MakeStrides(stride_in, stride_out);

  // Prepare full spectra for pocketfft::c2r
  BuildFullSpectrum(real_ptr, imag_ptr, frames);

  constexpr size_t axes = 1;  // transform along last axis (nfft)
  pocketfft::c2r<fft::buffer_type>(
      shape,
      stride_in,
      stride_out,
      axes,
      pocketfft::BACKWARD,
      m_in_fft_buf.data(),
      m_out_fft_buf.data(),
      1.0 / m_nfft);

  CopyOutputToVector(idft);
}

template <typename T>
iFftCalculator<T>::~iFftCalculator() {}

template class iFftCalculator<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
