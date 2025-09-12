#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <yaml-cpp/yaml.h>

namespace spench {
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
  explicit FeatureParams(const YAML::Node& yaml_config) {
    ReadFeatureParamsNode(yaml_config);
  }

  void ReadFeatureParamsNode(const YAML::Node& yaml_config) {
    if (yaml_config["sampling_freq"] && !yaml_config["sampling_freq"].IsNull()) {
      if (yaml_config["sampling_freq"].Type() != YAML::NodeType::Scalar) {
        throw("Config reading error. Sampling frequency is not a scalar!");
      }
      sampling_freq = yaml_config["sampling_freq"].as<uint32_t>();
    }

    if (yaml_config["frame_size_ms"] && !yaml_config["frame_size_ms"].IsNull()) {
      if (yaml_config["frame_size_ms"].Type() != YAML::NodeType::Scalar) {
        throw("Config reading error. Frame size is not a scalar!");
      }
      frame_size_ms = yaml_config["frame_size_ms"].as<uint32_t>();
    }

    if (yaml_config["frame_stride_ms"] && !yaml_config["frame_stride_ms"].IsNull()) {
      if (yaml_config["frame_stride_ms"].Type() != YAML::NodeType::Scalar) {
        throw("Config reading error. Window hop length is not a scalar!");
      }
      frame_stride_ms = yaml_config["frame_stride_ms"].as<uint32_t>();
    }

    if (yaml_config["window_type"] && !yaml_config["window_type"].IsNull()) {
      if (yaml_config["window_type"].Type() != YAML::NodeType::Scalar) {
        throw("Config reading error. Window type is not a single string!");
      }
      std::string window = yaml_config["window_type"].as<std::string>();
      window_type = StringToWindowTypeEnum(window);
    }
  }

  uint64_t NumFrameSizeSamples() const {
    return static_cast<uint64_t>(round(1e-3 * frame_size_ms * sampling_freq));
  }

  uint64_t NumFrameStrideSamples() const {
    return static_cast<uint64_t>(round(1e-3 * frame_stride_ms * sampling_freq));
  }

  uint64_t Nfft() const {
    int64_t nsamples = NumFrameSizeSamples();
    return (nsamples > 0) ? static_cast<uint64_t>(nsamples) : static_cast<int64_t>(0);
  }

  uint64_t FilterFreqResponseLen() const {
    return (Nfft() >> 1) + 1;
  }

  uint64_t PowSpecFeatSz() const {
    return FilterFreqResponseLen();
  }

  uint64_t NumFrames(uint64_t in_size) const {
    auto frame_size = NumFrameSizeSamples();
    auto frame_stride = NumFrameStrideSamples();
    if (frame_stride <= 0 || in_size < frame_size) {
      return 0;
    }
    return uint64_t{1} + static_cast<uint64_t>(floor(static_cast<float>(in_size - frame_size)
    / static_cast<float>(frame_stride)));
  }

 private:
  WindowType StringToWindowTypeEnum(const std::string& window) {
    if (window == "hanning") {
      return WindowType::HANNING;
    }
    if (window == "hamming") {
      return WindowType::HAMMING;
    }
    throw std::invalid_argument("Window type must be in [\"hanning\", \"hamming\"]");
  }
};
}  // namespace fe
}  // namespace spench
