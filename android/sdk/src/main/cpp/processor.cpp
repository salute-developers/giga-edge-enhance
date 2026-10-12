#include "spench/processor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace gigaedge::spench {

namespace {

constexpr double processing_chunk_step_seconds = 0.016;
constexpr double max_processing_chunk_seconds = 0.256;

double processing_chunk_seconds(const ProcessorSettings& settings) {
  // Constructor member initialization sizes buffers before validate_settings() runs.
  // Keep invalid direct-C++ settings from reaching a floating-point-to-size conversion.
  if (!std::isfinite(settings.max_processing_chunk_seconds) || !std::isfinite(settings.min_latency)) {
    return processing_chunk_step_seconds;
  }
  const double requested_seconds =
      std::min({settings.max_processing_chunk_seconds, settings.min_latency, max_processing_chunk_seconds});
  if (requested_seconds <= processing_chunk_step_seconds) {
    return processing_chunk_step_seconds;
  }

  return std::max(std::floor(requested_seconds / processing_chunk_step_seconds) * processing_chunk_step_seconds,
                  processing_chunk_step_seconds);
}

std::size_t chunk_frames_for_sample_rate(std::uint32_t sample_rate_hz, const ProcessorSettings& settings) {
  return static_cast<std::size_t>(std::ceil(sample_rate_hz * processing_chunk_seconds(settings)));
}

double now_seconds() {
  using Clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

double elapsed_milliseconds(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool finite(double value) { return std::isfinite(value); }

}  // namespace

Processor::LowPerformanceMonitor::LowPerformanceMonitor(const ProcessorSettings& settings, std::uint32_t sample_rate_hz)
    : settings(settings),
      sample_rate_hz(sample_rate_hz),
      current_latency_seconds(
          std::min(std::max(settings.min_latency, 0.0), Processor::max_output_buffer_duration_seconds)),
      refill_times(),
      refill_start_time(std::numeric_limits<double>::infinity()) {}

void Processor::LowPerformanceMonitor::reset(std::uint32_t new_sample_rate_hz) {
  sample_rate_hz = new_sample_rate_hz;
  current_latency_seconds =
      std::min(std::max(settings.min_latency, 0.0), Processor::max_output_buffer_duration_seconds);
  refill_times.clear();
  refill_start_time = std::numeric_limits<double>::infinity();
}

void Processor::LowPerformanceMonitor::mark_refilled() { refill_start_time = std::numeric_limits<double>::infinity(); }

bool Processor::LowPerformanceMonitor::should_relax_refill_requirement(double current_time_seconds) {
  if (!std::isfinite(refill_start_time)) {
    return false;
  }
  const double elapsed = current_time_seconds - refill_start_time;
  const double max_refill_time = settings.low_performance_max_refill_time_factor * current_latency_seconds;
  if (elapsed <= max_refill_time) {
    return false;
  }
  refill_start_time = std::numeric_limits<double>::infinity();
  return true;
}

std::optional<LowPerformanceEventNative> Processor::LowPerformanceMonitor::handle_underrun(
    double current_time_seconds, std::optional<double> mean_processing_time_milliseconds) {
  refill_start_time = current_time_seconds;
  refill_times.erase(std::remove_if(refill_times.begin(), refill_times.end(),
                                    [this, current_time_seconds](double refill_time) {
                                      return current_time_seconds - refill_time >
                                             settings.low_performance_window_seconds;
                                    }),
                     refill_times.end());
  refill_times.push_back(current_time_seconds);

  current_latency_seconds =
      std::min(static_cast<double>(refill_times.size() - 1U) * settings.latency_increase_step + settings.min_latency,
               Processor::max_output_buffer_duration_seconds);

  if (refill_times.size() < static_cast<std::size_t>(settings.low_performance_max_refill_times)) {
    return std::nullopt;
  }

  LowPerformanceEventNative event;
  event.sample_rate_hz = sample_rate_hz;
  event.current_latency_seconds = current_latency_seconds;
  event.mean_processing_time_milliseconds = mean_processing_time_milliseconds.value_or(-1.0);
  event.refill_count = static_cast<std::int32_t>(refill_times.size());
  refill_times.clear();
  refill_start_time = std::numeric_limits<double>::infinity();
  return event;
}

std::size_t Processor::LowPerformanceMonitor::prime_threshold_frames() const {
  return std::max<std::size_t>(1U, static_cast<std::size_t>(std::ceil(current_latency_seconds * sample_rate_hz)));
}

Processor::Processor(std::uint32_t sample_rate_hz, ProcessorSettings settings)
    : input_ring_buffer_(ring_capacity_for_sample_rate(sample_rate_hz, settings)),
      output_ring_buffer_(ring_capacity_for_sample_rate(sample_rate_hz, settings)),
      worker_input_buffer_(chunk_frames_for_sample_rate(sample_rate_hz, settings), 0.0f),
      worker_output_buffer_(chunk_frames_for_sample_rate(sample_rate_hz, settings), 0.0f),
      processor_input_buffer_(),
      processor_output_buffer_(),
      denoiser_output_buffer_(),
      audio_processor_(nullptr),
      denoiser_(nullptr),
      denoiser_context_(nullptr),
      noise_suppression_model_(),
      sample_rate_hz_(sample_rate_hz),
      worker_block_frames_(chunk_frames_for_sample_rate(sample_rate_hz, settings)),
      settings_(std::move(settings)),
      low_performance_monitor_(settings_, sample_rate_hz),
      normalization_telemetry_(),
      noise_suppression_telemetry_(),
      pending_low_performance_events_(),
      processing_times_milliseconds_(),
      reset_generation_(0U),
      completed_reset_generation_(0U),
      normalization_active_(false),
      output_primed_(false),
      stopping_(false) {
  if (sample_rate_hz_ == 0U || worker_block_frames_ == 0U) {
    throw std::invalid_argument("sample_rate_hz must be greater than zero");
  }
  validate_settings(settings_);
  worker_thread_ = std::thread(&Processor::worker_loop, this);
}

Processor::~Processor() {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    stopping_ = true;
  }
  worker_condition_.notify_all();
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  std::lock_guard<std::mutex> lock(state_mutex_);
  destroy_denoiser_context_locked();
}

void Processor::destroy_denoiser_context_locked() {
  if (denoiser_ != nullptr && denoiser_context_ != nullptr) {
    denoiser_->DestroyContext(&denoiser_context_);
  }
  denoiser_context_ = nullptr;
}

void Processor::rebuild_audio_processor_locked() {
  audio_processor_ = std::make_unique<AudioProcessor>(normalization_config());
  audio_processor_->AddLC().AddNG().AddLim();
}

std::string Processor::normalization_config() const {
  if (!settings_.unstable_custom_normalization_config.empty()) {
    return settings_.unstable_custom_normalization_config;
  }
  const std::string sample_rate = std::to_string(sample_rate_hz_);
  std::string vad_config;
  if (settings_.use_vad_normalization) {
    vad_config = "\n    use_vad: true\n    target_level_db: -18.0";
  }
  return "sound_processing:\n"
         "  loudness_ctrl:\n"
         "    sample_rate: " +
         sample_rate + vad_config +
         "\n"
         "  dynamic_range_compressor_ctrl:\n"
         "    sample_rate: " +
         sample_rate +
         "\n"
         "  noise_gate_ctrl:\n"
         "    sample_rate: " +
         sample_rate +
         "\n"
         "  limiter_ctrl:\n"
         "    sample_rate: " +
         sample_rate + "\n";
}

void Processor::rebuild_denoiser_locked(const std::string& model_name,
                                        std::shared_ptr<PackedModelReader> model_reader) {
  if (model_name.empty()) {
    destroy_denoiser_context_locked();
    denoiser_.reset();
    noise_suppression_model_.clear();
    return;
  }
  if (model_reader == nullptr) {
    throw std::invalid_argument("model_reader must not be null when denoising is enabled");
  }

  destroy_denoiser_context_locked();
  denoiser_ = std::make_unique<Denoiser>(std::move(model_reader));
  denoiser_context_ = denoiser_->CreateContext(sample_rate_hz_, sample_rate_hz_);
  noise_suppression_model_ = model_name;
}

void Processor::reset_processors_locked() {
  if (normalization_active_) {
    rebuild_audio_processor_locked();
  }
  if (denoiser_ != nullptr) {
    destroy_denoiser_context_locked();
    denoiser_context_ = denoiser_->CreateContext(sample_rate_hz_, sample_rate_hz_);
  }
}

void Processor::clear_ring_buffers_locked() {
  input_ring_buffer_.reset();
  output_ring_buffer_.reset();
}

void Processor::reset_congestion_locked() {
  output_primed_ = false;
  low_performance_monitor_.reset(sample_rate_hz_);
  pending_low_performance_events_.clear();
  processing_times_milliseconds_.clear();
}

void Processor::record_telemetry(bool normalization, double milliseconds) {
  const double scaled_bucket = milliseconds / settings_.telemetry_rounding_milliseconds;
  const std::int32_t bucket = !finite(scaled_bucket) || scaled_bucket >= std::numeric_limits<std::int32_t>::max()
                                  ? std::numeric_limits<std::int32_t>::max()
                                  : static_cast<std::int32_t>(std::llround(scaled_bucket));
  std::lock_guard<std::mutex> lock(telemetry_mutex_);
  std::int32_t& count = normalization ? normalization_telemetry_[bucket] : noise_suppression_telemetry_[bucket];
  if (count < std::numeric_limits<std::int32_t>::max()) {
    ++count;
  }
}

void Processor::record_processing_time_locked(double milliseconds) {
  processing_times_milliseconds_.push_back(milliseconds);
  while (processing_times_milliseconds_.size() > mean_processing_time_count) {
    processing_times_milliseconds_.pop_front();
  }
}

std::optional<double> Processor::mean_processing_time_locked() const {
  if (processing_times_milliseconds_.empty()) {
    return std::nullopt;
  }
  const double sum = std::accumulate(processing_times_milliseconds_.begin(), processing_times_milliseconds_.end(), 0.0);
  return sum / static_cast<double>(processing_times_milliseconds_.size());
}

void Processor::enqueue_low_performance_event_locked(LowPerformanceEventNative event) {
  if (pending_low_performance_events_.size() >= max_pending_low_performance_events) {
    pending_low_performance_events_.pop_front();
  }
  pending_low_performance_events_.push_back(event);
}

bool Processor::should_render_output_locked(double current_time_seconds, std::size_t available_frames) {
  if (low_performance_monitor_.should_relax_refill_requirement(current_time_seconds)) {
    output_primed_ = true;
  }
  if (output_primed_) {
    return true;
  }
  if (available_frames >= low_performance_monitor_.prime_threshold_frames()) {
    output_primed_ = true;
    low_performance_monitor_.mark_refilled();
    return true;
  }
  return false;
}

std::optional<LowPerformanceEventNative> Processor::handle_output_underrun_locked(double current_time_seconds,
                                                                                  std::size_t popped,
                                                                                  std::size_t requested) {
  if (popped >= requested) {
    low_performance_monitor_.mark_refilled();
    return std::nullopt;
  }
  output_primed_ = false;
  return low_performance_monitor_.handle_underrun(current_time_seconds, mean_processing_time_locked());
}

void Processor::reset() {
  std::unique_lock<std::mutex> lock(state_mutex_);
  const std::uint32_t target_generation = ++reset_generation_;
  worker_condition_.notify_all();
  worker_condition_.wait(
      lock, [this, target_generation] { return stopping_ || completed_reset_generation_ >= target_generation; });
}

void Processor::set_features(bool normalization, const std::string& noise_suppression_model,
                             std::shared_ptr<PackedModelReader> model_reader) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  const bool normalization_changed = normalization != normalization_active_;
  const bool noise_suppression_changed = noise_suppression_model != noise_suppression_model_;
  if (!normalization_changed && !noise_suppression_changed) {
    return;
  }

  if (normalization_changed) {
    normalization_active_ = normalization;
    if (normalization_active_) {
      rebuild_audio_processor_locked();
    } else {
      audio_processor_.reset();
    }
  }
  if (noise_suppression_changed) {
    rebuild_denoiser_locked(noise_suppression_model, std::move(model_reader));
  }

  std::lock_guard<std::mutex> io_lock(io_mutex_);
  clear_ring_buffers_locked();
  reset_congestion_locked();
}

void Processor::process(const float* input, float* output, std::size_t num_frames) {
  if (num_frames == 0U || input == nullptr || output == nullptr) {
    return;
  }

  std::size_t pushed = 0U;
  std::size_t popped = 0U;
  {
    std::lock_guard<std::mutex> lock(io_mutex_);
    pushed = input_ring_buffer_.push(input, num_frames);
    const double current_time_seconds = now_seconds();
    if (should_render_output_locked(current_time_seconds, output_ring_buffer_.available_to_read())) {
      popped = output_ring_buffer_.pop(output, num_frames);
      if (auto event = handle_output_underrun_locked(current_time_seconds, popped, num_frames)) {
        enqueue_low_performance_event_locked(*event);
      }
    }
  }
  if (pushed > 0U) {
    worker_condition_.notify_one();
  }

  if (popped < num_frames) {
    std::memset(output + popped, 0, (num_frames - popped) * sizeof(float));
  }
}

TelemetrySnapshot Processor::telemetry_snapshot() const {
  // Only contends for telemetry_mutex_, which is held for microseconds while a histogram bucket is
  // incremented. It never waits for the worker's ProcessChunk, so callers (e.g. a UI thread polling
  // telemetry) cannot stall the audio thread through this path. settings_ is immutable after
  // construction, so telemetry_rounding_milliseconds is read without a lock.
  TelemetrySnapshot snapshot;
  {
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    snapshot.normalization_counters = normalization_telemetry_;
    snapshot.noise_suppression_counters = noise_suppression_telemetry_;
  }
  snapshot.rounding_milliseconds = settings_.telemetry_rounding_milliseconds;
  return snapshot;
}

std::vector<LowPerformanceEventNative> Processor::drain_low_performance_events() {
  std::lock_guard<std::mutex> lock(io_mutex_);
  std::vector<LowPerformanceEventNative> events(pending_low_performance_events_.begin(),
                                                pending_low_performance_events_.end());
  pending_low_performance_events_.clear();
  return events;
}

void Processor::worker_loop() noexcept {
  try {
    while (true) {
      {
        std::unique_lock<std::mutex> lock(state_mutex_);
        worker_condition_.wait(lock, [this] {
          return stopping_ || completed_reset_generation_ != reset_generation_ ||
                 input_ring_buffer_.available_to_read() >= worker_block_frames_;
        });
        if (stopping_) {
          return;
        }
        if (completed_reset_generation_ != reset_generation_) {
          {
            // Cheap: flush the ring buffers and congestion state under io_mutex_ so the audio
            // thread only ever blocks on this short critical section.
            std::lock_guard<std::mutex> io_lock(io_mutex_);
            clear_ring_buffers_locked();
            reset_congestion_locked();
          }
          // Expensive: rebuilding the normalization chain and recreating the denoiser context is
          // done under state_mutex_ only (still held via `lock`). The audio thread never takes
          // state_mutex_, so process() is not stalled by this rebuild.
          reset_processors_locked();
          completed_reset_generation_ = reset_generation_;
          lock.unlock();
          worker_condition_.notify_all();
          continue;
        }
      }

      while (input_ring_buffer_.available_to_read() >= worker_block_frames_) {
        std::size_t frames_to_process = 0U;
        {
          std::lock_guard<std::mutex> io_lock(io_mutex_);
          frames_to_process = input_ring_buffer_.pop(worker_input_buffer_.data(), worker_input_buffer_.size());
        }
        if (frames_to_process != worker_block_frames_) {
          break;
        }

        bool normalization_active = false;
        bool denoising_active = false;
        {
          std::lock_guard<std::mutex> lock(state_mutex_);
          normalization_active = normalization_active_;
          denoising_active = denoiser_ != nullptr && denoiser_context_ != nullptr;
        }

        const auto chunk_start = std::chrono::steady_clock::now();

        if (!normalization_active && !denoising_active) {
          std::lock_guard<std::mutex> io_lock(io_mutex_);
          output_ring_buffer_.push(worker_input_buffer_.data(), frames_to_process);
          record_processing_time_locked(elapsed_milliseconds(chunk_start));
          continue;
        }

        processor_input_buffer_.assign(worker_input_buffer_.begin(),
                                       worker_input_buffer_.begin() + static_cast<std::ptrdiff_t>(frames_to_process));
        processor_output_buffer_ = processor_input_buffer_;
        denoiser_output_buffer_ = processor_input_buffer_;

        GigaEdgeSpenchRetCode process_result = GigaEdgeSpenchRetCode::SUCCESS;
        GigaEdgeSpenchRetCode denoise_result = GigaEdgeSpenchRetCode::SUCCESS;
        {
          std::lock_guard<std::mutex> lock(state_mutex_);
          if (settings_.unstable_reverse_processing_order) {
            if (denoiser_ != nullptr && denoiser_context_ != nullptr) {
              denoiser_output_buffer_.clear();
              const auto start = std::chrono::steady_clock::now();
              denoise_result =
                  denoiser_->ProcessChunk(processor_input_buffer_, denoiser_output_buffer_, denoiser_context_);
              record_telemetry(false, elapsed_milliseconds(start));
              processor_output_buffer_ = denoiser_output_buffer_;
            }
            if (denoise_result >= GigaEdgeSpenchRetCode::SUCCESS && normalization_active &&
                audio_processor_ != nullptr) {
              denoiser_output_buffer_ = processor_output_buffer_;
              processor_output_buffer_.clear();
              const auto start = std::chrono::steady_clock::now();
              process_result = audio_processor_->ProcessChunk(denoiser_output_buffer_, processor_output_buffer_);
              record_telemetry(true, elapsed_milliseconds(start));
            }
            denoiser_output_buffer_ = processor_output_buffer_;
          } else {
            if (normalization_active && audio_processor_ != nullptr) {
              processor_output_buffer_.clear();
              const auto start = std::chrono::steady_clock::now();
              process_result = audio_processor_->ProcessChunk(processor_input_buffer_, processor_output_buffer_);
              record_telemetry(true, elapsed_milliseconds(start));
            }
            if (process_result >= GigaEdgeSpenchRetCode::SUCCESS && denoiser_ != nullptr &&
                denoiser_context_ != nullptr) {
              denoiser_output_buffer_.clear();
              const auto start = std::chrono::steady_clock::now();
              denoise_result =
                  denoiser_->ProcessChunk(processor_output_buffer_, denoiser_output_buffer_, denoiser_context_);
              record_telemetry(false, elapsed_milliseconds(start));
            } else {
              denoiser_output_buffer_ = processor_output_buffer_;
            }
          }
        }
        {
          std::lock_guard<std::mutex> io_lock(io_mutex_);
          record_processing_time_locked(elapsed_milliseconds(chunk_start));
        }

        std::size_t frames_produced = frames_to_process;
        if (process_result < GigaEdgeSpenchRetCode::SUCCESS || denoise_result < GigaEdgeSpenchRetCode::SUCCESS) {
          frames_produced = 0U;
        } else {
          frames_produced = std::min(frames_to_process, denoiser_output_buffer_.size());
        }

        for (std::size_t i = 0; i < frames_produced; ++i) {
          worker_output_buffer_[i] = denoiser_output_buffer_[i];
        }
        for (std::size_t i = frames_produced; i < frames_to_process; ++i) {
          worker_output_buffer_[i] = 0.0f;
        }
        {
          std::lock_guard<std::mutex> io_lock(io_mutex_);
          output_ring_buffer_.push(worker_output_buffer_.data(), frames_to_process);
        }
      }
    }
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      stopping_ = true;
      completed_reset_generation_ = reset_generation_;
    }
    worker_condition_.notify_all();
  }
}

void Processor::validate_settings(const ProcessorSettings& settings) {
  if (!(settings.low_performance_window_seconds > 0.0) || !finite(settings.low_performance_window_seconds)) {
    throw std::invalid_argument("lowPerformanceWindowSeconds must be greater than zero");
  }
  if (settings.low_performance_max_refill_times <= 0) {
    throw std::invalid_argument("lowPerformanceMaxRefillTimes must be greater than zero");
  }
  if (settings.low_performance_max_refill_time_factor < 0.0 ||
      !finite(settings.low_performance_max_refill_time_factor)) {
    throw std::invalid_argument("lowPerformanceMaxRefillTimeFactor must be finite and non-negative");
  }
  if (settings.min_latency < 0.0 || !finite(settings.min_latency)) {
    throw std::invalid_argument("minLatency must be finite and non-negative");
  }
  if (settings.latency_increase_step < 0.0 || !finite(settings.latency_increase_step)) {
    throw std::invalid_argument("latencyIncreaseStep must be finite and non-negative");
  }
  if (!(settings.max_processing_chunk_seconds > 0.0) || !finite(settings.max_processing_chunk_seconds)) {
    throw std::invalid_argument("maxProcessingChunkSeconds must be finite and greater than zero");
  }
  if (!(settings.telemetry_rounding_milliseconds > 0.0) || !finite(settings.telemetry_rounding_milliseconds)) {
    throw std::invalid_argument("telemetryRoundingMilliseconds must be greater than zero");
  }
}

std::size_t Processor::ring_capacity_for_sample_rate(std::uint32_t sample_rate_hz, const ProcessorSettings& settings) {
  const double capacity_seconds =
      std::max(max_output_buffer_duration_seconds, processing_chunk_seconds(settings) * 2.0);
  return std::max(default_ring_capacity, static_cast<std::size_t>(std::ceil(sample_rate_hz * capacity_seconds)));
}

}  // namespace gigaedge::spench
