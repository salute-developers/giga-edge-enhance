#include "spench/src/feature_extractor.h"

#include <vector>
#include <valarray>
#include <algorithm>
#include <functional>
#include <numeric>
#include <unordered_map>
#include <tuple>
#include <cassert>
#include "spench/src/feature_params.h"
#include "spench/src/spench_context.h"

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
std::vector<T> FeatureExtractor<T>::FrameDirectSignal(const std::vector<T>& input) {
  size_t frame_size = m_feat_params.NumFrameSizeSamples();
  size_t frame_stride = m_feat_params.NumFrameStrideSamples();
  size_t numframes = m_feat_params.NumFrames(input.size());

  std::vector<T> frames(numframes * frame_size);
  for (size_t f = 0; f < numframes; ++f) {
    for (size_t i = 0; i < frame_size; ++i) {
      frames[f * frame_size + i] = input[f * frame_stride + i];
    }
  }
  return frames;
}

template <typename T>
void FeatureExtractor<T>::STFTApply(const std::vector<T>& input,
                                    std::vector<ComplexNumber<T>>& result) {
  std::vector<T> frames = FrameDirectSignal(input);
  if (frames.empty()) {
    return;
  }
  STFTImpl(frames, result);
  return;
}

template <typename T>
void FeatureExtractor<T>::STFTImpl(std::vector<T>& frames,
                                   std::vector<ComplexNumber<T>>& result) {
  size_t n_samples = static_cast<size_t>(m_feat_params.NumFrameSizeSamples());
  size_t n_frames = frames.size() / n_samples;
  size_t k = m_feat_params.PowSpecFeatSz();
  m_windowing.ApplyInPlace(frames);

  for (size_t f = 0; f < n_frames; ++f) {
    ComplexNumber<T> dft(k, k);
    auto begin = frames.data() + f * n_samples;
    {
      std::lock_guard<std::mutex> lock(m_fft_mutex);
      m_fft_calculator.CalculateSTFT(n_samples, begin, dft);
    }
    result.push_back(dft);
  }
  return;
}

template <typename T>
ComplexNumber<T> FeatureExtractor<T>::FrameInverseSignal(
                                                      const ComplexNumber<T>& input_spec) {
  size_t k = m_feat_params.PowSpecFeatSz();
  size_t frame_size = 2 * k;
  size_t num_frames = m_feat_params.NumFrames(input_spec.real.size());

  ComplexNumber<T> result(num_frames * frame_size, num_frames * frame_size);

  for (size_t f = 0; f < num_frames; ++f) {
    for (size_t i = 0; i < frame_size; ++i) {
      result.real[f * frame_size + i] = input_spec.real[f * k + i];
      result.imag[f * frame_size + i] = input_spec.imag[f * k + i];
    }
  }

  return result;
}

template <typename T>
void FeatureExtractor<T>::iSTFTApply(const ComplexNumber<T>& input_spec,
                                     std::vector<T>& output_signal) {
  auto frames = FrameInverseSignal(input_spec);

  iSTFTImpl(frames, output_signal);
  return;
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
void FeatureExtractor<T>::iSTFTImpl(ComplexNumber<T>& input_spec,
                                    std::vector<T>& output_signal) {
  size_t n_feats = m_feat_params.PowSpecFeatSz();  // 257 for example
  size_t hop_size = m_feat_params.NumFrameStrideSamples();  // 256 for example

  size_t iter_size = m_feat_params.frames_num_for_istft * n_feats;
  size_t n_frames = input_spec.real.size() / iter_size;  // can be > 2

  size_t n_samples = static_cast<size_t>(m_feat_params.NumFrameSizeSamples());

  std::vector<T> signal(n_samples * m_feat_params.frames_num_for_istft, 0.0);
  std::vector<T> signal_lookuped(n_samples + hop_size * n_frames);

  for (size_t f = 0; f < n_frames; ++f) {
    auto begin_real = &input_spec.real[f * iter_size];
    auto begin_imag = &input_spec.imag[f * iter_size];
    {
      std::lock_guard<std::mutex> lock(m_ifft_mutex);
      m_ifft_calculator.CalculateISTFT(n_feats, begin_real, begin_imag, signal);
    }
    // apply window (ifft_window, hann or hamm)
    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L418
    m_windowing.ApplyInPlace(signal);

    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L421
    std::fill(signal_lookuped.begin(), signal_lookuped.end(), 0.0);
    lookupAdd(signal, signal_lookuped);

    // apply window_sumsquare
    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L426
    m_windowing.sumSquareInPlace(signal_lookuped);

    output_signal.insert(output_signal.end(),
                         signal_lookuped.begin() + hop_size,
                         signal_lookuped.begin() + n_samples * (m_feat_params.frames_num_for_istft - 1));
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

  // Use only the module of the complex mask.
  // This will help prevent anomalies at the frame boundaries caused by the influence of the phase.
  const auto abs_mask = enhanced_spec.getMagnitude();
  for (uint32_t i = 0; i < abs_mask.size(); ++i) {
    auto mult = std::min(abs_mask[i], 1.0f);
    enhanced_spec.real[i] = input_spec.real[i] * mult;
    enhanced_spec.imag[i] = input_spec.imag[i] * mult;
  }
}

template <typename T>
T FeatureExtractor<T>::calcSnrEstimation(const ComplexNumber<T>& input_spec,
                                         const ComplexNumber<T>& enhanced_spec,
                                         StreamCurrentMetrics<T>& metrics) {
  T E1 = (enhanced_spec.real * enhanced_spec.real + enhanced_spec.imag * enhanced_spec.imag).sum() + this->m_eps;
  T E2 = (input_spec.real * input_spec.real + input_spec.imag * input_spec.imag).sum() + this->m_eps;
  metrics.m_e1_max = std::max(metrics.m_e1_max, E1);
  assert(E1 > 0);
  assert(E2 > 0);
  if (E1 > metrics.m_e1_max * this->vad_threshold) {
      metrics.m_speech_energy_avg = (metrics.m_speech_energy_avg * 0.95) + (E1 * 0.05);
  } else {
      metrics.m_noise_energy_avg = (metrics.m_noise_energy_avg * 0.95) + (E2 * 0.05);
  }
  assert((metrics.m_speech_energy_avg / metrics.m_noise_energy_avg) > 0);
  T SNR_est = 10 * std::log10(metrics.m_speech_energy_avg / metrics.m_noise_energy_avg);
  return SNR_est;
}
template class FeatureExtractor<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
