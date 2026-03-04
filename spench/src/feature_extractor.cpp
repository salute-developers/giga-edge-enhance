#include "spench/src/feature_extractor.h"

#include <vector>
#include <valarray>
#include <algorithm>
#include <functional>
#include <numeric>
#include <tuple>
#include <cassert>
#include <utility>
#include "spench/src/feature_params.h"

namespace spench {
namespace fe {

template <typename T>
FeatureExtractor<T>::FeatureExtractor(const FeatureParams& params)
    : m_feat_params(params)
    , m_windowing(params.NumFrameSizeSamples(), params.window_type, params.NumFrameStrideSamples(), true)
    , m_fft_calculator(params.Nfft(), params.PowSpecFeatSz(), params.frames_num_for_istft)
    , m_ifft_calculator(params.PowSpecFeatSz(), params.Nfft(), params.frames_num_for_istft) {
  ValidatePowSpecParams();
}

template <typename T>
void FeatureExtractor<T>::STFTApply(const std::vector<T>& input,
                                    std::vector<ComplexNumber<T>>& result) {
  if (input.empty()) {
    return;
  }
  STFTImpl(input, result);
}

template <typename T>
void FeatureExtractor<T>::STFTImpl(const std::vector<T>& input,
                                   std::vector<ComplexNumber<T>>& result) {
  const size_t frame_size   = static_cast<size_t>(m_feat_params.NumFrameSizeSamples());
  const size_t frame_stride = static_cast<size_t>(m_feat_params.NumFrameStrideSamples());
  const size_t numframes    = static_cast<size_t>(m_feat_params.NumFrames(input.size()));
  const size_t k            = static_cast<size_t>(m_feat_params.PowSpecFeatSz());

  if (numframes == 0) {
    return;
  }

  result.reserve(result.size() + numframes);

  std::vector<T> frame_buf(frame_size);

  for (size_t f = 0; f < numframes; ++f) {
    const size_t start = f * frame_stride;
    // Copy raw frame (same indexing as old FrameDirectSignal()).
    std::copy_n(input.data() + start, frame_size, frame_buf.data());
    // Window this frame in place.
    m_windowing.ApplyFrameInPlace(frame_buf.data(), frame_size);
    ComplexNumber<T> dft(k, k);
    {
      std::lock_guard<std::mutex> lock(m_fft_mutex);
      m_fft_calculator.CalculateSTFT(frame_size, frame_buf.data(), dft);
    }
    result.push_back(std::move(dft));
  }
}

template <typename T>
void FeatureExtractor<T>::iSTFTApply(const ComplexNumber<T>& input_spec,
                                     std::vector<T>& output_signal) {
  if (input_spec.empty()) {
    return;
  }
  iSTFTImpl(input_spec, output_signal);
}

template <typename T>
void FeatureExtractor<T>::lookupAdd(const std::vector<T>& signal,
                                    std::vector<T>& output_signal) {
  uint32_t n_samples = m_feat_params.NumFrameSizeSamples();
  uint32_t hop_length = m_feat_params.NumFrameStrideSamples();
  uint32_t start_from, start_to;

  for (uint32_t frame_num = 0; frame_num < signal.size() / n_samples; ++frame_num) {
    start_from = frame_num * n_samples;
    start_to = frame_num * hop_length;
    for (uint32_t i = 0; i < n_samples; ++i) {
      output_signal[start_to + i] += signal[start_from + i];
    }
  }
}

template <typename T>
void FeatureExtractor<T>::iSTFTImpl(const ComplexNumber<T>& input_spec,
                                    std::vector<T>& output_signal) {
  const size_t n_feats   = m_feat_params.PowSpecFeatSz();
  const size_t hop_size  = m_feat_params.NumFrameStrideSamples();
  const size_t n_samples = m_feat_params.NumFrameSizeSamples();
  const size_t win_frames = m_feat_params.frames_num_for_istft;  // == 2
  size_t frames_count = input_spec.real.size() / n_feats;

  if (win_frames != 2) {
    throw std::runtime_error("iSTFTApply: expected frames_num_for_istft == 2");
  }
  if (frames_count < 2) {
    return;
  }
  if (input_spec.real.size() != input_spec.imag.size()) {
    throw std::runtime_error("iSTFTApply: real/imag size mismatch");
  }

  if (n_feats == 0 || (input_spec.real.size() % n_feats) != 0) {
    throw std::runtime_error("iSTFTImpl: input_spec size is not multiple of n_feats");
  }

  const size_t frames_in_buffer = input_spec.real.size() / n_feats;

  size_t frames_to_use = (frames_count == 0)
      ? frames_in_buffer
      : std::min<size_t>(frames_count, frames_in_buffer);

  if (frames_to_use < 2) {
    output_signal.clear();
    return;
  }

  const size_t out_hops = frames_to_use - 1;
  const size_t out_len  = out_hops * hop_size;
  output_signal.assign(out_len, T(0));

  std::vector<T> signal(n_samples * win_frames, T(0));
  std::vector<T> lookuped(n_samples + hop_size * (win_frames - 1), T(0));

  size_t out_pos = 0;

  for (size_t f = 0; f + 1 < frames_count; ++f) {
    const T* begin_real = &input_spec.real[f * n_feats];
    const T* begin_imag = &input_spec.imag[f * n_feats];

    {
      std::lock_guard<std::mutex> lock(m_ifft_mutex);
      m_ifft_calculator.CalculateISTFT(n_feats, begin_real, begin_imag, signal);
    }

    // apply window (ifft_window, hann or hamm)
    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L418
    m_windowing.ApplyInPlace(signal);

    std::fill(lookuped.begin(), lookuped.end(), T(0));
    lookupAdd(signal, lookuped);
    // apply window_sumsquare
    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L426
    m_windowing.sumSquareInPlace(lookuped);

    const auto src_begin = lookuped.begin() + hop_size;
    const auto src_end   = src_begin + hop_size;

    std::copy(src_begin, src_end, output_signal.begin() + out_pos);
    out_pos += hop_size;
  }

  // sanity
  if (out_pos != out_len) {
    throw std::runtime_error("iSTFTApply: internal length mismatch");
  }
}

template <typename T>
FeatureParams FeatureExtractor<T>::GetFeatureParams() const {
  return m_feat_params;
}

template <typename T>
void FeatureExtractor<T>::ValidatePowSpecParams() const {
  if (m_feat_params.sampling_freq <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: sampling_freq is negative");
  } else if (m_feat_params.frame_size_ms <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_size_ms is negative");
  } else if (m_feat_params.frame_stride_ms <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_stride_ms is negative");
  } else if (m_feat_params.NumFrameSizeSamples() <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_size_ms is too low");
  } else if (m_feat_params.NumFrameStrideSamples() <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_stride_ms is too low");
  }
}

template <typename T>
std::valarray<T> FeatureExtractor<T>::boolToT(const std::valarray<bool> b) {
  std::valarray<T> result(b.size());
  for (uint32_t i = 0; i < b.size(); ++i) {
    result[i] = b[i] ? static_cast<T>(1) : static_cast<T>(0);
  }
  return result;
}

template <typename T>
void FeatureExtractor<T>::apply_decompressCIRMInPlace(std::valarray<T>& mask_part) {
  // limit * (mask >= limit) - limit * (mask <= -limit) + mask * (np.abs(mask) < limit)
  mask_part = m_cirmLimit * boolToT(mask_part >= m_cirmLimit) -
              m_cirmLimit * boolToT(mask_part <= -m_cirmLimit) +
              mask_part * boolToT(std::abs(mask_part) < m_cirmLimit);

  // -K * np.log((K - mask) / (K + mask))
  mask_part = -m_cirmK * std::log((m_cirmK - mask_part) / (m_cirmK + mask_part));
}

template <typename T>
void FeatureExtractor<T>::decompressCIRMInPlace(ComplexNumber<T>& predicted_mask) {
  // mask for imag or real is empty
  if (predicted_mask.real.size() == 0 || predicted_mask.imag.size() == 0) {
    return;
  }

  // masks for real and imag parts are not the same
  if (predicted_mask.real.size() != predicted_mask.imag.size()) {
    return;
  }

  apply_decompressCIRMInPlace(predicted_mask.real);
  apply_decompressCIRMInPlace(predicted_mask.imag);
}

template <typename T>
void FeatureExtractor<T>::calcEnhancedPartsInPlace(const ComplexNumber<T>& input_spec,
                                                   ComplexNumber<T>& enhanced_spec) {
  if (input_spec.real.size() == 0 ||
      input_spec.imag.size() == 0 ||
      enhanced_spec.real.size() == 0 ||
      enhanced_spec.imag.size() == 0) {
    return;
  }

  if (input_spec.real.size() != input_spec.imag.size()) {
    return;
  }

  decompressCIRMInPlace(enhanced_spec);

  if (enhanced_spec.size() != input_spec.size()) {
    throw std::runtime_error("calcEnhancedPartsInPlace: enhanced_spec size != input_spec size");
  }
  // Use only the module of the complex mask.
  // This will help prevent anomalies at the frame boundaries caused by the influence of the phase.
  const auto abs_mask = enhanced_spec.getMagnitude();
  for (uint32_t i = 0; i < abs_mask.size(); ++i) {
    auto mult = std::min(abs_mask[i], 1.0f);
    enhanced_spec.real[i] = input_spec.real[i] * mult;
    enhanced_spec.imag[i] = input_spec.imag[i] * mult;
  }
}

template class FeatureExtractor<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
