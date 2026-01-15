#pragma once

#include <spench/util/sound/resampler.h>
#include <spench/util/sound/sound_types.h>

#include <cstring>
#include <deque>
#include <memory>
#include <vector>

namespace util {
namespace sound {

class ISoundStream {
 public:
  virtual ~ISoundStream() = default;

  virtual std::vector<float> ReadChunk(const std::vector<float>& in_chunk) = 0;

  virtual std::vector<float> FinalizeStream() = 0;

  // Returns number of output-domain samples queued in internal FIFO.
  virtual size_t PendingSamples() const = 0;
  // Returns milliseconds represented by PendingSamples().
  virtual double PendingMs() const = 0;
  // Returns PendingMs() + internal resampler filter delay (if any).
  virtual double GetAccumulatedDelayMs() const = 0;
  // Rate-limited warning gate
  virtual bool ShouldWarnDelay(double threshold_ms, double min_interval_ms, double* out_delay_ms) = 0;

  SoundInfo m_in;
  SoundInfo m_out;

  ISoundStream(const SoundInfo& in, const SoundInfo& out) : m_in(in), m_out(out) {}
};

class SoundStream : public ISoundStream {
 public:
  SoundStream(const SoundInfo& src, const SoundInfo& dst);
  virtual ~SoundStream() = default;

  std::vector<float> ReadChunk(const std::vector<float>& in_chunk) override;
  std::vector<float> ReadChunkToVector(const std::vector<float>& in_chunk);
  std::vector<float> FinalizeStream() override;

  // Diagnostics
  size_t PendingSamples() const override;
  double PendingMs() const override;
  double GetAccumulatedDelayMs() const override;
  bool ShouldWarnDelay(double threshold_ms, double min_interval_ms, double* out_delay_ms) override;

 private:
  // Runs drift control (variable-rate or fallback) if enabled for this stream.
  void ApplyDriftControl();
  // Fallback: rare single-sample insert/remove with interpolation.
  void ApplyFallbackDriftCorrection(double delay_ms);

  std::deque<float> m_output_buffer;

  Resampler m_resampler;
  double m_nominal_ratio{1.0};  // out_rate / in_rate at construction time

  // --- drift control & logging ---
  bool m_enable_drift_control{false};   // true only if resampling involves 44100 Hz
  uint64_t m_emitted_samples_total{0};  // total samples emitted to caller (out-domain)
  double m_last_warn_emitted_ms{-1};    // last emitted-ms when we logged a warning
  bool m_allow_varying_ratio{true};     // allowed only when m_enable_drift_control;
                                        // disabled if SetResamplerIoRatio fails
  double m_last_adjust_emitted_ms{-1};  // rate-limit fallback adjustments

  // --- drift control parameters (tunable) ---
  // High/low target window for accumulated delay (ms).
  double m_drift_hi_ms{350.0};
  double m_drift_lo_ms{100.0};
  // Maximum relative ratio adjustment (e.g. 0.003 = ±0.3%).
  double m_drift_max_rel{0.003};
  // Minimum interval between fallback insert/remove operations (ms).
  double m_drift_min_adjust_interval_ms{1000.0};
};

enum TemplateType { INPUT = 0, OUTPUT = 1 };

std::unique_ptr<ISoundStream> BuildSoundStream(uint32_t src_rate, uint32_t dst_rate, uint32_t frame_size,
                                               TemplateType template_type);

std::unique_ptr<ISoundStream> BuildSoundStreamTIn(const SoundInfo& src_info, const SoundInfo& dst_info);

std::unique_ptr<ISoundStream> BuildSoundStreamTOut(const SoundInfo& src_info, const SoundInfo& dst_info);

}  // namespace sound
}  // namespace util
