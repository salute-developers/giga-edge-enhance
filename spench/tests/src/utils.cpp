#include "tests/src/utils.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <catch.hpp>


std::vector<float> loadWavFx(const char* path) {
  FILE* file = fopen(path, "r");
  if (file == nullptr) {
    std::string errorMsg = "Can't open file: " + std::string(path);
    throw std::invalid_argument(errorMsg.c_str());
  }

  WavHeader header;
  int bytesRead = fread(&header, 1, 44, file);

  std::vector<float> wavData;
  if (bytesRead > 0) {
    static const int16_t BUFFER_SIZE = 4096;
    int16_t* buffer = new int16_t[BUFFER_SIZE];

    bytesRead = fread(
        buffer,
        sizeof buffer[0],
        BUFFER_SIZE / (sizeof buffer[0]),
        file);
    while (bytesRead > 0) {
      for (int i = 0; i < bytesRead; i++) {
        wavData.push_back(static_cast<float>(buffer[i]) / 32767.0);
      }
      bytesRead = fread(
          buffer,
          sizeof buffer[0],
          BUFFER_SIZE / (sizeof buffer[0]),
          file);
    }

    delete [] buffer;
    buffer = nullptr;
  }

  fclose(file);
  return wavData;
}

void IsVectorsClose(const std::vector<float>& v1, const std::vector<float>& v2, float eps) {
  size_t num_samples = std::min(v1.size(), v2.size());
  for (size_t i = 0; i < num_samples; ++i) {
    CAPTURE(i);
    REQUIRE(v1[i] == Approx(v2[i]).margin(eps));
  }
}

std::vector<std::vector<float>> chunkWav(const std::vector<float>& wav, int sample_rate, int chunk_size_ms) {
  std::vector<std::vector<float>> chunks;
  size_t step_size_samples = (sample_rate * chunk_size_ms) / 1000;

  if (step_size_samples > wav.size()) {
    return {wav};
  }

  size_t num_steps = (wav.size() + step_size_samples - 1) / step_size_samples;
  chunks.reserve(num_steps);
  for (size_t step = 0; step < num_steps; ++step) {
    std::vector<float> wav_chunk(wav.begin() + step * step_size_samples,
                                 wav.begin() + std::min((step + 1) * step_size_samples, wav.size()));

    // padding
    if (wav_chunk.size() < step_size_samples) {
      std::vector<float> zeroes(step_size_samples - wav_chunk.size(), 0.0);
      wav_chunk.insert(wav_chunk.end(), zeroes.begin(), zeroes.end());
    }
    chunks.push_back(wav_chunk);
  }
  return chunks;
}
