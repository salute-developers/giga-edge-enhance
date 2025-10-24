#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <memory>

namespace spench {
class NetworkConfig;
namespace fe {
namespace fft {
using buffer_type = float;
}  // namespace fft

enum class WindowType {
  HAMMING = 0,
  HANNING = 1,
};

class FeatureParams {
 public:
  // frequency (Hz) of speech signal recording
  uint32_t sampling_freq = 16000;

  // frame size in milliseconds
  uint32_t frame_size_ms = 32;

  // frame step size in milliseconds
  uint32_t frame_stride_ms = 16;

  // number of frames processed in one iteration of iSTFTImpl loop
  // while inference we need at least 2 frames for decoding of 1 frame
  // framework works only with 2 frames in one iteration!
  uint32_t frames_num_for_istft = 2;

  // analysis window function handle for framing (hamming by default)
  WindowType window_type = WindowType::HANNING;

  FeatureParams() = default;
  explicit FeatureParams(std::shared_ptr<spench::NetworkConfig> config) {
    ReadFeatureParamsNode(config);
  }

  void ReadFeatureParamsNode(std::shared_ptr<spench::NetworkConfig> config);

  inline uint64_t NumFrameSizeSamples() const {
    return static_cast<uint64_t>(round(1e-3 * frame_size_ms * sampling_freq));
  }

  inline uint64_t NumFrameStrideSamples() const {
    return static_cast<uint64_t>(round(1e-3 * frame_stride_ms * sampling_freq));
  }

  inline uint64_t Nfft() const {
    int64_t nsamples = NumFrameSizeSamples();
    return (nsamples > 0) ? static_cast<uint64_t>(nsamples) : static_cast<int64_t>(0);
  }

  inline uint64_t FilterFreqResponseLen() const {
    return (Nfft() >> 1) + 1;
  }

  inline uint64_t PowSpecFeatSz() const {
    return FilterFreqResponseLen();
  }

  inline uint64_t NumFrames(uint64_t in_size) const {
    auto frame_size = NumFrameSizeSamples();
    auto frame_stride = NumFrameStrideSamples();
    if (frame_stride <= 0 || in_size < frame_size) {
      return 0;
    }
    return uint64_t{1} + static_cast<uint64_t>(floor(static_cast<float>(in_size - frame_size)
    / static_cast<float>(frame_stride)));
  }

 private:
  WindowType StringToWindowTypeEnum(const std::string& window);
};
}  // namespace fe
}  // namespace spench
