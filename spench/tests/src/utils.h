#pragma once

#include <vector>
#include <fstream>
#include <iostream>
#include <string>

typedef struct WAV_HEADER {
  uint8_t RIFF[4];
  uint32_t chunkSize;  // RIFF Chunk Size
  uint8_t WAVE[4];
  uint8_t fmt[4];
  uint32_t subchunk1Size;  // Size of the fmt chunk
  uint16_t audioFormat;  // must be 1 (PCM)
  uint16_t numOfChan;
  uint32_t sampleRate;
  uint32_t bytesPerSec;  // bytes per second
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  /* "data" sub-chunk */
  uint8_t subchunk2ID[4];  // "data"
  uint32_t subchunk2Size;  // Sampled data length
} WavHeader;

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

std::vector<float> loadWavFx(const char* path);

void IsVectorsClose(const std::vector<float>& v1, const std::vector<float>& v2, float eps);
