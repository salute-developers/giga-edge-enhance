#include "spench/src/windowing.h"

#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include "spench/src/spench_logger.h"

namespace spench {
namespace fe {

template <typename T>
Windowing<T>::Windowing(int64_t n, WindowType window_type, uint32_t hop_size, bool is_periodic)
    : m_window_length(n),
      m_window_type(window_type),
      m_coefs(n, 0.0),
      m_coefs_pow_2(n, 0.0),
      m_hop_size(hop_size),
      m_window_periodic(is_periodic) {
  if (m_window_length <= 1) {
    spdlog::error("Windowing: window_length must be > 1");
    throw std::invalid_argument("Windowing: window_length must be > 1");
  }

  if (m_window_periodic) {
    ++m_window_length;
    m_coefs.push_back(0.0);
    m_coefs_pow_2.push_back(0.0);
  }
  std::iota(m_coefs.begin(), m_coefs.end(), 0.0);
  uint32_t i = 0;
  switch (window_type) {
    case WindowType::HAMMING:
      for (auto& c : m_coefs) {
        c = 0.54 - 0.46 * std::cos(2 * M_PI * c / (m_window_length - 1));
        m_coefs_pow_2[i++] = c * c;
      }
      break;
    case WindowType::HANNING:
      for (auto& c : m_coefs) {
        c = 0.5 * (1.0 - std::cos(2 * M_PI * c / (m_window_length - 1)));
        m_coefs_pow_2[i++] = c * c;
      }
      break;
    default:
      spdlog::error("Windowing: unsupported window type");
      throw std::invalid_argument("Windowing: unsupported window type");
  }
}

template <typename T>
std::vector<T> Windowing<T>::Apply(const std::vector<T>& input) const {
  auto output(input);
  ApplyInPlace(output);
  return output;
}

template <typename T>
void Windowing<T>::ApplyInPlace(std::vector<T>& input) const {
  if ((!m_window_periodic && input.size() % (m_window_length) != 0) ||
       (m_window_periodic && input.size() % (m_window_length - 1) != 0)) {
    spdlog::error("Windowing: input size is not divisible by windowLength");
    throw std::invalid_argument(
        "Windowing: input size is not divisible by windowLength");
  }
  uint32_t i = 0;
  for (auto& in : input) {
      in *= m_coefs[i++];
    if ((!m_window_periodic && i == m_window_length) ||
        (m_window_periodic && i == m_window_length - 1)) {
      i = 0;
    }
  }
}

template <typename T>
void Windowing<T>::ApplyFrameInPlace(T* frame, size_t frame_size) const {
  const size_t coef_len = m_window_periodic ? (m_window_length - 1) : m_window_length;

  if (frame_size != coef_len) {
    spdlog::error("Windowing: frame size ({}) does not match window length ({})",
                  frame_size, coef_len);
    throw std::invalid_argument("Windowing: frame size does not match window length");
  }

  for (size_t i = 0; i < frame_size; ++i) {
    frame[i] *= m_coefs[i];
  }
}

template <typename T>
void Windowing<T>::sumSquareInPlace(std::vector<T>& input) const {
  T addition;
  uint64_t n = 0;
  uint32_t i = 0;
  for (auto& in : input) {
    if (m_coefs[n] > 1e-5) {
      // https://github.com/librosa/librosa/blob/a668dffeada59aa6dc4e05bbbec39ad42ac77bed/librosa/filters.py#L1475
      addition = (m_hop_size < n && n < (input.size() - m_hop_size)) ?
                  m_coefs_pow_2[(n - m_hop_size) % m_coefs.size()] : 0;
      in /= m_coefs_pow_2[i] + addition;
    }
    n++;
    if (n >= m_coefs.size()) {
      n = 0;
    }
    i++;
    if ((!m_window_periodic && i == m_window_length) ||
        (m_window_periodic && i == m_window_length - 1)) {
      i = 0;
    }
  }
}

template class Windowing<fft::buffer_type>;
}  // namespace fe
}  // namespace spench
