#pragma once

#include <cmath>
#include <cstdint>

namespace util {
namespace sound {

struct SoundInfo {
  uint32_t processing_frame_ms;
  uint32_t sample_rate;
  uint16_t channels = 1;
  bool operator==(SoundInfo const& l) const;
  // friend std::ostream& operator<<(std::ostream& os, const SoundInfo& info);
  uint64_t FrameSizeSamples() const {
    return static_cast<uint64_t>(round(1e-3 * processing_frame_ms * sample_rate));
  }
};

}  // namespace sound
}  // namespace util
