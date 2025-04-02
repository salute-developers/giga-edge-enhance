#include "spench/src/ifft_calculator.h"

#include <cmath>

#include "contrib/pocketfft/pocketfft_hdronly.h"

namespace spench {
namespace fe {
template <typename T>
iFftCalculator<T>::iFftCalculator(const uint32_t feature_size, const uint32_t nfft, const uint32_t frames_num_for_istft)
    : m_nfft(nfft),
      m_feature_size(feature_size),
      m_frames_num_for_istft(frames_num_for_istft)  // now can be 2 only
      ,
      m_in_fft_buf(m_frames_num_for_istft * m_feature_size, 0.0),
      m_out_fft_buf(m_frames_num_for_istft * m_nfft, 0.0) {}

template <typename T>
void iFftCalculator<T>::CalculateISTFT(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr,
                                       std::vector<T>& idft) {
  CalculateIFft(n_feats, real_ptr, imag_ptr, idft);
}

template <typename T>
void iFftCalculator<T>::CalculateIFft(const uint32_t n_feats, const T* real_ptr, const T* imag_ptr,
                                      std::vector<T>& idft) {
  // Materials from official pocketfft github and example from
  // https://github.com/project-gemmi/gemmi/blob/master/include/gemmi/fourier.hpp
  // are used.
  pocketfft::shape_t shape{m_frames_num_for_istft, m_nfft};
  std::ptrdiff_t sz = sizeof(fft::buffer_type);
  pocketfft::stride_t stride{static_cast<int32_t>(2 * sz * m_feature_size), 2 * sz};
  pocketfft::stride_t stride_out{static_cast<int32_t>(sz * m_nfft), sz};
  size_t axes = 1;

  for (size_t i = 0; i < n_feats * m_frames_num_for_istft; ++i) {
    m_in_fft_buf[i].real(static_cast<fft::buffer_type>(*(real_ptr + i)));
    m_in_fft_buf[i].imag(static_cast<fft::buffer_type>(*(imag_ptr + i)));
  }

  pocketfft::c2r<fft::buffer_type>(shape, stride,
                                   stride_out, axes,
                                   pocketfft::BACKWARD,
                                   m_in_fft_buf.data(),
                                   m_out_fft_buf.data(),
                                   1.0 / m_nfft);

  for (uint16_t i = 0; i < m_out_fft_buf.size(); ++i) {
    idft[i] = m_out_fft_buf[i];
  }
}

template <typename T>
iFftCalculator<T>::~iFftCalculator() {}

template class iFftCalculator<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
