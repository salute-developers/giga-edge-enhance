#include "spench/ring_buffer.h"

namespace gigaedge::spench {

RingBuffer::RingBuffer(std::size_t capacity)
    : storage_(next_power_of_two(std::max<std::size_t>(capacity, 2U)), 0.0f),
      capacity_(storage_.size()),
      mask_(capacity_ - 1U),
      head_(0U),
      tail_(0U) {}

std::size_t RingBuffer::available_to_read() const {
  const std::uint32_t head = head_.load(std::memory_order_acquire);
  const std::uint32_t tail = tail_.load(std::memory_order_acquire);
  return static_cast<std::size_t>(head - tail);
}

std::size_t RingBuffer::available_to_write() const { return capacity_ - available_to_read(); }

std::size_t RingBuffer::push(const float* input, std::size_t count) {
  if (count == 0U || input == nullptr) {
    return 0U;
  }

  const std::uint32_t head = head_.load(std::memory_order_relaxed);
  const std::uint32_t tail = tail_.load(std::memory_order_acquire);
  const std::size_t free_space = capacity_ - static_cast<std::size_t>(head - tail);
  const std::size_t write_count = std::min(count, free_space);
  if (write_count == 0U) {
    return 0U;
  }

  const std::size_t start_index = static_cast<std::size_t>(head) & mask_;
  const std::size_t first_chunk = std::min(write_count, capacity_ - start_index);
  std::memcpy(storage_.data() + start_index, input, first_chunk * sizeof(float));
  if (write_count > first_chunk) {
    std::memcpy(storage_.data(), input + first_chunk, (write_count - first_chunk) * sizeof(float));
  }

  head_.store(head + static_cast<std::uint32_t>(write_count), std::memory_order_release);
  return write_count;
}

std::size_t RingBuffer::pop(float* output, std::size_t count) {
  if (count == 0U || output == nullptr) {
    return 0U;
  }

  const std::uint32_t tail = tail_.load(std::memory_order_relaxed);
  const std::uint32_t head = head_.load(std::memory_order_acquire);
  const std::size_t available = static_cast<std::size_t>(head - tail);
  const std::size_t read_count = std::min(count, available);
  if (read_count == 0U) {
    return 0U;
  }

  const std::size_t start_index = static_cast<std::size_t>(tail) & mask_;
  const std::size_t first_chunk = std::min(read_count, capacity_ - start_index);
  std::memcpy(output, storage_.data() + start_index, first_chunk * sizeof(float));
  if (read_count > first_chunk) {
    std::memcpy(output + first_chunk, storage_.data(), (read_count - first_chunk) * sizeof(float));
  }

  tail_.store(tail + static_cast<std::uint32_t>(read_count), std::memory_order_release);
  return read_count;
}

void RingBuffer::reset() { tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release); }

std::size_t RingBuffer::next_power_of_two(std::size_t value) {
  if (value <= 2U) {
    return 2U;
  }

  value -= 1U;
  value |= value >> 1U;
  value |= value >> 2U;
  value |= value >> 4U;
  value |= value >> 8U;
  value |= value >> 16U;
  if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
    value |= value >> 32U;
  }
  return value + 1U;
}

}  // namespace gigaedge::spench
