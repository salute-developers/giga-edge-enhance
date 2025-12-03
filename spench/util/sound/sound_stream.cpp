#include <spench/util/sound/sound_stream.h>
#include <spench/util/sound/sound_types.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <iterator>

#ifndef BUILD_WITHOUT_SOXR
#include <spench/submodules/soxr/src/soxr.h>
#endif
// #include <iostream>
namespace util {
namespace sound {

template <typename TIN, typename TOUT>
ISoundStream<TIN, TOUT>::~ISoundStream() = default;

template <typename TIN, typename TOUT>
SoundStream<TIN, TOUT>::~SoundStream() = default;

template <typename TIN, typename TOUT>
SoundStream<TIN, TOUT>::SoundStream(const SoundInfo& src, const SoundInfo& dst)
    : ISoundStream<TIN, TOUT>(src, dst)
#ifndef BUILD_WITHOUT_SOXR
    , m_resampler {}
    , m_nominal_ratio(dst.sample_rate == 0 || src.sample_rate == 0 ? 1.0
              : static_cast<double>(dst.sample_rate) / static_cast<double>(src.sample_rate))
#endif
            {
  m_in_resample = false;
  // Allow only mono↔mono or mono↔stereo (one side must be mono)
  if (src.channels != dst.channels && !(src.channels == 1 || dst.channels == 1)) {
    throw std::runtime_error("SoundStream::SoundStream: channel conversion from " +
                             std::to_string(src.channels) + " to " + std::to_string(dst.channels) +
                             " is not supported!");
  }
  if (src.sample_rate != dst.sample_rate) {
#ifndef BUILD_WITHOUT_SOXR
    if (std::is_same<TIN, float>::value ||
        std::is_same<TIN, int16_t>::value ||
        std::is_same<TIN, int32_t>::value) {
      m_in_resample = true;
      m_resampler = BuildResampler(src.sample_rate, dst.sample_rate, src.channels);
    } else if (std::is_same<TOUT, float>::value ||
               std::is_same<TOUT, int16_t>::value ||
               std::is_same<TOUT, int32_t>::value) {
      m_resampler = BuildResampler(src.sample_rate, dst.sample_rate, src.channels);
    } else {
      throw std::runtime_error("SoundStream::SoundStream: when src.sample_rate != dst.sample_rate either "
                               "TIN or TOUT has to be float, int16_t or int32_t");
    }
#else
    throw std::runtime_error("SoundStream::SoundStream: Library built without resampling ability. "
                             "Please use a sample rate that is supported by model!");
#endif
  }
  // Enable drift-control scheme only when resampling and 44.1 kHz is involved.
  m_enable_drift_control = (src.sample_rate != dst.sample_rate) &&
                           (src.sample_rate == 44100 || dst.sample_rate == 44100);
  // Allow varying ratio only within the scheme; it may still be disabled later if soxr rejects io_ratio.
  m_allow_varying_ratio  = m_enable_drift_control;
}

template <typename TIN, typename TOUT>
std::vector<TOUT> SoundStream<TIN, TOUT>::ReadChunkToVector(const std::vector<TIN>& in_chunk) {
  if constexpr (!std::is_same<TIN, double>::value && !std::is_same<TIN, float>::value &&
                !std::is_same<TIN, int16_t>::value && !std::is_same<TIN, int>::value) {
    throw std::runtime_error("SoundStream::ReadChunk: conversion is not supported :(");
  }
  if constexpr (!std::is_same<TOUT, double>::value && !std::is_same<TOUT, float>::value &&
                !std::is_same<TOUT, int16_t>::value && !std::is_same<TOUT, int>::value) {
    throw std::runtime_error("SoundStream::ReadChunk: conversion is not supported :(");
  }
  std::vector<TOUT> result;

  if (!in_chunk.size()) {
    return {};
  }

  // Unified channel rule: allow only if one side is mono.
  if (ISoundStream<TIN, TOUT>::m_in.channels != ISoundStream<TIN, TOUT>::m_out.channels &&
      !(ISoundStream<TIN, TOUT>::m_in.channels == 1 || ISoundStream<TIN, TOUT>::m_out.channels == 1)) {
    throw std::runtime_error("SoundStream::ReadChunk: channel conversion is not supported for non-mono cases");
  }

#ifndef BUILD_WITHOUT_SOXR
  if (ISoundStream<TIN, TOUT>::m_in.sample_rate != ISoundStream<TIN, TOUT>::m_out.sample_rate) {
    // Apply drift control only when enabled (44.1 kHz involved).
    if (m_enable_drift_control) {
      ApplyDriftControl();
    }

    result = ResampleVec<TIN>(m_resampler,
                              in_chunk,
                              ISoundStream<TIN, TOUT>::m_in.sample_rate,
                              ISoundStream<TIN, TOUT>::m_out.sample_rate);
  }
#endif
  return result;
}

// --- Drift control implementation ---
template <typename TIN, typename TOUT>
void SoundStream<TIN, TOUT>::ApplyDriftControl() {
#ifndef BUILD_WITHOUT_SOXR
  // Soft drift control window and limits (only when 44.1 kHz is involved).
  const double delay_ms = GetAccumulatedDelayMs();
  double rel = 0.0;
  if (delay_ms > m_drift_hi_ms)       rel = -m_drift_max_rel;
  else if (delay_ms < m_drift_lo_ms)  rel = +m_drift_max_rel;

  bool ratio_set = false;
  if (m_allow_varying_ratio && m_resampler) {
    const double io_ratio = m_nominal_ratio * (1.0 + rel);
    ratio_set = SetResamplerIoRatio(m_resampler, io_ratio);

    if (!ratio_set) {
      m_allow_varying_ratio = false;  // stop trying; fall back from now on
    }
  }
  if (!m_allow_varying_ratio) {
    ApplyFallbackDriftCorrection(delay_ms);
  }
#endif
}

// --- Fallback drift correction: rare single-sample insert/remove with interpolation.
template <typename TIN, typename TOUT>
void SoundStream<TIN, TOUT>::ApplyFallbackDriftCorrection(double delay_ms) {
  const double emitted_ms = (ISoundStream<TIN, TOUT>::m_out.sample_rate == 0) ? 0.0 :
    (static_cast<double>(m_emitted_samples_total) * 1000.0) /
    static_cast<double>(ISoundStream<TIN, TOUT>::m_out.sample_rate);

  if (m_last_adjust_emitted_ms < 0.0 ||
      emitted_ms - m_last_adjust_emitted_ms >= m_drift_min_adjust_interval_ms) {
    if (delay_ms > m_drift_hi_ms && m_output_buffer.size() > 2) {
      // remove one sample near the front; smooth boundary
      TOUT a = m_output_buffer[0];
      TOUT b = m_output_buffer[1];
      m_output_buffer[0] = static_cast<TOUT>((static_cast<double>(a) + static_cast<double>(b)) * 0.5);
      m_output_buffer.erase(std::begin(m_output_buffer) + 1);
      m_last_adjust_emitted_ms = emitted_ms;
    } else if (delay_ms < m_drift_lo_ms && m_output_buffer.size() >= 2) {
      // insert one interpolated sample near the back
      TOUT p = *(std::end(m_output_buffer) - 2);
      TOUT q = *(std::end(m_output_buffer) - 1);
      TOUT mid = static_cast<TOUT>((static_cast<double>(p) + static_cast<double>(q)) * 0.5);
      m_output_buffer.insert(std::end(m_output_buffer) - 1, mid);
      m_last_adjust_emitted_ms = emitted_ms;
    }
  }
}

template <typename TIN, typename TOUT>
std::vector<TOUT> SoundStream<TIN, TOUT>::ReadChunk(const std::vector<TIN>& in_chunk) {
  if (in_chunk.empty() ||
      ISoundStream<TIN, TOUT>::m_in.sample_rate == ISoundStream<TIN, TOUT>::m_out.sample_rate) {
    return in_chunk;
  }

  const uint32_t threshold = ISoundStream<TIN, TOUT>::m_out.FrameSizeSamples();
  auto resampled_vector = ReadChunkToVector(in_chunk);
  std::move(resampled_vector.begin(),
            resampled_vector.end(),
            std::back_inserter(m_output_buffer));

  std::vector<TOUT> result;
  if (m_output_buffer.size() >= threshold) {
    std::move(std::begin(m_output_buffer),
              std::begin(m_output_buffer) + threshold,
              std::back_inserter(result));
    m_output_buffer.erase(std::begin(m_output_buffer),
                          std::begin(m_output_buffer) + threshold);

    // account emitted samples
    m_emitted_samples_total += result.size();
  }

  return result;
}

template <typename TIN, typename TOUT>
std::vector<TOUT> SoundStream<TIN, TOUT>::FinalizeStream() {
  std::vector<TOUT> result;
  std::move(std::begin(m_output_buffer),
              std::end(m_output_buffer),
              std::back_inserter(result));
#ifndef BUILD_WITHOUT_SOXR
  // Always flush if rates differ, regardless of m_in_resample flag.
  if (ISoundStream<TIN, TOUT>::m_in.sample_rate != ISoundStream<TIN, TOUT>::m_out.sample_rate) {
    std::vector<TOUT> flushed_out = FlushVec<TOUT>(m_resampler);
    std::move(std::begin(flushed_out),
              std::end(flushed_out),
              std::back_inserter(result));
  }
#endif
  m_output_buffer.clear();
  m_emitted_samples_total += result.size();
  return result;
}

template <typename T>
std::unique_ptr<ISoundStream<T, T>> BuildSoundStream(uint32_t src_rate, uint32_t dst_rate,
                                                     uint32_t frame_size, TemplateType template_type) {
  SoundInfo src;
  SoundInfo dst;

  src.processing_frame_ms = frame_size;
  src.sample_rate = src_rate;
  src.channels = 1;

  dst.processing_frame_ms = frame_size;
  dst.sample_rate = dst_rate;
  dst.channels = 1;

  if (template_type == TemplateType::INPUT) {
    return BuildSoundStreamTIn<T>(src, dst);
  } else if (template_type == TemplateType::OUTPUT) {
    return BuildSoundStreamTOut<T>(src, dst);
  } else {
    throw std::runtime_error("SoundStream::BuildSoundStream not able to fetch template type");
  }
}

template <typename TIN>
std::unique_ptr<ISoundStream<TIN, TIN>> BuildSoundStreamTIn(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::move(std::make_unique<SoundStream<TIN, TIN>>(src_info, dst_info));
}

template <typename TOUT>
std::unique_ptr<ISoundStream<TOUT, TOUT>> BuildSoundStreamTOut(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::move(std::make_unique<SoundStream<TOUT, TOUT>>(src_info, dst_info));
}

// --- Diagnostics implementations ---
template <typename TIN, typename TOUT>
size_t SoundStream<TIN, TOUT>::PendingSamples() const {
  return m_output_buffer.size();
}

template <typename TIN, typename TOUT>
double SoundStream<TIN, TOUT>::PendingMs() const {
  if (ISoundStream<TIN, TOUT>::m_out.sample_rate == 0) return 0.0;
  return (static_cast<double>(m_output_buffer.size()) * 1000.0) /
         static_cast<double>(ISoundStream<TIN, TOUT>::m_out.sample_rate);
}

template <typename TIN, typename TOUT>
double SoundStream<TIN, TOUT>::GetAccumulatedDelayMs() const {
  double ms = PendingMs();
#ifndef BUILD_WITHOUT_SOXR
  if (ISoundStream<TIN, TOUT>::m_in.sample_rate != ISoundStream<TIN, TOUT>::m_out.sample_rate && m_resampler) {
    const double d = soxr_delay(m_resampler.get());  // in output samples
    if (d > 0.0 && ISoundStream<TIN, TOUT>::m_out.sample_rate != 0) {
      ms += 1000.0 * d / static_cast<double>(ISoundStream<TIN, TOUT>::m_out.sample_rate);
    }
  }
#endif
  return ms;
}

template <typename TIN, typename TOUT>
bool SoundStream<TIN, TOUT>::ShouldWarnDelay(double threshold_ms,
                                             double min_interval_ms,
                                             double* out_delay_ms) {
  const double delay_ms = GetAccumulatedDelayMs();
  if (out_delay_ms) {
    *out_delay_ms = delay_ms;
  }
  if (delay_ms <= threshold_ms) {
    return false;
  }

  // rate-limit by emitted-audio time
  const double emitted_ms = (ISoundStream<TIN, TOUT>::m_out.sample_rate == 0) ? 0.0 :
    (static_cast<double>(m_emitted_samples_total) * 1000.0) /
    static_cast<double>(ISoundStream<TIN, TOUT>::m_out.sample_rate);

  if (m_last_warn_emitted_ms < 0.0 || emitted_ms - m_last_warn_emitted_ms >= min_interval_ms) {
    m_last_warn_emitted_ms = emitted_ms;
    return true;
  }
  return false;
}

}  // namespace sound
}  // namespace util

// declarations
template class util::sound::SoundStream<float, float>;

template std::unique_ptr<util::sound::ISoundStream<float, float>,
                         std::default_delete<util::sound::ISoundStream<float, float>>>
                              util::sound::BuildSoundStream<float>(uint32_t,
                                                                   uint32_t,
                                                                   uint32_t,
                                                                   util::sound::TemplateType);

template std::unique_ptr<util::sound::ISoundStream<float, float>,
                         std::default_delete<util::sound::ISoundStream<float, float> >>
                              util::sound::BuildSoundStreamTIn<float>(util::sound::SoundInfo const&,
                                                                      util::sound::SoundInfo const&);

template std::unique_ptr<util::sound::ISoundStream<float, float>,
                         std::default_delete<util::sound::ISoundStream<float, float> >>
                              util::sound::BuildSoundStreamTOut<float>(util::sound::SoundInfo const&,
                                                                      util::sound::SoundInfo const&);
