#pragma once

#include <sys/stat.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace audio {

/// Basic format info for a WAV file
struct WavInfo {
  uint16_t num_channels = 0;     ///< Number of audio channels
  uint32_t sample_rate = 0;      ///< Samples per second
  uint16_t bits_per_sample = 0;  ///< Bits per sample (e.g. 16)
};

/// Read raw 16-bit PCM samples from a WAV file, filling out_samples and out_info.
/// Throws std::runtime_error on any I/O or format error.
void load_wav(const std::string& path, std::vector<int16_t>& out_samples, WavInfo& out_info);

/// Read and (optionally) normalize to float in [-1,1], or leave raw int values as float.
/// Throws std::runtime_error on error.
void load_wav(const std::string& path, std::vector<float>& out_samples, bool keep_unnormalized, WavInfo& out_info);

/// Write interleaved 16-bit PCM WAV samples. Floats are clamped to [-1,1].
/// Throws std::runtime_error on any I/O error.
void write_wav(const std::string& path, const std::vector<float>& data, const WavInfo& info);

}  // namespace audio

// -----------------------------------------------------------------------------

struct Args {
  std::string config_path;
  std::string packed_model_path;
  std::string source_audio_path;
  std::string enhanced_audio_path;
  int32_t use_resampling = 0;
  uint32_t chunk_ms = 16;
};

// -----------------------------------------------------------------------------

#include <functional>
#include <valarray>

template <typename InputType, typename OutputType>
std::vector<OutputType> load_vector_from_binary_stream(std::istream& f) {
  std::vector<OutputType> data;
  InputType value;
  while (f.read(reinterpret_cast<char*>(&value), sizeof(InputType))) {
    data.push_back(static_cast<OutputType>(value));
  }
  return data;
}

template <typename InputType>
std::vector<InputType> load_vector_from_binary_stream(std::istream& f) {
  return load_vector_from_binary_stream<InputType, InputType>(f);
}

template <typename InputType, typename OutputType>
std::vector<OutputType> load_vector_from_binary_file(const std::string& fname) {
  std::ifstream f(fname, std::ios::binary);
  if (!f.is_open()) {
    throw std::runtime_error("Could not open file " + fname);
  }
  return load_vector_from_binary_stream<InputType, OutputType>(f);
}

template <typename InputType>
void save_vector_to_binary_file(const std::string& fname, const std::vector<InputType>& data) {
  std::ofstream f(fname, std::ios::binary);
  if (!f) throw std::runtime_error("Could not create file " + fname);
  f.write(reinterpret_cast<const char*>(data.data()), sizeof(InputType) * data.size());
}

// -----------------------------------------------------------------------------

#include <cmath>

template <typename InputType>
std::vector<std::vector<InputType>> chunk_wav(const std::vector<InputType>& wav, int32_t sample_rate,
                                              int32_t chunk_size_ms, int32_t stride_ms) {
  std::vector<std::vector<InputType>> chunks;
  size_t window = static_cast<size_t>(std::ceil((sample_rate * chunk_size_ms) / 1000.0f));
  size_t hop = static_cast<size_t>(std::ceil((sample_rate * stride_ms) / 1000.0f));

  if (window >= wav.size()) {
    return {wav};
  }

  size_t num_steps = (wav.size() + hop - 1) / hop;
  chunks.reserve(num_steps);

  for (size_t i = 0; i < num_steps; ++i) {
    size_t start = std::min(i * hop, wav.size());
    size_t end = std::min(start + window, wav.size());
    std::vector<InputType> slice(wav.begin() + start, wav.begin() + end);
    if (slice.size() < window) {
      slice.resize(window, InputType(0));
    }
    chunks.push_back(std::move(slice));
  }
  return chunks;
}

// -----------------------------------------------------------------------------

template <typename InputType>
void load_bin(const char* path, uint32_t step_size_samples, std::vector<std::vector<InputType>>& output) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) throw std::runtime_error(std::string("Could not open file ") + path);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  size_t loops = static_cast<size_t>(fsize) / (step_size_samples * sizeof(InputType));
  output.reserve(loops);
  for (size_t i = 0; i < loops; ++i) {
    std::vector<InputType> buffer(step_size_samples);
    ifs.read(reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(InputType));
    output.emplace_back(std::move(buffer));
  }
}

template <typename InputType>
void load_bin(const char* path, std::vector<InputType>& output) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) throw std::runtime_error(std::string("Could not open file ") + path);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  output.resize(static_cast<size_t>(fsize) / sizeof(InputType));
  ifs.read(reinterpret_cast<char*>(output.data()), output.size() * sizeof(InputType));
}

template <typename InputType>
void load_bin(const char* path, std::valarray<InputType>& output) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) throw std::runtime_error(std::string("Could not open file ") + path);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  output.resize(static_cast<size_t>(fsize) / sizeof(InputType));
  ifs.read(reinterpret_cast<char*>(&output[0]), output.size() * sizeof(InputType));
}

std::string make_audio_process_config_yaml(uint32_t sample_rate);

void parse_arguments(int32_t argc, char* argv[], Args& args, std::function<void(int32_t, char*[], Args&)> help_func);
