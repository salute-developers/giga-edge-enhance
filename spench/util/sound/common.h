#pragma once

#include <dsplib/array.h>
#include <dsplib/math.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace util::sound {

class CycleBuffer {
 public:
  explicit CycleBuffer(size_t size, float init = 0) : size_{size} {
    if (size_ == 0) {
      throw std::invalid_argument("CycleBuffer size must be greater than zero.");
    }
    buf_.resize(size_, init);
  }

  void push(float value) noexcept {
    buf_[pos_] = value;
    pos_ = (pos_ + 1) % size_;
  }

  const std::vector<float>& data() const noexcept { return buf_; }

  size_t size() const noexcept { return buf_.size(); }

 private:
  size_t size_;
  std::vector<float> buf_;
  size_t pos_{0};
};

/**
 * @class FIFOBuffer
 * @brief A thread-unsafe FIFO buffer for trivially copyable data types
 * @tparam T Element type (must be trivially copyable and not bool)
 * @warning Not thread-safe for concurrent access
 * @warning T must be trivially copyable (POD-like)
 */
template <typename T>
class FIFOBuffer {
 public:
  static_assert(!std::is_same_v<T, bool>, "boolean buffer is not supported");
  static_assert(std::is_trivially_copyable<T>(), "type must be trivially copyable");

  /**
   * @brief Default constructor
   * Creates an empty buffer with default capacity
   */
  FIFOBuffer() = default;

  /**
   * @brief Write data to buffer
   * @param data Contiguous span of elements to append
   * Appends elements to the end of buffer. No-op for empty spans.
   */
  void write(const std::vector<T>& data) {
    if (data.empty()) {
      return;
    }
    buf_.insert(buf_.end(), data.begin(), data.end());
  }

  /**
   * @brief Write single element (copy)
   * @param value Element to append
   */
  void write(const T& value) { buf_.push_back(value); }

  /**
   * @brief Read data from buffer
   * @param out Output span for data
   * @return Number of elements actually read
   */
  uint32_t read(std::vector<T>& out) noexcept {
    const size_t n = dsplib::min(out.size(), buf_.size());
    if (n == 0) {
      return 0;
    }

    std::memcpy(out.data(), buf_.data(), n * sizeof(T));
    if (n == buf_.size()) {
      buf_.clear();
    } else {
      std::memmove(buf_.data(), buf_.data() + n, (buf_.size() - n) * sizeof(T));
      buf_.resize(buf_.size() - n);
    }
    return n;
  }

  /**
   * @brief Read data to new vector
   * @param n Number of elements to read
   * @return Vector with read elements
   */
  std::vector<T> read(uint32_t n) {
    n = dsplib::min(n, buf_.size());
    std::vector<T> out(n);
    this->read(out);
    return out;
  }

  /**
   * @brief Get current element count
   * @return Number of elements in buffer
   */
  [[nodiscard]] size_t size() const noexcept { return buf_.size(); }

  /**
   * @brief Check if buffer empty
   * @return true if size() == 0
   */
  [[nodiscard]] bool empty() const noexcept { return buf_.empty(); }

  /**
   * @brief Clear buffer contents
   * Removes all elements. Preserves allocated capacity.
   */
  void reset() noexcept { buf_.clear(); }

  /**
   * @brief Get read-only view of buffer
   * @return Span covering all elements (in FIFO order)
   * @warning Invalidated by write operations
   */
  [[nodiscard]] const std::vector<T>& view() const noexcept { return buf_; }

 private:
  std::vector<T> buf_;
};

// quantiles of data set
float quantile(const dsplib::arr_real& x, float q, bool sorted = false);

}  // namespace util::sound
