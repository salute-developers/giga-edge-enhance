#pragma once

#include <dsplib.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <valarray>
#include <vector>

#pragma pack(push, 1)
struct WavHeader {
  uint8_t chunk_id[4];  // "RIFF"
  uint32_t chunk_size;
  uint8_t format[4];  // "WAVE"
};

struct WavFmtChunk {
  uint8_t subchunk1_id[4];  // "fmt "
  uint32_t subchunk1_size;
  uint16_t audio_format;  // PCM = 1
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
};

struct WavDataChunk {
  uint8_t subchunk2_id[4];  // "data"
  uint32_t subchunk2_size;
};
#pragma pack(pop)

template <typename InputType, typename OutputType>
std::vector<OutputType> LoadVectorFromBinaryStream(std::istream& f) {
  std::vector<OutputType> data;
  InputType value;
  while (f.read(reinterpret_cast<char*>(&value), sizeof(InputType))) {
    data.push_back(static_cast<OutputType>(value));
  }
  return data;
}
template <typename InputType>
std::vector<InputType> LoadVectorFromBinaryStream(std::istream& f) {
  return LoadVectorFromBinaryStream<InputType, InputType>(f);
}

template <typename InputType, typename OutputType>
std::vector<OutputType> LoadVectorFromBinaryFile(const std::string& fname) {
  std::ifstream f(fname, std::ios::binary);
  if (!f.is_open()) {
    throw std::runtime_error("could not open file " + fname);
  }
  return LoadVectorFromBinaryStream<InputType, OutputType>(f);
}

template <typename InputType>
void SaveVectorToBinaryFile(const std::string& fname, const std::vector<InputType>& data) {
  std::ofstream f(fname, std::ios::binary);
  f.write(reinterpret_cast<const char*>(data.data()), sizeof(InputType) * data.size());
}

void LoadWavFx(const char* path, std::vector<float>& wav_data, float keep_unnorm = false);
void LoadWavFx(const char* path, std::vector<int16_t>& wav_data);

void IsVectorsClose(const std::vector<float>& v1, const std::vector<float>& v2, float eps);
void IsVectorsClose(const std::vector<int16_t>& v1, const std::vector<int16_t>& v2, float eps);

void IsVectorsClose(const std::vector<float>& v1, const std::valarray<float>& v2, float eps);

template <typename InputType>
std::vector<std::vector<InputType>> ChunkWav(const std::vector<InputType>& wav, int32_t sample_rate,
                                             int32_t chunk_size_ms, int32_t stride_ms) {
  std::vector<std::vector<InputType>> chunks;

  size_t step_size_samples = std::floor((sample_rate * chunk_size_ms) / 1000.0f);
  size_t hop_window_samples = std::floor((sample_rate * stride_ms) / 1000.0f);
  if (step_size_samples > wav.size()) {
    return {wav};
  }

  size_t num_steps = (wav.size() + step_size_samples - 1) / hop_window_samples;
  chunks.reserve(num_steps);
  for (size_t step = 0; step < num_steps; ++step) {
    std::vector<InputType> wav_chunk(wav.begin() + std::min(step * hop_window_samples, wav.size()),
                                     wav.begin() + std::min(step * hop_window_samples + step_size_samples, wav.size()));

    if (wav_chunk.size() < step_size_samples) {
      std::vector<InputType> zeroes(step_size_samples - wav_chunk.size(), 0);
      wav_chunk.insert(wav_chunk.end(), zeroes.begin(), zeroes.end());
    }
    chunks.push_back(wav_chunk);
  }
  return chunks;
}

template <typename T>
static std::vector<T> MergeChunks(const std::vector<std::vector<T>>& chunks, std::size_t start, std::size_t count) {
  REQUIRE(start <= chunks.size());
  REQUIRE(start + count <= chunks.size());

  std::size_t total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    total += chunks[start + i].size();
  }

  std::vector<T> merged;
  merged.reserve(total);

  for (std::size_t i = 0; i < count; ++i) {
    const auto& c = chunks[start + i];
    merged.insert(merged.end(), c.begin(), c.end());
  }

  return merged;
}

template <typename InputType>
void LoadBin(const char* path, uint32_t step_size_samples, std::vector<std::vector<InputType>>& output) {
  std::ifstream ifs(path, std::ios::binary);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  int16_t loops = fsize / (step_size_samples * sizeof(InputType));
  for (int16_t i = 0; i < loops; ++i) {
    std::vector<InputType> stft_tensor(step_size_samples, 0.0);
    ifs.read(reinterpret_cast<char*>(stft_tensor.data()), step_size_samples * sizeof(InputType));
    output.emplace_back(stft_tensor);
  }
}

template <typename InputType>
void LoadBin(const char* path, std::vector<InputType>& output) {
  std::ifstream ifs(path, std::ios::binary);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, ifs.beg);

  output.resize(fsize / sizeof(InputType));
  ifs.read(reinterpret_cast<char*>(output.data()), output.size() * sizeof(InputType));
}

template <typename InputType>
void LoadBin(const char* path, std::valarray<InputType>& output) {
  std::ifstream ifs(path, std::ios::binary);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, ifs.beg);

  output.resize(fsize / sizeof(InputType));
  ifs.read(reinterpret_cast<char*>(&output[0]), output.size() * sizeof(InputType));
}

dsplib::arr_real DumbRir(int n);

dsplib::arr_real Tone(uint32_t fs, int32_t n, float f_hz, float amp);
dsplib::arr_real Noise(int32_t n, float amp);
float RmsDb(const dsplib::arr_real& x);
float MaxAbs(const dsplib::arr_real& x);
