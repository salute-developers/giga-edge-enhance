#pragma once

#include <vector>
#include <fstream>
#include <string>
#include <algorithm>
#include <valarray>
#include <dsplib.h>

#pragma pack(push, 1)
typedef struct WAV_HEADER {
  uint8_t chunkID[4];    // "RIFF"
  uint32_t chunkSize;
  uint8_t format[4];     // "WAVE"
} WavHeader;

typedef struct WAV_FMT_CHUNK {
  uint8_t subchunk1ID[4];  // "fmt "
  uint32_t subchunk1Size;
  uint16_t audioFormat;    // PCM = 1
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
} WavFmtChunk;

typedef struct WAV_DATA_CHUNK {
  uint8_t subchunk2ID[4];  // "data"
  uint32_t subchunk2Size;
} WavDataChunk;
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

void loadWavFx(const char* path, std::vector<float>&, float keepUnnorm = false);
void loadWavFx(const char* path, std::vector<int16_t>&);

void IsVectorsClose(const std::vector<float>& v1, const std::vector<float>& v2, float eps);
void IsVectorsClose(const std::vector<int16_t>& v1, const std::vector<int16_t>& v2, float eps);

void IsVectorsClose(const std::vector<float>& v1, const std::valarray<float>& v2, float eps);

template <typename InputType>
std::vector<std::vector<InputType>> chunkWav(const std::vector<InputType>& wav,
                                             int32_t sample_rate,
                                             int32_t chunk_size_ms,
                                             int32_t stride_ms) {
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
                                      wav.begin() + std::min(step * hop_window_samples +
                                                                    step_size_samples, wav.size()));

    // padding
    if (wav_chunk.size() < step_size_samples) {
      std::vector<InputType> zeroes(step_size_samples - wav_chunk.size(), 0);
      wav_chunk.insert(wav_chunk.end(), zeroes.begin(), zeroes.end());
    }
    chunks.push_back(wav_chunk);
  }
  return chunks;
}

template <typename InputType>
void loadBin(const char* path, uint32_t step_size_samples, std::vector<std::vector<InputType>>& output) {
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
void loadBin(const char* path, std::vector<InputType>& output) {
  std::ifstream ifs(path, std::ios::binary);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, ifs.beg);

  output.resize(fsize / sizeof(InputType));
  ifs.read(reinterpret_cast<char*>(output.data()), output.size() * sizeof(InputType));
}

template <typename InputType>
void loadBin(const char* path, std::valarray<InputType>& output) {
  std::ifstream ifs(path, std::ios::binary);

  ifs.seekg(0, std::ios::end);
  std::streampos fsize = ifs.tellg();
  ifs.seekg(0, ifs.beg);

  output.resize(fsize / sizeof(InputType));
  ifs.read(reinterpret_cast<char*>(&output[0]), output.size() * sizeof(InputType));
}

dsplib::arr_real DumbRIR(int n);

dsplib::arr_real tone(uint32_t fs, int32_t n, float f_hz, float amp);
dsplib::arr_real noise(int32_t n, float amp);
float rmsDb(const dsplib::arr_real& x);
float maxAbs(const dsplib::arr_real& x);
