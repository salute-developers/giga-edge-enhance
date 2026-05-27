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
    : feat_params_(params)
    , windowing_(params.num_frame_size_samples(), params.window_type, params.num_frame_stride_samples(), true)
    , fft_calculator_(params.nfft(), params.pow_spec_feat_sz(), params.frames_num_for_istft)
    , ifft_calculator_(params.pow_spec_feat_sz(), params.nfft(), params.frames_num_for_istft) {
  validate_pow_spec_params();
}

template <typename T>
void FeatureExtractor<T>::stft_apply(const std::vector<T>& input,
                                    std::vector<ComplexNumber<T>>& result) {
  if (input.empty()) {
    return;
  }
  stft_impl(input, result);
}

template <typename T>
void FeatureExtractor<T>::stft_impl(const std::vector<T>& input,
                                   std::vector<ComplexNumber<T>>& result) {
  const size_t frame_size   = static_cast<size_t>(feat_params_.num_frame_size_samples());
  const size_t frame_stride = static_cast<size_t>(feat_params_.num_frame_stride_samples());
  const size_t numframes    = static_cast<size_t>(feat_params_.num_frames(input.size()));
  const size_t k            = static_cast<size_t>(feat_params_.pow_spec_feat_sz());

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
    windowing_.apply_frame_in_place(frame_buf.data(), frame_size);
    ComplexNumber<T> dft(k, k);
    {
      std::lock_guard<std::mutex> lock(fft_mutex_);
      fft_calculator_.calculate_stft(frame_size, frame_buf.data(), dft);
    }
    result.push_back(std::move(dft));
  }
}

template <typename T>
void FeatureExtractor<T>::istft_apply(const ComplexNumber<T>& input_spec,
                                     std::vector<T>& output_signal) {
  if (input_spec.empty()) {
    return;
  }
  istft_impl(input_spec, output_signal);
}

template <typename T>
void FeatureExtractor<T>::lookup_add(const std::vector<T>& signal,
                                    std::vector<T>& output_signal) {
  uint32_t n_samples = feat_params_.num_frame_size_samples();
  uint32_t hop_length = feat_params_.num_frame_stride_samples();
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
void FeatureExtractor<T>::istft_impl(const ComplexNumber<T>& input_spec,
                                    std::vector<T>& output_signal) {
  const size_t n_feats   = feat_params_.pow_spec_feat_sz();
  const size_t hop_size  = feat_params_.num_frame_stride_samples();
  const size_t n_samples = feat_params_.num_frame_size_samples();
  const size_t win_frames = feat_params_.frames_num_for_istft;  // == 2
  size_t frames_count = input_spec.real.size() / n_feats;

  if (win_frames != 2) {
    throw std::runtime_error("istft_apply: expected frames_num_for_istft == 2");
  }
  if (frames_count < 2) {
    return;
  }
  if (input_spec.real.size() != input_spec.imag.size()) {
    throw std::runtime_error("istft_apply: real/imag size mismatch");
  }

  if (n_feats == 0 || (input_spec.real.size() % n_feats) != 0) {
    throw std::runtime_error("istft_impl: input_spec size is not multiple of n_feats");
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
      std::lock_guard<std::mutex> lock(ifft_mutex_);
      ifft_calculator_.calculate_istft(n_feats, begin_real, begin_imag, signal);
    }

    // apply window (ifft_window, hann or hamm)
    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L418
    windowing_.apply_in_place(signal);

    std::fill(lookuped.begin(), lookuped.end(), T(0));
    lookup_add(signal, lookuped);
    // apply window_sumsquare
    // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/core/spectrum.py#L426
    windowing_.sum_square_in_place(lookuped);

    const auto src_begin = lookuped.begin() + hop_size;
    const auto src_end   = src_begin + hop_size;

    std::copy(src_begin, src_end, output_signal.begin() + out_pos);
    out_pos += hop_size;
  }

  // sanity
  if (out_pos != out_len) {
    throw std::runtime_error("istft_apply: internal length mismatch");
  }
}

template <typename T>
FeatureParams FeatureExtractor<T>::feature_params() const {
  return feat_params_;
}

template <typename T>
void FeatureExtractor<T>::validate_pow_spec_params() const {
  if (feat_params_.sampling_freq <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: sampling_freq is negative");
  } else if (feat_params_.frame_size_ms <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_size_ms is negative");
  } else if (feat_params_.frame_stride_ms <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_stride_ms is negative");
  } else if (feat_params_.num_frame_size_samples() <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_size_ms is too low");
  } else if (feat_params_.num_frame_stride_samples() <= 0) {
    throw std::invalid_argument("PowerSpectrumBase: frame_stride_ms is too low");
  }
}

template <typename T>
std::valarray<T> FeatureExtractor<T>::bool_to_t(const std::valarray<bool> b) {
  std::valarray<T> result(b.size());
  for (uint32_t i = 0; i < b.size(); ++i) {
    result[i] = b[i] ? static_cast<T>(1) : static_cast<T>(0);
  }
  return result;
}

template <typename T>
void FeatureExtractor<T>::apply_decompress_cirm_in_place(std::valarray<T>& mask_part) {
  // limit * (mask >= limit) - limit * (mask <= -limit) + mask * (np.abs(mask) < limit)
  mask_part = cirm_limit_ * bool_to_t(mask_part >= cirm_limit_) -
              cirm_limit_ * bool_to_t(mask_part <= -cirm_limit_) +
              mask_part * bool_to_t(std::abs(mask_part) < cirm_limit_);

  // -K * np.log((K - mask) / (K + mask))
  mask_part = -cirm_k_ * std::log((cirm_k_ - mask_part) / (cirm_k_ + mask_part));
}

template <typename T>
void FeatureExtractor<T>::decompress_cirm_in_place(ComplexNumber<T>& predicted_mask) {
  // mask for imag or real is empty
  if (predicted_mask.real.size() == 0 || predicted_mask.imag.size() == 0) {
    return;
  }

  // masks for real and imag parts are not the same
  if (predicted_mask.real.size() != predicted_mask.imag.size()) {
    return;
  }

  apply_decompress_cirm_in_place(predicted_mask.real);
  apply_decompress_cirm_in_place(predicted_mask.imag);
}

template <typename T>
void FeatureExtractor<T>::calc_enhanced_parts_in_place(const ComplexNumber<T>& input_spec,
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

  decompress_cirm_in_place(enhanced_spec);

  if (enhanced_spec.size() != input_spec.size()) {
    throw std::runtime_error("calc_enhanced_parts_in_place: enhanced_spec size != input_spec size");
  }
  // Use only the module of the complex mask.
  // This will help prevent anomalies at the frame boundaries caused by the influence of the phase.
  const auto abs_mask = enhanced_spec.get_magnitude();
  for (uint32_t i = 0; i < abs_mask.size(); ++i) {
    auto mult = std::min(abs_mask[i], 1.0f);
    enhanced_spec.real[i] = input_spec.real[i] * mult;
    enhanced_spec.imag[i] = input_spec.imag[i] * mult;
  }
}

template class FeatureExtractor<fft::BufferType>;
}  // namespace fe
}  // namespace spench
