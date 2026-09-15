#include <util/sound/sound_stream.h>
#include <util/sound/sound_types.h>

#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace util {
namespace sound {

SoundStream::SoundStream(const SoundInfo& src, const SoundInfo& dst)
    : ISoundStream(src, dst),
      resampler_{src.sample_rate, dst.sample_rate, src.channels},
      nominal_ratio_(dst.sample_rate == 0 || src.sample_rate == 0
                         ? 1.0
                         : static_cast<double>(dst.sample_rate) / static_cast<double>(src.sample_rate)) {
  // Allow only mono↔mono or mono↔stereo (one side must be mono)
  if (src.channels != dst.channels && !(src.channels == 1 || dst.channels == 1)) {
    throw std::runtime_error("SoundStream::SoundStream: channel conversion from " + std::to_string(src.channels) +
                             " to " + std::to_string(dst.channels) + " is not supported!");
  }
  // Enable drift-control scheme only when resampling and 44.1 kHz is involved.
  enable_drift_control_ =
      (src.sample_rate != dst.sample_rate) && (src.sample_rate == 44100 || dst.sample_rate == 44100);
  // Allow varying ratio only within the scheme; it may still be disabled later if soxr rejects
  // io_ratio.
  allow_varying_ratio_ = enable_drift_control_;
}

std::vector<float> SoundStream::read_chunk_to_vector(const std::vector<float>& inputchunk) {
  std::vector<float> result;
  if (!inputchunk.size()) {
    return {};
  }

  // Unified channel rule: allow only if one side is mono.
  if (input.channels != output.channels && !(input.channels == 1 || output.channels == 1)) {
    throw std::runtime_error("SoundStream::read_chunk: channel conversion is not supported for non-mono cases");
  }

  if (input.sample_rate != output.sample_rate) {
    // Apply drift control only when enabled (44.1 kHz involved).
    if (enable_drift_control_) {
      apply_drift_control();
    }
  }

  return resampler_.process(inputchunk);
}

// --- Drift control implementation ---
void SoundStream::apply_drift_control() {
  // Soft drift control window and limits (only when 44.1 kHz is involved).
  const double delay_ms = accumulated_delay_ms();
  double rel = 0.0;
  if (delay_ms > drift_hi_ms_)
    rel = -drift_max_rel_;
  else if (delay_ms < drift_lo_ms_)
    rel = +drift_max_rel_;

  bool ratio_set = false;
  if (allow_varying_ratio_) {
    const double io_ratio = nominal_ratio_ * (1.0 + rel);
    ratio_set = resampler_.set_io_ratio(io_ratio);

    if (!ratio_set) {
      allow_varying_ratio_ = false;  // stop trying; fall back from now on
    }
  }
  if (!allow_varying_ratio_) {
    apply_fallback_drift_correction(delay_ms);
  }
}

// --- Fallback drift correction: rare single-sample insert/remove with interpolation.
void SoundStream::apply_fallback_drift_correction(double delay_ms) {
  const double emitted_ms = (ISoundStream::output.sample_rate == 0)
                                ? 0.0
                                : (static_cast<double>(emitted_samples_total_) * 1000.0) /
                                      static_cast<double>(ISoundStream::output.sample_rate);

  if (last_adjust_emitted_ms_ < 0.0 || emitted_ms - last_adjust_emitted_ms_ >= drift_min_adjust_interval_ms_) {
    if (delay_ms > drift_hi_ms_ && output_buffer_.size() > 2) {
      // remove one sample near the front; smooth boundary
      float a = output_buffer_[0];
      float b = output_buffer_[1];
      output_buffer_[0] = (a + b) * 0.5;
      output_buffer_.erase(std::begin(output_buffer_) + 1);
      last_adjust_emitted_ms_ = emitted_ms;
    } else if (delay_ms < drift_lo_ms_ && output_buffer_.size() >= 2) {
      // insert one interpolated sample near the back
      float p = *(std::end(output_buffer_) - 2);
      float q = *(std::end(output_buffer_) - 1);
      float mid = (p + q) * 0.5;
      output_buffer_.insert(std::end(output_buffer_) - 1, mid);
      last_adjust_emitted_ms_ = emitted_ms;
    }
  }
}

namespace {

inline std::size_t aligned_sample_count(std::size_t total, std::size_t frame_samples) {
  return (total / frame_samples) * frame_samples;
}

std::vector<float> take_aligned_frames(std::deque<float>& pending, std::size_t frame_samples) {
  const std::size_t emit_samples = aligned_sample_count(pending.size(), frame_samples);
  if (emit_samples == 0) {
    return {};
  }

  std::vector<float> out(emit_samples);
  std::move(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(emit_samples), out.begin());
  pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(emit_samples));
  return out;
}

}  // namespace

std::vector<float> SoundStream::read_chunk(const std::vector<float>& in_chunk) {
  if (input.sample_rate == output.sample_rate) {
    return in_chunk;
  }

  const std::size_t frame_samples = output.frame_size_samples();
  if (frame_samples == 0) {
    return {};
  }

  if (!in_chunk.empty()) {
    std::vector<float> resampled = read_chunk_to_vector(in_chunk);
    if (!resampled.empty()) {
      output_buffer_.insert(output_buffer_.end(), std::make_move_iterator(resampled.begin()),
                            std::make_move_iterator(resampled.end()));
    }
  }

  // Emit every complete frame_stride block at the output rate (fixes variable-length
  // chunks). Partial remainder stays buffered for the next call.
  std::vector<float> out = take_aligned_frames(output_buffer_, frame_samples);
  if (!out.empty()) {
    emitted_samples_total_ += out.size();
  }
  return out;
}

std::vector<float> SoundStream::finalize_stream() {
  std::vector<float> result;
  std::move(std::begin(output_buffer_), std::end(output_buffer_), std::back_inserter(result));

  // Always flush if rates differ, regardless of inputresample_ flag.
  if (input.sample_rate != output.sample_rate) {
    std::vector<float> flushed_out = resampler_.flush();
    std::move(std::begin(flushed_out), std::end(flushed_out), std::back_inserter(result));
  }

  output_buffer_.clear();
  emitted_samples_total_ += result.size();
  return result;
}

std::unique_ptr<ISoundStream> build_sound_stream(uint32_t src_rate, uint32_t dst_rate, uint32_t frame_size,
                                                 TemplateType template_type) {
  SoundInfo src;
  SoundInfo dst;

  src.processing_frame_ms = frame_size;
  src.sample_rate = src_rate;
  src.channels = 1;

  dst.processing_frame_ms = frame_size;
  dst.sample_rate = dst_rate;
  dst.channels = 1;

  if (template_type == TemplateType::input) {
    return build_sound_stream_t_in(src, dst);
  } else if (template_type == TemplateType::output) {
    return build_sound_stream_t_out(src, dst);
  } else {
    throw std::runtime_error("SoundStream::build_sound_stream not able to fetch template type");
  }
}

std::unique_ptr<ISoundStream> build_sound_stream_t_in(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::make_unique<SoundStream>(src_info, dst_info);
}

std::unique_ptr<ISoundStream> build_sound_stream_t_out(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::make_unique<SoundStream>(src_info, dst_info);
}

// --- Diagnostics implementations ---
size_t SoundStream::pending_samples() const { return output_buffer_.size(); }

double SoundStream::pending_ms() const {
  if (output.sample_rate == 0) {
    return 0.0;
  }
  return (static_cast<double>(output_buffer_.size()) * 1000.0) / static_cast<double>(output.sample_rate);
}

double SoundStream::accumulated_delay_ms() const {
  double ms = pending_ms() + resampler_.accumulated_delay_ms();
  return ms;
}

bool SoundStream::should_warn_delay(double threshold_ms, double min_interval_ms, double* out_delay_ms) {
  const double delay_ms = accumulated_delay_ms();
  if (out_delay_ms) {
    *out_delay_ms = delay_ms;
  }
  if (delay_ms <= threshold_ms) {
    return false;
  }

  // rate-limit by emitted-audio time
  const double emitted_ms = (output.sample_rate == 0) ? 0.0
                                                      : (static_cast<double>(emitted_samples_total_) * 1000.0) /
                                                            static_cast<double>(output.sample_rate);

  if (last_warn_emitted_ms_ < 0.0 || emitted_ms - last_warn_emitted_ms_ >= min_interval_ms) {
    last_warn_emitted_ms_ = emitted_ms;
    return true;
  }
  return false;
}

}  // namespace sound
}  // namespace util
