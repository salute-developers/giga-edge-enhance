#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "enhance/audio_processor.h"
#include "enhance/echo_canceller.h"

namespace {
std::vector<float> read_audio(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() < 0 || static_cast<size_t>(file.tellg()) % sizeof(float) != 0) {
    throw std::runtime_error("Invalid reference input");
  }
  std::vector<float> samples(static_cast<size_t>(file.tellg()) / sizeof(float));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(samples.data()), static_cast<std::streamsize>(samples.size() * sizeof(float)));
  return samples;
}
}  // namespace

int main(int argc, char** argv) {  // gigaedge-enhance-allow-int: standard entry point
  try {
    if (argc != 6) {
      throw std::invalid_argument("Expected mode, config, input.f32, reference.f32, output.f32");
    }
    const std::string mode(argv[1]);
    const auto yaml = YAML::LoadFile(argv[2]);
    const auto input = read_audio(argv[3]);
    std::vector<float> reference;
    std::unique_ptr<giga_edge::enhance::EchoCanceller> aec;
    std::unique_ptr<giga_edge::enhance::AudioProcessor> dsp;
    uint32_t frame = 0;
    if (mode == "aec") {
      reference = read_audio(argv[4]);
      if (reference.size() != input.size()) {
        throw std::invalid_argument("Reference lengths differ");
      }
      aec = std::make_unique<giga_edge::enhance::EchoCanceller>(YAML::Dump(yaml));
      frame = aec->GetFrameLength();
    } else {
      const uint32_t rate = yaml["sample_rate"].as<uint32_t>();
      const auto p = yaml["parameters"];
      frame = rate / 100;
      dsp = std::make_unique<giga_edge::enhance::AudioProcessor>();
      if (mode == "agc") {
        dsp->AddAGC(rate, p["target_power_level"].as<float>(), p["max_gain_db"].as<float>(), p["rise_step"].as<float>(),
                    p["fall_step"].as<float>());
      } else if (mode == "range_control") {
        dsp->AddRC(rate, p["target_level_db"].as<float>(), p["max_gain_db"].as<float>(), p["attack_time"].as<float>(),
                   p["release_time"].as<float>(), p["hold_time"].as<float>());
      } else if (mode == "loudness_control") {
        dsp->AddLC(rate, p["target_level_db"].as<float>(), p["bypass_time"].as<float>(), p["max_gain_db"].as<float>(),
                   p["use_vad"].as<bool>(), p["post_processing"].as<bool>());
      } else if (mode == "noise_gate") {
        dsp->AddNG(rate, p["threshold_db"].as<float>(), p["attack_time"].as<float>(), p["release_time"].as<float>(),
                   p["hold_time"].as<float>());
      } else if (mode == "limiter") {
        dsp->AddLim(rate, p["threshold_db"].as<float>(), p["knee_width_db"].as<float>(), p["attack_time"].as<float>(),
                    p["release_time"].as<float>());
      } else if (mode == "compressor") {
        dsp->AddDRC(rate, p["threshold_db"].as<float>(), p["ratio"].as<uint32_t>(), p["knee_width_db"].as<float>(),
                    p["attack_time"].as<float>(), p["release_time"].as<float>());
      } else {
        throw std::invalid_argument("Unknown component mode");
      }
    }
    std::vector<float> result;
    for (size_t offset = 0; offset < input.size(); offset += frame) {
      const size_t count = std::min<size_t>(frame, input.size() - offset);
      std::vector<float> block(frame, 0);
      std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(offset), count, block.begin());
      std::vector<float> output;
      if (aec) {
        std::vector<float> playback(frame, 0);
        std::copy_n(reference.begin() + static_cast<std::ptrdiff_t>(offset), count, playback.begin());
        aec->ProcessChunk(block, playback, output);
      } else {
        dsp->ProcessChunk(block, output);
      }
      result.insert(result.end(), output.begin(), output.end());
    }
    std::ofstream file(argv[5], std::ios::binary);
    file.write(reinterpret_cast<const char*>(result.data()),
               static_cast<std::streamsize>(input.size() * sizeof(float)));
    if (!file) {
      throw std::runtime_error("Cannot write reference output");
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
