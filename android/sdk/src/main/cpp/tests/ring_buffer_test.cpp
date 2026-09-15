#include "gigadenoiser/ring_buffer.h"

#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "test_support.h"

namespace {

void test_push_pop_round_trip() {
  gigaedge::denoiser::RingBuffer ring_buffer(8);
  const std::array<float, 4> input = {0.1f, 0.2f, 0.3f, 0.4f};
  std::array<float, 4> output = {};

  expect_true(ring_buffer.push(input.data(), input.size()) == input.size(), "ring buffer should push full input");
  expect_true(ring_buffer.pop(output.data(), output.size()) == output.size(), "ring buffer should pop full output");
  for (std::size_t i = 0; i < input.size(); ++i) {
    expect_near(output[i], input[i], 1.0e-6f, "ring buffer round trip mismatch");
  }
}

void test_wraparound() {
  gigaedge::denoiser::RingBuffer ring_buffer(4);
  const std::array<float, 4> first = {1.0f, 2.0f, 3.0f, 4.0f};
  const std::array<float, 2> second = {5.0f, 6.0f};
  std::array<float, 2> scratch = {};
  std::array<float, 4> output = {};

  expect_true(ring_buffer.push(first.data(), first.size()) == first.size(), "wraparound first push");
  expect_true(ring_buffer.pop(scratch.data(), scratch.size()) == scratch.size(), "wraparound scratch pop");
  expect_true(ring_buffer.push(second.data(), second.size()) == second.size(), "wraparound second push");
  expect_true(ring_buffer.pop(output.data(), output.size()) == output.size(), "wraparound final pop");

  expect_near(output[0], 3.0f, 1.0e-6f, "wraparound value 0");
  expect_near(output[1], 4.0f, 1.0e-6f, "wraparound value 1");
  expect_near(output[2], 5.0f, 1.0e-6f, "wraparound value 2");
  expect_near(output[3], 6.0f, 1.0e-6f, "wraparound value 3");
}

void test_capacity_limit() {
  gigaedge::denoiser::RingBuffer ring_buffer(4);
  const std::array<float, 6> input = {0, 1, 2, 3, 4, 5};
  std::array<float, 4> output = {};

  expect_true(ring_buffer.push(input.data(), input.size()) == 4U, "ring buffer should cap writes at capacity");
  expect_true(ring_buffer.available_to_write() == 0U, "ring buffer should be full");
  expect_true(ring_buffer.pop(output.data(), output.size()) == 4U, "ring buffer should pop capacity");
  for (std::size_t i = 0; i < output.size(); ++i) {
    expect_near(output[i], input[i], 1.0e-6f, "capacity limit value mismatch");
  }
}

void test_single_producer_single_consumer_stress() {
  constexpr std::size_t sample_count = 20000;
  gigaedge::denoiser::RingBuffer ring_buffer(1024);
  std::vector<float> consumed;
  consumed.reserve(sample_count);
  std::atomic<bool> producer_done{false};

  std::thread producer([&] {
    std::size_t next_value = 0;
    while (next_value < sample_count) {
      const float sample = static_cast<float>(next_value);
      if (ring_buffer.push(&sample, 1) == 1U) {
        ++next_value;
      } else {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&] {
    float sample = 0.0f;
    while (!producer_done.load(std::memory_order_acquire) || ring_buffer.available_to_read() > 0U) {
      if (ring_buffer.pop(&sample, 1) == 1U) {
        consumed.push_back(sample);
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  expect_true(consumed.size() == sample_count, "spsc stress should consume every produced sample");
  for (std::size_t i = 0; i < consumed.size(); ++i) {
    expect_near(consumed[i], static_cast<float>(i), 1.0e-6f, "spsc stress should preserve ordering");
  }
}

}  // namespace

void run_ring_buffer_tests() {
  test_push_pop_round_trip();
  test_wraparound();
  test_capacity_limit();
  test_single_producer_single_consumer_stress();
  std::cout << "ring_buffer_test passed" << std::endl;
}
