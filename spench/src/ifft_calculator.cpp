#include "spench/src/ifft_calculator.h"

#include <cmath>
#include <algorithm>
#include <iostream>

namespace spench {
namespace fe {
template <typename T>
iFftCalculator<T>::iFftCalculator(const uint32_t feature_size,
                                  const uint32_t nfft,
                                  const uint32_t frames_num_for_istft)
    : m_nfft(nfft)
    , m_feature_size(feature_size)
    , m_frames_num_for_istft(frames_num_for_istft)  // now can be 2 only
#ifdef BUILD_POCKETFFT_FE
    , m_in_fft_buf(m_frames_num_for_istft * m_feature_size, 0.0)
#elif BUILD_FFTW3_FE
    , m_in_fft_buf(2 * m_frames_num_for_istft * m_feature_size, 0.0)  // 2 is real and imag parts
#endif
    , m_out_fft_buf(m_frames_num_for_istft * m_nfft, 0.0) {
#ifdef BUILD_FFTW3_FE
  std::vector<fftw_iodim> dims, howmany_dims;
  dims.push_back(fftw_iodim{static_cast<int>(nfft),
                 static_cast<int>(m_frames_num_for_istft),
                 static_cast<int>(m_frames_num_for_istft)});
  howmany_dims.push_back(fftw_iodim{static_cast<int>(m_frames_num_for_istft), 1, 1});

  m_ifft_plan = fftw_plan_guru_dft_c2r(1, dims.data(), 1, howmany_dims.data(),
                                       reinterpret_cast<fftw_complex *>(m_in_fft_buf.data()),
                                       m_out_fft_buf.data(),
                                       FFTW_MEASURE);
#endif
}

template<typename T>
void iFftCalculator<T>::CalculateISTFT(const uint32_t n_feats,
                                       const T* real_ptr,
                                       const T* imag_ptr,
                                       std::vector<T>& idft) {
  CalculateIFftw3(n_feats, real_ptr, imag_ptr, idft);
}

template<typename T>
void iFftCalculator<T>::CalculateIFftw3(const uint32_t n_feats,
                                        const T* real_ptr,
                                        const T* imag_ptr,
                                        std::vector<T>& idft) {
#ifdef BUILD_FFTW3_FE
  // store data for fftw_plan_guru_dft_c2r in (n_feats, n_frames) format
  // for example in (257, 2)
  uint32_t frame_index;
  for (size_t i = 0; i < n_feats * m_frames_num_for_istft; ++i) {
    frame_index = i % m_frames_num_for_istft;

    m_in_fft_buf[2 * i] = static_cast<fft::buffer_type>(*(real_ptr + n_feats * frame_index));
    m_in_fft_buf[2 * i + 1] = static_cast<fft::buffer_type>(*(imag_ptr + n_feats * frame_index));
    // if we copy all n-th feats from every frame, move to n+1 feat
    if (frame_index == m_frames_num_for_istft - 1) {
      ++real_ptr;
      ++imag_ptr;
    }
  }

  std::fill(m_out_fft_buf.begin(), m_out_fft_buf.end(), 0.0);
  fftw_execute(m_ifft_plan);

  // transpose c2r output
  // for example (512, 2) -> (2, 512)
  // was [[0, 1], ... [510, 511],   [512, 513], ...]
  // now [[0, 2], ... [1020, 1022], [1, 3],     ...]
  uint32_t pos_in_frame;
  for (size_t i = 0; i < m_out_fft_buf.size(); ++i) {
    frame_index = static_cast<uint32_t>(i / m_nfft);
    pos_in_frame = (i % m_nfft) * m_frames_num_for_istft;
    idft[i] = static_cast<T>(m_out_fft_buf[frame_index + pos_in_frame] / m_nfft);
  }

#elif BUILD_POCKETFFT_FE
  // Materials from official pocketfft github and example from
  // https://github.com/project-gemmi/gemmi/blob/master/include/gemmi/fourier.hpp
  // are used.
  pocketfft::shape_t shape{m_frames_num_for_istft, m_nfft};
  std::ptrdiff_t sz = sizeof(fft::buffer_type);
  pocketfft::stride_t stride {static_cast<int32_t>(2 * sz * m_feature_size), 2 * sz};
  pocketfft::stride_t stride_out {static_cast<int32_t>(sz * m_nfft), sz};
  size_t axes = 1;

  for (size_t i = 0; i < n_feats * m_frames_num_for_istft; ++i) {
    m_in_fft_buf[i].real(static_cast<fft::buffer_type>(*(real_ptr + i)));
    m_in_fft_buf[i].imag(static_cast<fft::buffer_type>(*(imag_ptr + i)));
  }

  pocketfft::c2r(shape, stride, stride_out, axes, pocketfft::BACKWARD,
        m_in_fft_buf.data(), m_out_fft_buf.data(), 1.0 / m_nfft);

  for (uint16_t i = 0; i < m_out_fft_buf.size(); ++i) {
    idft[i] = m_out_fft_buf[i];
  }
#endif
}

template <typename T>
iFftCalculator<T>::~iFftCalculator() {
#ifdef BUILD_FFTW3_FE
  fftw_cleanup();
  fftw_destroy_plan(m_ifft_plan);
#endif
}

template class iFftCalculator<float>;
template class iFftCalculator<double>;
}  // namespace fe
}  // namespace spench
