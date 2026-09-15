#pragma once

#include <util/sound/resampler.h>
#include <util/sound/sound_types.h>

#include <cstring>
#include <deque>
#include <memory>
#include <vector>

namespace util {
namespace sound {

class ISoundStream {
 public:
  virtual ~ISoundStream() = default;

  virtual std::vector<float> read_chunk(const std::vector<float>& in_chunk) = 0;

  virtual std::vector<float> finalize_stream() = 0;

  // Returns number of output-domain samples queued in internal FIFO.
  virtual size_t pending_samples() const = 0;
  // Returns milliseconds represented by pending_samples().
  virtual double pending_ms() const = 0;
  // Returns pending_ms() + internal resampler filter delay (if any).
  virtual double accumulated_delay_ms() const = 0;
  // Rate-limited warning gate
  virtual bool should_warn_delay(double threshold_ms, double min_interval_ms, double* out_delay_ms) = 0;

  SoundInfo input;
  SoundInfo output;

  ISoundStream(const SoundInfo& in, const SoundInfo& out) : input(in), output(out) {}
};

class SoundStream : public ISoundStream {
 public:
  SoundStream(const SoundInfo& src, const SoundInfo& dst);
  virtual ~SoundStream() = default;

  std::vector<float> read_chunk(const std::vector<float>& in_chunk) override;
  std::vector<float> read_chunk_to_vector(const std::vector<float>& in_chunk);
  std::vector<float> finalize_stream() override;

  // Diagnostics
  size_t pending_samples() const override;
  double pending_ms() const override;
  double accumulated_delay_ms() const override;
  bool should_warn_delay(double threshold_ms, double min_interval_ms, double* out_delay_ms) override;

 private:
  // Runs drift control (variable-rate or fallback) if enabled for this stream.
  void apply_drift_control();
  // Fallback: rare single-sample insert/remove with interpolation.
  void apply_fallback_drift_correction(double delay_ms);

  std::deque<float> output_buffer_;

  Resampler resampler_;
  double nominal_ratio_{1.0};  // out_rate / in_rate at construction time

  // --- drift control & logging ---
  bool enable_drift_control_{false};   // true only if resampling involves 44100 Hz
  uint64_t emitted_samples_total_{0};  // total samples emitted to caller (out-domain)
  double last_warn_emitted_ms_{-1};    // last emitted-ms when we logged a warning
  bool allow_varying_ratio_{true};     // allowed only when enable_drift_control_;
                                       // disabled if SetResamplerIoRatio fails
  double last_adjust_emitted_ms_{-1};  // rate-limit fallback adjustments

  // --- drift control parameters (tunable) ---
  // High/low target window for accumulated delay (ms).
  double drift_hi_ms_{350.0};
  double drift_lo_ms_{100.0};
  // Maximum relative ratio adjustment (e.g. 0.003 = ±0.3%).
  double drift_max_rel_{0.003};
  // Minimum interval between fallback insert/remove operations (ms).
  double drift_min_adjust_interval_ms_{1000.0};
};

enum TemplateType { input = 0, output = 1 };

std::unique_ptr<ISoundStream> build_sound_stream(uint32_t src_rate, uint32_t dst_rate, uint32_t frame_size,
                                                 TemplateType template_type);

std::unique_ptr<ISoundStream> build_sound_stream_t_in(const SoundInfo& src_info, const SoundInfo& dst_info);

std::unique_ptr<ISoundStream> build_sound_stream_t_out(const SoundInfo& src_info, const SoundInfo& dst_info);

}  // namespace sound
}  // namespace util
