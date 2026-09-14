#ifndef SPENCH_PROCESSOR_H
#define SPENCH_PROCESSOR_H

#include <spench/packed_model.h>
#include <spench/spench_audio_denoise.h>
#include <spench/spench_audio_process.h>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "spench/ring_buffer.h"

namespace spench {

struct SpenchProcessorSettings {
  double low_performance_window_seconds = 1.0;
  std::int32_t low_performance_max_refill_times = 4;
  double low_performance_max_refill_time_factor = 3.0;
  double min_latency = 0.064;
  double latency_increase_step = 0.032;
  double max_processing_chunk_seconds = 0.016;
  double telemetry_rounding_milliseconds = 0.25;
  bool use_vad_normalization = false;
  bool unstable_reverse_processing_order = false;
  std::string unstable_custom_normalization_config;
};

struct SpenchTelemetrySnapshot {
  std::map<std::int32_t, std::int32_t> normalization_counters;
  std::map<std::int32_t, std::int32_t> noise_suppression_counters;
  double rounding_milliseconds = 0.25;
};

struct SpenchLowPerformanceEventNative {
  std::uint32_t sample_rate_hz = 0;
  double current_latency_seconds = 0.0;
  double mean_processing_time_milliseconds = -1.0;
  std::int32_t refill_count = 0;
};

class SpenchProcessor {
 public:
  explicit SpenchProcessor(std::uint32_t sample_rate_hz, SpenchProcessorSettings settings = SpenchProcessorSettings());
  ~SpenchProcessor();

  SpenchProcessor(const SpenchProcessor&) = delete;
  SpenchProcessor& operator=(const SpenchProcessor&) = delete;

  void reset();
  void set_features(bool normalization, const std::string& noise_suppression_model,
                    std::shared_ptr<PackedModelReader> model_reader);
  void process(const float* input, float* output, std::size_t num_frames);
  SpenchTelemetrySnapshot telemetry_snapshot() const;
  std::vector<SpenchLowPerformanceEventNative> drain_low_performance_events();

 private:
  static constexpr std::size_t default_ring_capacity = 32768;
  static constexpr double max_output_buffer_duration_seconds = 0.256;
  static constexpr std::size_t mean_processing_time_count = 60;
  static constexpr std::size_t max_pending_low_performance_events = 16;

  struct LowPerformanceMonitor {
    explicit LowPerformanceMonitor(const SpenchProcessorSettings& settings, std::uint32_t sample_rate_hz);
    void reset(std::uint32_t sample_rate_hz);
    void mark_refilled();
    bool should_relax_refill_requirement(double now_seconds);
    std::optional<SpenchLowPerformanceEventNative> handle_underrun(
        double now_seconds, std::optional<double> mean_processing_time_milliseconds);
    std::size_t prime_threshold_frames() const;

    const SpenchProcessorSettings& settings;
    std::uint32_t sample_rate_hz;
    double current_latency_seconds;
    std::vector<double> refill_times;
    double refill_start_time;
  };

  void rebuild_audio_processor_locked();
  void rebuild_denoiser_locked(const std::string& model_name, std::shared_ptr<PackedModelReader> model_reader);
  void reset_processors_locked();
  void destroy_denoiser_context_locked();
  void clear_ring_buffers_locked();
  void reset_congestion_locked();
  void record_telemetry(bool normalization, double milliseconds);
  void record_processing_time_locked(double milliseconds);
  std::optional<double> mean_processing_time_locked() const;
  void enqueue_low_performance_event_locked(SpenchLowPerformanceEventNative event);
  bool should_render_output_locked(double now_seconds, std::size_t available_frames);
  std::optional<SpenchLowPerformanceEventNative> handle_output_underrun_locked(double now_seconds, std::size_t popped,
                                                                               std::size_t requested);
  void worker_loop() noexcept;
  static void validate_settings(const SpenchProcessorSettings& settings);
  static std::size_t ring_capacity_for_sample_rate(
        std::uint32_t sample_rate_hz,
        const SpenchProcessorSettings& settings);
  std::string normalization_config() const;

  RingBuffer input_ring_buffer_;
  RingBuffer output_ring_buffer_;

  // Lock ordering when more than one is held: state_mutex_ -> io_mutex_ -> telemetry_mutex_.
  // telemetry_mutex_ guards only the telemetry histograms so telemetry_snapshot() never has to
  // wait for a full ProcessChunk (which runs under state_mutex_).
  mutable std::mutex state_mutex_;
  mutable std::mutex io_mutex_;
  mutable std::mutex telemetry_mutex_;
  std::condition_variable worker_condition_;
  std::thread worker_thread_;
  std::vector<float> worker_input_buffer_;
  std::vector<float> worker_output_buffer_;
  std::vector<float> processor_input_buffer_;
  std::vector<float> processor_output_buffer_;
  std::vector<float> denoiser_output_buffer_;

  std::unique_ptr<SpenchAudioProcess> audio_processor_;
  std::unique_ptr<SpenchDenoise> denoiser_;
  SpenchContext<float>* denoiser_context_;
  std::string noise_suppression_model_;
  std::uint32_t sample_rate_hz_;
  std::size_t worker_block_frames_;
  SpenchProcessorSettings settings_;
  LowPerformanceMonitor low_performance_monitor_;
  std::map<std::int32_t, std::int32_t> normalization_telemetry_;
  std::map<std::int32_t, std::int32_t> noise_suppression_telemetry_;
  std::deque<SpenchLowPerformanceEventNative> pending_low_performance_events_;
  std::deque<double> processing_times_milliseconds_;
  std::uint32_t reset_generation_;
  std::uint32_t completed_reset_generation_;
  bool normalization_active_;
  bool output_primed_;
  bool stopping_;
};

}  // namespace spench

#endif
