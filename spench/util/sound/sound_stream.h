#pragma once

#ifndef BUILD_WITHOUT_SOXR
#include <spench/util/sound/resampler.h>
#endif
#include <spench/util/sound/sound_types.h>

#include <cstring>
#include <memory>
#include <vector>
#include <deque>

namespace util {
namespace sound {

template <typename TIN, typename TOUT>
class ISoundStream {
 public:
  virtual std::vector<TOUT> ReadChunk(const std::vector<TIN>& in_chunk) = 0;
  virtual ~ISoundStream();
  virtual std::vector<TOUT> FinalizeStream() = 0;

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

  ISoundStream(const SoundInfo& in, const SoundInfo& out)
      : m_in(in)
      , m_out(out) {}
};

template <typename TIN, typename TOUT>
class SoundStream : public ISoundStream<TIN, TOUT> {
 public:
  SoundStream(const SoundInfo& src, const SoundInfo& dst);
  virtual ~SoundStream();
  std::vector<TOUT> ReadChunk(const std::vector<TIN>& in_chunk) override;
  std::vector<TOUT> ReadChunkToVector(const std::vector<TIN>& in_chunk);
  std::vector<TOUT> FinalizeStream() override;

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

  std::deque<TOUT> m_output_buffer;
#ifndef BUILD_WITHOUT_SOXR
  ResampleWrapper m_resampler;
  double   m_nominal_ratio {1.0};        // out_rate / in_rate at construction time
#endif
  bool     m_in_resample {false};
  // --- drift control & logging ---
  bool     m_enable_drift_control {false};  // true only if resampling involves 44100 Hz
  uint64_t m_emitted_samples_total {0};     // total samples emitted to caller (out-domain)
  double   m_last_warn_emitted_ms  {-1};    // last emitted-ms when we logged a warning
  bool     m_allow_varying_ratio {true};    // allowed only when m_enable_drift_control;
                                            // disabled if SetResamplerIoRatio fails
  double   m_last_adjust_emitted_ms {-1};   // rate-limit fallback adjustments

  // --- drift control parameters (tunable) ---
  // High/low target window for accumulated delay (ms).
  double   m_drift_hi_ms {350.0};
  double   m_drift_lo_ms {100.0};
  // Maximum relative ratio adjustment (e.g. 0.003 = ±0.3%).
  double   m_drift_max_rel {0.003};
  // Minimum interval between fallback insert/remove operations (ms).
  double   m_drift_min_adjust_interval_ms {1000.0};
};

enum TemplateType {
  INPUT = 0,
  OUTPUT = 1
};

template <typename T>
std::unique_ptr<ISoundStream<T, T>> BuildSoundStream(uint32_t src_rate,
                                                     uint32_t dst_rate,
                                                     uint32_t frame_size,
                                                     TemplateType template_type);
template <typename TIN>
std::unique_ptr<ISoundStream<TIN, TIN>> BuildSoundStreamTIn(const SoundInfo& src_info,
                                                            const SoundInfo& dst_info);
template <typename TOUT>
std::unique_ptr<ISoundStream<TOUT, TOUT>> BuildSoundStreamTOut(const SoundInfo& src_info,
                                                               const SoundInfo& dst_info);
}  // namespace sound
}  // namespace util
