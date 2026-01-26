#pragma once

#include <cstdint>
#include <vector>

namespace util::sound {

// effective implementation of median filtering for binary data
class BinaryMedfilt {
 public:
  explicit BinaryMedfilt(size_t size, float threshold = 0.5, bool init = false) : m_threshold{threshold} {
    m_size = (size % 2 == 0) ? (size + 1) : size;
    m_sig.resize(m_size, init);
    m_sum = m_size * (init ? 1 : 0);
  }

  bool process(float x) {
    const uint8_t v = (x > m_threshold);
    m_sum += v;
    m_sum -= m_sig[m_idx];
    m_sig[m_idx] = v;
    m_idx = (m_idx + 1) % m_size;
    return m_sum > (m_size / 2);
  }

 private:
  float m_threshold;
  size_t m_size;
  std::vector<uint8_t> m_sig;
  size_t m_idx{0};
  uint32_t m_sum{0};
};

}  // namespace util::sound
