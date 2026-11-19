#include <enhance/packed_model.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "enhance/processor.h"
#include "test_support.h"

void run_ring_buffer_tests();

namespace {

std::shared_ptr<gigaedge::enhance::PackedModelReader> test_model_reader() {
  return std::make_shared<gigaedge::enhance::PackedModelReader>(GEE_TEST_MODEL_PATH);
}

constexpr std::uint32_t test_sample_rate_hz = 48000U;
constexpr std::size_t test_chunk_frames = 768U;
constexpr const char* test_model_name = "quality48";

void enable_full_processing(gigaedge::enhance::Processor& processor) {
  processor.set_features(true, test_model_name, test_model_reader());
}

gigaedge::enhance::ProcessorSettings fast_congestion_settings() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.min_latency = 0.0;
  settings.latency_increase_step = 0.2;
  settings.low_performance_max_refill_times = 2;
  settings.low_performance_max_refill_time_factor = 0.0;
  settings.low_performance_window_seconds = 10.0;
  return settings;
}

void test_async_processor_eventually_produces_output() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  enable_full_processing(processor);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);

  processor.process(input.data(), output.data(), input.size());

  bool produced_non_zero = false;
  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    for (float sample : output) {
      if (std::fabs(sample) > 1.0e-4f) {
        produced_non_zero = true;
        break;
      }
    }
    if (produced_non_zero) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  expect_true(produced_non_zero, "async processor should eventually produce output");
}

void test_first_process_zero_fills_when_worker_has_no_output_yet() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  const std::array<float, 8> input = {0.5f, -0.5f, 0.25f, -0.25f, 1.0f, -1.0f, 0.1f, -0.1f};
  std::array<float, 8> output = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

  processor.process(input.data(), output.data(), input.size());

  for (float sample : output) {
    expect_near(sample, 0.0f, 1.0e-6f, "first async process should zero-fill before worker output exists");
  }
}

void test_reset_clears_pending_output() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  enable_full_processing(processor);
  std::vector<float> input(test_chunk_frames, 1.0f);
  std::vector<float> output(input.size(), 1.0f);

  processor.process(input.data(), output.data(), input.size());
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  processor.reset();
  processor.process(input.data(), output.data(), input.size());

  for (float sample : output) {
    expect_near(sample, 0.0f, 1.0e-6f, "reset should zero missing output");
  }
}

void test_processor_output_is_finite() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  enable_full_processing(processor);
  std::vector<float> input(test_chunk_frames, 0.5f);
  std::vector<float> output(input.size(), 0.0f);

  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    if (std::fabs(output[0]) > 1.0e-4f) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  expect_true(std::fabs(output[0]) > 1.0e-4f, "processor should eventually produce output");
  for (float sample : output) {
    expect_true(std::isfinite(sample), "processor output should stay finite");
  }
}

void test_overflow_backlog_does_not_hang() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  enable_full_processing(processor);
  std::vector<float> input(65536, 0.25f);
  std::vector<float> output(input.size(), 0.0f);

  const auto start = std::chrono::steady_clock::now();
  processor.process(input.data(), output.data(), input.size());
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

  expect_true(elapsed.count() < 50, "backlog process should stay non-blocking");
}

void test_repeated_create_process_destroy_is_stable() {
  const std::array<float, 32> input = {0.2f};

  for (std::int32_t iteration = 0; iteration < 200; ++iteration) {
    gigaedge::enhance::Processor processor(test_sample_rate_hz);
    enable_full_processing(processor);
    std::array<float, 32> output = {};
    processor.process(input.data(), output.data(), input.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  expect_true(true, "repeated create/process/destroy completed");
}

void test_default_processor_passes_input_through_async_path() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  std::vector<float> input(test_chunk_frames, 0.0f);
  std::vector<float> output(input.size(), 0.0f);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<float>(i) / static_cast<float>(input.size());
  }

  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    bool matches = true;
    for (std::size_t i = 0; i < input.size(); ++i) {
      if (std::fabs(input[i] - output[i]) > 1.0e-6f) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  expect_true(false, "default all-off processor should pass input through asynchronously");
}

void test_disabled_features_pass_input_through_async_path() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  processor.set_features(false, "", nullptr);
  std::vector<float> input(test_chunk_frames, 0.0f);
  std::vector<float> output(input.size(), 0.0f);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<float>(i) / static_cast<float>(input.size());
  }

  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    bool matches = true;
    for (std::size_t i = 0; i < input.size(); ++i) {
      if (std::fabs(input[i] - output[i]) > 1.0e-6f) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  expect_true(false, "disabled features should pass input through asynchronously");
}

void test_feature_switching_keeps_processor_usable() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);

  processor.set_features(true, "", nullptr);
  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    if (output[0] != 0.0f) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  for (float sample : output) {
    expect_true(std::isfinite(sample), "normalization-only output should stay finite");
  }

  processor.set_features(true, test_model_name, test_model_reader());
  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    if (std::fabs(output[0]) > 1.0e-4f) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  for (float sample : output) {
    expect_true(std::isfinite(sample), "denoiser output should stay finite after feature switch");
  }

  processor.set_features(false, "", nullptr);
  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    if (output == input) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  expect_true(false, "processor should pass through after disabling all features");
}

void test_settings_driven_processor_construction() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.telemetry_rounding_milliseconds = 0.5;
  settings.use_vad_normalization = true;
  gigaedge::enhance::Processor processor(test_sample_rate_hz, settings);
  processor.set_features(true, "", nullptr);
  expect_true(true, "settings-driven processor should construct");
}

void test_worker_waits_for_configured_full_chunk() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.min_latency = 0.05;
  settings.max_processing_chunk_seconds = 0.064;
  gigaedge::enhance::Processor processor(test_sample_rate_hz, settings);
  processor.set_features(true, "", nullptr);

  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);
  processor.process(input.data(), output.data(), input.size());
  processor.process(input.data(), output.data(), input.size());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  expect_true(processor.telemetry_snapshot().normalization_counters.empty(),
              "worker must not process before the configured 48ms chunk is complete");

  processor.process(input.data(), output.data(), input.size());
  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    std::size_t invocation_count = 0U;
    for (const auto& entry : processor.telemetry_snapshot().normalization_counters) {
      invocation_count += static_cast<std::size_t>(entry.second);
    }
    if (invocation_count == 1U) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  expect_true(false, "worker must process exactly one configured 48ms chunk");
}

void test_invalid_processing_chunk_setting_is_rejected() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.max_processing_chunk_seconds = std::numeric_limits<double>::quiet_NaN();
  try {
    gigaedge::enhance::Processor processor(test_sample_rate_hz, settings);
    expect_true(false, "invalid processing chunk setting must be rejected");
  } catch (const std::invalid_argument&) {
    // expected
  }
}

void test_telemetry_counters_increment_only_for_enabled_stages() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  processor.set_features(true, "", nullptr);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);
  for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  const auto telemetry = processor.telemetry_snapshot();
  expect_true(!telemetry.normalization_counters.empty(), "normalization telemetry should increment");
  expect_true(telemetry.noise_suppression_counters.empty(), "noise telemetry should stay empty when disabled");
}

void test_telemetry_bucket_saturates_for_tiny_rounding_step() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.telemetry_rounding_milliseconds = std::numeric_limits<double>::denorm_min();
  gigaedge::enhance::Processor processor(test_sample_rate_hz, settings);
  processor.set_features(true, "", nullptr);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);

  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    const auto telemetry = processor.telemetry_snapshot();
    if (!telemetry.normalization_counters.empty()) {
      expect_true(telemetry.normalization_counters.begin()->first == std::numeric_limits<std::int32_t>::max(),
                  "oversized telemetry bucket should saturate");
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  expect_true(false, "timed out waiting for telemetry");
}

void test_reset_does_not_clear_telemetry() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  processor.set_features(true, "", nullptr);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);
  for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const auto before = processor.telemetry_snapshot();
  processor.reset();
  const auto after = processor.telemetry_snapshot();
  expect_true(after.normalization_counters == before.normalization_counters, "reset should not clear telemetry");
}

void test_reverse_order_produces_finite_output() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.unstable_reverse_processing_order = true;
  gigaedge::enhance::Processor processor(test_sample_rate_hz, settings);
  enable_full_processing(processor);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);
  for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
    processor.process(input.data(), output.data(), input.size());
    if (std::fabs(output[0]) > 1.0e-4f) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  for (float sample : output) {
    expect_true(std::isfinite(sample), "reverse-order output should stay finite");
  }
}

void test_congestion_priming_returns_silence_until_threshold() {
  gigaedge::enhance::ProcessorSettings settings;
  settings.min_latency = 0.064;
  gigaedge::enhance::Processor processor(test_sample_rate_hz, settings);
  std::vector<float> input(test_chunk_frames, 0.5f);
  std::vector<float> output(input.size(), 1.0f);
  processor.process(input.data(), output.data(), input.size());
  for (float sample : output) {
    expect_near(sample, 0.0f, 1.0e-6f, "unprimed output should be silent");
  }
}

void test_repeated_underruns_create_low_performance_event_and_cap_latency() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz, fast_congestion_settings());
  std::vector<float> input(test_chunk_frames, 0.5f);
  std::vector<float> large_input(test_chunk_frames * 8U, 0.5f);
  std::vector<float> output(test_chunk_frames * 8U, 0.0f);

  for (std::int32_t underrun = 0; underrun < 2; ++underrun) {
    processor.process(input.data(), output.data(), input.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    processor.process(large_input.data(), output.data(), output.size());
  }

  const auto events = processor.drain_low_performance_events();
  expect_true(!events.empty(), "repeated underruns should create low-performance event");
  expect_true(events.back().current_latency_seconds <= 0.256 + 1.0e-9, "latency should cap at max output duration");
}

// Exercises the dedicated telemetry lock: a background thread streams audio (worker records telemetry
// under telemetry_mutex_) while the main thread hammers telemetry_snapshot(). This must never deadlock
// and every snapshot must be internally consistent and monotonically non-decreasing.
void test_concurrent_process_and_telemetry_do_not_deadlock() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  processor.set_features(true, "", nullptr);
  std::vector<float> input(test_chunk_frames, 0.25f);
  std::vector<float> output(input.size(), 0.0f);

  std::atomic<bool> stop{false};
  std::thread producer([&] {
    while (!stop.load(std::memory_order_acquire)) {
      processor.process(input.data(), output.data(), input.size());
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  std::size_t last_total = 0U;
  for (std::int32_t i = 0; i < 500 && last_total == 0U; ++i) {
    const auto snapshot = processor.telemetry_snapshot();
    std::size_t total = 0U;
    for (const auto& entry : snapshot.normalization_counters) {
      expect_true(entry.second >= 0, "telemetry counts must be non-negative");
      total += static_cast<std::size_t>(entry.second);
    }
    expect_true(total >= last_total, "cumulative telemetry must not go backwards");
    last_total = total;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  stop.store(true, std::memory_order_release);
  producer.join();
  expect_true(last_total > 0U, "telemetry should have accumulated during concurrent processing");
}

// Exercises the reset path with the processor-rebuild moved out of io_mutex_: a background thread keeps
// streaming audio while the main thread repeatedly resets and flips features. This must terminate (no
// deadlock) and never produce non-finite output.
void test_concurrent_reset_and_features_stay_finite() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz);
  const std::vector<float> input(test_chunk_frames, 0.25f);
  const std::shared_ptr<gigaedge::enhance::PackedModelReader> reader = test_model_reader();

  std::atomic<bool> stop{false};
  std::atomic<bool> saw_nonfinite{false};
  std::thread producer([&] {
    std::vector<float> output(input.size(), 0.0f);
    while (!stop.load(std::memory_order_acquire)) {
      processor.process(input.data(), output.data(), input.size());
      for (float sample : output) {
        if (!std::isfinite(sample)) {
          saw_nonfinite.store(true, std::memory_order_release);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  for (std::int32_t i = 0; i < 20; ++i) {
    const bool denoise = i % 3 == 0;
    processor.set_features(i % 2 == 0, denoise ? test_model_name : "", denoise ? reader : nullptr);
    processor.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  stop.store(true, std::memory_order_release);
  producer.join();
  expect_true(!saw_nonfinite.load(std::memory_order_acquire), "concurrent reset/feature churn must keep output finite");
}

void test_reset_clears_pending_congestion_state() {
  gigaedge::enhance::Processor processor(test_sample_rate_hz, fast_congestion_settings());
  std::vector<float> input(test_chunk_frames, 0.5f);
  std::vector<float> large_input(test_chunk_frames * 8U, 0.5f);
  std::vector<float> output(test_chunk_frames * 8U, 0.0f);

  processor.process(input.data(), output.data(), input.size());
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  processor.process(large_input.data(), output.data(), output.size());
  processor.reset();
  processor.process(input.data(), output.data(), input.size());

  const auto events = processor.drain_low_performance_events();
  expect_true(events.empty(), "reset should clear pending congestion state");
}

}  // namespace

int main() {  // gigaedge-enhance-allow-int
  run_ring_buffer_tests();
  test_async_processor_eventually_produces_output();
  test_first_process_zero_fills_when_worker_has_no_output_yet();
  test_reset_clears_pending_output();
  test_processor_output_is_finite();
  test_overflow_backlog_does_not_hang();
  test_repeated_create_process_destroy_is_stable();
  test_default_processor_passes_input_through_async_path();
  test_disabled_features_pass_input_through_async_path();
  test_feature_switching_keeps_processor_usable();
  test_settings_driven_processor_construction();
  test_worker_waits_for_configured_full_chunk();
  test_invalid_processing_chunk_setting_is_rejected();
  test_telemetry_counters_increment_only_for_enabled_stages();
  test_telemetry_bucket_saturates_for_tiny_rounding_step();
  test_reset_does_not_clear_telemetry();
  test_reverse_order_produces_finite_output();
  test_congestion_priming_returns_silence_until_threshold();
  test_repeated_underruns_create_low_performance_event_and_cap_latency();
  test_reset_clears_pending_congestion_state();
  test_concurrent_process_and_telemetry_do_not_deadlock();
  test_concurrent_reset_and_features_stay_finite();
  std::cout << "processor_async_test passed" << std::endl;
  return 0;
}
