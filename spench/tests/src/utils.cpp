#include "tests/src/utils.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <catch.hpp>
#include <cstring>


void loadWavFx(const char* path, std::vector<float>& wavData, float keepUnnorm) {
  std::vector<int16_t> pcm16;
  wavData.clear();
  loadWavFx(path, pcm16);

  std::for_each(pcm16.begin(), pcm16.end(),
                [&wavData, keepUnnorm](int16_t const & item) {
                    if (keepUnnorm) {
                      wavData.push_back(static_cast<float>(item));
                    } else {
                      float value = (item < 0) ?
                                    static_cast<float>(item) / 32768.0f :
                                    static_cast<float>(item) / 32767.0f;
                      wavData.push_back(static_cast<float>(value));
                    }
                });
}

void loadWavFx(const char* path, std::vector<int16_t>& wavData) {
  FILE* file = fopen(path, "rb");
  if (!file)
    throw std::invalid_argument(std::string("Can't open file: ") + path);

  WavHeader riffHeader;
  if (fread(&riffHeader, sizeof(WavHeader), 1, file) != 1)
    throw std::runtime_error("Unable to read WAV header");

  if (std::strncmp(reinterpret_cast<char*>(riffHeader.chunkID), "RIFF", 4) ||
      std::strncmp(reinterpret_cast<char*>(riffHeader.format), "WAVE", 4))
    throw std::runtime_error("Invalid WAV file format");

  WavFmtChunk fmtChunk;
  bool fmtChunkFound = false;
  WavDataChunk dataChunk;
  bool dataChunkFound = false;

  // Search for fmt and data chunks
  while (!fmtChunkFound || !dataChunkFound) {
    uint8_t chunkID[4];
    uint32_t chunkSize;
    if (fread(chunkID, sizeof(chunkID), 1, file) != 1 ||
        fread(&chunkSize, sizeof(chunkSize), 1, file) != 1)
      throw std::runtime_error("Unexpected end of file when reading chunk headers");

    if (!std::strncmp(reinterpret_cast<char*>(chunkID), "fmt ", 4)) {
      if (chunkSize < sizeof(WavFmtChunk) - 8)
        throw std::runtime_error("Invalid fmt chunk size");

      fmtChunk.subchunk1Size = chunkSize;
      if (fread(&fmtChunk.audioFormat, chunkSize, 1, file) != 1)
        throw std::runtime_error("Could not read fmt chunk");

      if (fmtChunk.audioFormat != 1 || fmtChunk.bitsPerSample != 16)
        throw std::runtime_error("Unsupported WAV format (only 16-bit PCM supported)");

      fmtChunkFound = true;
    } else if (!std::strncmp(reinterpret_cast<char*>(chunkID), "data", 4)) {
      dataChunk.subchunk2Size = chunkSize;
      dataChunkFound = true;
      // Data chunk found; typically the last needed chunk
      break;
    } else {
      // Skip unrecognized chunk
      fseek(file, chunkSize, SEEK_CUR);
    }
  }

  if (!fmtChunkFound || !dataChunkFound)
    throw std::runtime_error("Required chunks not found (fmt or data)");

  size_t samplesCount = dataChunk.subchunk2Size / sizeof(int16_t);
  wavData.resize(samplesCount);

  if (fread(wavData.data(), sizeof(int16_t), samplesCount, file) != samplesCount)
    throw std::runtime_error("Failed to read WAV data");

  fclose(file);
}

void IsVectorsClose(const std::vector<float>& v1, const std::vector<float>& v2, float eps) {
  size_t num_samples = std::min(v1.size(), v2.size());
  REQUIRE(v1.size() == v2.size());
  for (size_t i = 0; i < num_samples; ++i) {
    CAPTURE(i);
    REQUIRE(v1[i] == Approx(v2[i]).margin(eps));
  }
}

void IsVectorsClose(const std::vector<float>& v1, const std::valarray<float>& v2, float eps) {
  size_t num_samples = std::min(v1.size(), v2.size());
  REQUIRE(v1.size() == v2.size());
  for (size_t i = 0; i < num_samples; ++i) {
    CAPTURE(i);
    REQUIRE(v1[i] == Approx(v2[i]).margin(eps));
  }
}

void IsVectorsClose(const std::vector<int16_t>& v1, const std::vector<int16_t>& v2, float eps) {
  std::vector<float> v1_float;
  std::vector<float> v2_float;

  std::for_each(v1.begin(), v1.end(),
                [&v1_float](int16_t const & item) {
                    v1_float.push_back(static_cast<float>(item));
                                                        });
  std::for_each(v2.begin(), v2.end(),
                [&v2_float](int16_t const & item) {
                    v2_float.push_back(static_cast<int16_t>(item));
                                                        });
  IsVectorsClose(v1_float, v2_float, eps);
}

// dumb room impulse responce generator
// TODO(vayulis): use real RIR
dsplib::arr_real DumbRIR(int n) {
  auto rir = dsplib::rand({-1, 1}, 2 * n) * dsplib::window::gauss(2 * n, 20);
  rir = rir / dsplib::sum(rir);
  rir[n] = 0.5;
  rir = rir.slice(n, 2 * n);
  return rir;
}
