#include <spench/util/sound/sound_stream.h>
#include <spench/util/sound/sound_types.h>

#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace util {
namespace sound {

SoundStream::SoundStream(const SoundInfo& src, const SoundInfo& dst)
    : ISoundStream(src, dst),
      m_resampler{src.sample_rate, dst.sample_rate, src.channels},
      m_nominal_ratio(dst.sample_rate == 0 || src.sample_rate == 0
                          ? 1.0
                          : static_cast<double>(dst.sample_rate) / static_cast<double>(src.sample_rate)) {
  // Allow only mono↔mono or mono↔stereo (one side must be mono)
  if (src.channels != dst.channels && !(src.channels == 1 || dst.channels == 1)) {
    throw std::runtime_error("SoundStream::SoundStream: channel conversion from " + std::to_string(src.channels) +
                             " to " + std::to_string(dst.channels) + " is not supported!");
  }
  // Enable drift-control scheme only when resampling and 44.1 kHz is involved.
  m_enable_drift_control =
      (src.sample_rate != dst.sample_rate) && (src.sample_rate == 44100 || dst.sample_rate == 44100);
  // Allow varying ratio only within the scheme; it may still be disabled later if soxr rejects
  // io_ratio.
  m_allow_varying_ratio = m_enable_drift_control;
}

std::vector<float> SoundStream::ReadChunkToVector(const std::vector<float>& in_chunk) {
  std::vector<float> result;
  if (!in_chunk.size()) {
    return {};
  }

  // Unified channel rule: allow only if one side is mono.
  if (m_in.channels != m_out.channels && !(m_in.channels == 1 || m_out.channels == 1)) {
    throw std::runtime_error("SoundStream::ReadChunk: channel conversion is not supported for non-mono cases");
  }

  if (m_in.sample_rate != m_out.sample_rate) {
    // Apply drift control only when enabled (44.1 kHz involved).
    if (m_enable_drift_control) {
      ApplyDriftControl();
    }
  }

  return m_resampler.process(in_chunk);
}

// --- Drift control implementation ---
void SoundStream::ApplyDriftControl() {
  // Soft drift control window and limits (only when 44.1 kHz is involved).
  const double delay_ms = GetAccumulatedDelayMs();
  double rel = 0.0;
  if (delay_ms > m_drift_hi_ms)
    rel = -m_drift_max_rel;
  else if (delay_ms < m_drift_lo_ms)
    rel = +m_drift_max_rel;

  bool ratio_set = false;
  if (m_allow_varying_ratio) {
    const double io_ratio = m_nominal_ratio * (1.0 + rel);
    ratio_set = m_resampler.set_io_ratio(io_ratio);

    if (!ratio_set) {
      m_allow_varying_ratio = false;  // stop trying; fall back from now on
    }
  }
  if (!m_allow_varying_ratio) {
    ApplyFallbackDriftCorrection(delay_ms);
  }
}

// --- Fallback drift correction: rare single-sample insert/remove with interpolation.
void SoundStream::ApplyFallbackDriftCorrection(double delay_ms) {
  const double emitted_ms = (ISoundStream::m_out.sample_rate == 0)
                                ? 0.0
                                : (static_cast<double>(m_emitted_samples_total) * 1000.0) /
                                      static_cast<double>(ISoundStream::m_out.sample_rate);

  if (m_last_adjust_emitted_ms < 0.0 || emitted_ms - m_last_adjust_emitted_ms >= m_drift_min_adjust_interval_ms) {
    if (delay_ms > m_drift_hi_ms && m_output_buffer.size() > 2) {
      // remove one sample near the front; smooth boundary
      float a = m_output_buffer[0];
      float b = m_output_buffer[1];
      m_output_buffer[0] = (a + b) * 0.5;
      m_output_buffer.erase(std::begin(m_output_buffer) + 1);
      m_last_adjust_emitted_ms = emitted_ms;
    } else if (delay_ms < m_drift_lo_ms && m_output_buffer.size() >= 2) {
      // insert one interpolated sample near the back
      float p = *(std::end(m_output_buffer) - 2);
      float q = *(std::end(m_output_buffer) - 1);
      float mid = (p + q) * 0.5;
      m_output_buffer.insert(std::end(m_output_buffer) - 1, mid);
      m_last_adjust_emitted_ms = emitted_ms;
    }
  }
}

namespace {

inline std::size_t alignedSampleCount(std::size_t total, std::size_t frame_samples) {
  return (total / frame_samples) * frame_samples;
}

std::vector<float> takeAlignedFrames(std::deque<float>& pending, std::size_t frame_samples) {
  const std::size_t emit_samples = alignedSampleCount(pending.size(), frame_samples);
  if (emit_samples == 0) {
    return {};
  }

  std::vector<float> out(emit_samples);
  std::move(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(emit_samples), out.begin());
  pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(emit_samples));
  return out;
}

}  // namespace

std::vector<float> SoundStream::ReadChunk(const std::vector<float>& in_chunk) {
  if (m_in.sample_rate == m_out.sample_rate) {
    return in_chunk;
  }

  const std::size_t frame_samples = m_out.FrameSizeSamples();
  if (frame_samples == 0) {
    return {};
  }

  if (!in_chunk.empty()) {
    std::vector<float> resampled = ReadChunkToVector(in_chunk);
    if (!resampled.empty()) {
      m_output_buffer.insert(m_output_buffer.end(),
                             std::make_move_iterator(resampled.begin()),
                             std::make_move_iterator(resampled.end()));
    }
  }

  // Emit every complete frame_stride block at the output rate (fixes variable-length
  // chunks). Partial remainder stays buffered for the next call.
  std::vector<float> out = takeAlignedFrames(m_output_buffer, frame_samples);
  if (!out.empty()) {
    m_emitted_samples_total += out.size();
  }
  return out;
}

std::vector<float> SoundStream::FinalizeStream() {
  std::vector<float> result;
  std::move(std::begin(m_output_buffer), std::end(m_output_buffer), std::back_inserter(result));

  // Always flush if rates differ, regardless of m_in_resample flag.
  if (m_in.sample_rate != m_out.sample_rate) {
    std::vector<float> flushed_out = m_resampler.flush();
    std::move(std::begin(flushed_out), std::end(flushed_out), std::back_inserter(result));
  }

  m_output_buffer.clear();
  m_emitted_samples_total += result.size();
  return result;
}

std::unique_ptr<ISoundStream> BuildSoundStream(uint32_t src_rate, uint32_t dst_rate, uint32_t frame_size,
                                               TemplateType template_type) {
  SoundInfo src;
  SoundInfo dst;

  src.processing_frame_ms = frame_size;
  src.sample_rate = src_rate;
  src.channels = 1;

  dst.processing_frame_ms = frame_size;
  dst.sample_rate = dst_rate;
  dst.channels = 1;

  if (template_type == TemplateType::INPUT) {
    return BuildSoundStreamTIn(src, dst);
  } else if (template_type == TemplateType::OUTPUT) {
    return BuildSoundStreamTOut(src, dst);
  } else {
    throw std::runtime_error("SoundStream::BuildSoundStream not able to fetch template type");
  }
}

std::unique_ptr<ISoundStream> BuildSoundStreamTIn(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::make_unique<SoundStream>(src_info, dst_info);
}

std::unique_ptr<ISoundStream> BuildSoundStreamTOut(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::make_unique<SoundStream>(src_info, dst_info);
}

// --- Diagnostics implementations ---
size_t SoundStream::PendingSamples() const { return m_output_buffer.size(); }

double SoundStream::PendingMs() const {
  if (m_out.sample_rate == 0) {
    return 0.0;
  }
  return (static_cast<double>(m_output_buffer.size()) * 1000.0) / static_cast<double>(m_out.sample_rate);
}

double SoundStream::GetAccumulatedDelayMs() const {
  double ms = PendingMs() + m_resampler.accumulated_delay_ms();
  return ms;
}

bool SoundStream::ShouldWarnDelay(double threshold_ms, double min_interval_ms, double* out_delay_ms) {
  const double delay_ms = GetAccumulatedDelayMs();
  if (out_delay_ms) {
    *out_delay_ms = delay_ms;
  }
  if (delay_ms <= threshold_ms) {
    return false;
  }

  // rate-limit by emitted-audio time
  const double emitted_ms = (m_out.sample_rate == 0) ? 0.0
                                                     : (static_cast<double>(m_emitted_samples_total) * 1000.0) /
                                                           static_cast<double>(m_out.sample_rate);

  if (m_last_warn_emitted_ms < 0.0 || emitted_ms - m_last_warn_emitted_ms >= min_interval_ms) {
    m_last_warn_emitted_ms = emitted_ms;
    return true;
  }
  return false;
}

}  // namespace sound
}  // namespace util
