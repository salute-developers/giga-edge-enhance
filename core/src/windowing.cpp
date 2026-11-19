#include "src/windowing.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>

#include "src/logger.h"

namespace gigaedge::enhance {
namespace fe {

template <typename T>
Windowing<T>::Windowing(int64_t n, WindowType window_type, uint32_t hop_size, bool is_periodic)
    : window_length_(n),
      window_type_(window_type),
      coefs_(n, 0.0),
      coefs_pow_2_(n, 0.0),
      hop_size_(hop_size),
      window_periodic_(is_periodic) {
  if (window_length_ <= 1) {
    spdlog::error("Windowing: window_length must be > 1");
    throw std::invalid_argument("Windowing: window_length must be > 1");
  }

  if (window_periodic_) {
    ++window_length_;
    coefs_.push_back(0.0);
    coefs_pow_2_.push_back(0.0);
  }
  std::iota(coefs_.begin(), coefs_.end(), 0.0);
  uint32_t i = 0;
  switch (window_type) {
    case WindowType::hamming:
      for (auto& c : coefs_) {
        c = 0.54 - 0.46 * std::cos(2 * M_PI * c / (window_length_ - 1));
        coefs_pow_2_[i++] = c * c;
      }
      break;
    case WindowType::hanning:
      for (auto& c : coefs_) {
        c = 0.5 * (1.0 - std::cos(2 * M_PI * c / (window_length_ - 1)));
        coefs_pow_2_[i++] = c * c;
      }
      break;
    default:
      spdlog::error("Windowing: unsupported window type");
      throw std::invalid_argument("Windowing: unsupported window type");
  }
}

template <typename T>
std::vector<T> Windowing<T>::apply(const std::vector<T>& input) const {
  auto output(input);
  apply_in_place(output);
  return output;
}

template <typename T>
void Windowing<T>::apply_in_place(std::vector<T>& input) const {
  if ((!window_periodic_ && input.size() % (window_length_) != 0) ||
      (window_periodic_ && input.size() % (window_length_ - 1) != 0)) {
    spdlog::error("Windowing: input size is not divisible by windowLength");
    throw std::invalid_argument("Windowing: input size is not divisible by windowLength");
  }
  uint32_t i = 0;
  for (auto& in : input) {
    in *= coefs_[i++];
    if ((!window_periodic_ && i == window_length_) || (window_periodic_ && i == window_length_ - 1)) {
      i = 0;
    }
  }
}

template <typename T>
void Windowing<T>::apply_frame_in_place(T* frame, size_t frame_size) const {
  const size_t coef_len = window_periodic_ ? (window_length_ - 1) : window_length_;

  if (frame_size != coef_len) {
    spdlog::error("Windowing: frame size ({}) does not match window length ({})", frame_size, coef_len);
    throw std::invalid_argument("Windowing: frame size does not match window length");
  }

  for (size_t i = 0; i < frame_size; ++i) {
    frame[i] *= coefs_[i];
  }
}

template <typename T>
void Windowing<T>::sum_square_in_place(std::vector<T>& input) const {
  T addition;
  uint64_t n = 0;
  uint32_t i = 0;
  for (auto& in : input) {
    if (coefs_[n] > 1e-5) {
      // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/filters.py#L1475
      addition = (hop_size_ < n && n < (input.size() - hop_size_)) ? coefs_pow_2_[(n - hop_size_) % coefs_.size()] : 0;
      in /= coefs_pow_2_[i] + addition;
    }
    n++;
    if (n >= coefs_.size()) {
      n = 0;
    }
    i++;
    if ((!window_periodic_ && i == window_length_) || (window_periodic_ && i == window_length_ - 1)) {
      i = 0;
    }
  }
}

template class Windowing<fft::BufferType>;
}  // namespace fe
}  // namespace gigaedge::enhance
