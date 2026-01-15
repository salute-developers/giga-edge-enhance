#pragma once

#include <memory>
#include <vector>

namespace util {
namespace sound {

class ResamplerImpl;

class Resampler {
 public:
  explicit Resampler(size_t in_rate, size_t out_rate, size_t channels);

  std::vector<float> process(const std::vector<float>& x);

  std::vector<float> flush();

  bool set_io_ratio(double io_ratio);

  float accumulated_delay_ms() const noexcept;

 private:
  std::shared_ptr<ResamplerImpl> m_d;
};

}  // namespace sound
}  // namespace util
