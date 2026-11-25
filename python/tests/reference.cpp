#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "enhance/denoiser.h"

int main(int argc, char** argv) {
  try {
    if (argc != 6) {
      throw std::invalid_argument("Expected config, input.f32, output.f32, input_rate, output_rate");
    }
    const auto config_path = std::filesystem::absolute(argv[1]);
    auto config = YAML::LoadFile(config_path.string());
    auto weights = std::filesystem::path(config["model"]["model_path"].as<std::string>());
    if (weights.is_relative()) {
      weights = config_path.parent_path() / weights;
    }
    config["model"]["model_path"] = weights.string();
    giga_edge::enhance::Denoiser denoiser(YAML::Dump(config));
    const uint32_t input_rate = static_cast<uint32_t>(std::stoul(argv[4]));
    const uint32_t output_rate = static_cast<uint32_t>(std::stoul(argv[5]));
    const uint64_t model_rate = denoiser.GetModelSampleRate();
    const uint64_t hop = denoiser.GetModelInputFrameSize();
    const size_t input_hop = static_cast<size_t>((hop * input_rate + model_rate - 1) / model_rate);
    std::ifstream file(argv[2], std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || static_cast<size_t>(file.tellg()) % sizeof(float) != 0) {
      throw std::runtime_error("Invalid reference input");
    }
    std::vector<float> input(static_cast<size_t>(file.tellg()) / sizeof(float));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(input.data()), static_cast<std::streamsize>(input.size() * sizeof(float)));
    auto destroy = [&denoiser](giga_edge::enhance::Context<float>* context) { denoiser.DestroyContext(&context); };
    std::unique_ptr<giga_edge::enhance::Context<float>, decltype(destroy)> context(
        denoiser.CreateContext(input_rate, output_rate), destroy);
    std::vector<float> result;
    for (size_t offset = 0; offset < input.size(); offset += input_hop) {
      const size_t count = std::min(input_hop, input.size() - offset);
      std::vector<float> frame(input_hop, 0);
      std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(offset), count, frame.begin());
      std::vector<float> output;
      if (denoiser.ProcessChunk(frame, output, context.get()) < GigaEdgeEnhanceRetCode::SUCCESS) {
        throw std::runtime_error("Reference processing failed");
      }
      result.insert(result.end(), output.begin(), output.end());
    }
    std::vector<float> tail;
    if (denoiser.GetFinalChunk(tail, context.get()) < GigaEdgeEnhanceRetCode::SUCCESS) {
      throw std::runtime_error("Reference finalization failed");
    }
    result.insert(result.end(), tail.begin(), tail.end());
    const uint64_t future = config["model"]["model_input_ctx_len"].as<uint32_t>(0);
    const size_t delay = static_cast<size_t>((future + 1) * ((hop * output_rate + model_rate / 2) / model_rate));
    const size_t count = static_cast<size_t>((input.size() * output_rate + input_rate - 1) / input_rate);
    if (result.size() < delay + count) {
      throw std::runtime_error("Reference result has insufficient tail");
    }
    std::ofstream output(argv[3], std::ios::binary);
    output.write(reinterpret_cast<const char*>(result.data() + delay),
                 static_cast<std::streamsize>(count * sizeof(float)));
    if (!output) {
      throw std::runtime_error("Cannot write reference output");
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
