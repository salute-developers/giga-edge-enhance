#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace util::sound {

// effective implementation of median filtering for binary data
class BinaryMedfilt {
 public:
  explicit BinaryMedfilt(size_t size, float threshold = 0.5, bool init = false) : threshold_{threshold} {
    if (size == 0) {
      throw std::invalid_argument("BinaryMedfilt size must be greater than zero.");
    }
    size_ = (size % 2 == 0) ? (size + 1) : size;
    sig_.resize(size_, init);
    sum_ = size_ * (init ? 1 : 0);
  }

  bool process(float x) {
    const uint8_t v = (x > threshold_);
    sum_ += v;
    sum_ -= sig_[idx_];
    sig_[idx_] = v;
    idx_ = (idx_ + 1) % size_;
    return sum_ > (size_ / 2);
  }

 private:
  float threshold_;
  size_t size_;
  std::vector<uint8_t> sig_;
  size_t idx_{0};
  uint32_t sum_{0};
};

}  // namespace util::sound
