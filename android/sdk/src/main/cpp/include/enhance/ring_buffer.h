#ifndef GEE_RING_BUFFER_H
#define GEE_RING_BUFFER_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gigaedge::enhance {

class RingBuffer {
 public:
  explicit RingBuffer(std::size_t capacity);

  std::size_t capacity() const { return capacity_; }
  std::size_t available_to_read() const;
  std::size_t available_to_write() const;

  std::size_t push(const float* input, std::size_t count);
  std::size_t pop(float* output, std::size_t count);
  void reset();

 private:
  static std::size_t next_power_of_two(std::size_t value);

  std::vector<float> storage_;
  std::size_t capacity_;
  std::size_t mask_;
  std::atomic<std::uint32_t> head_;
  std::atomic<std::uint32_t> tail_;
};

}  // namespace gigaedge::enhance

#endif
