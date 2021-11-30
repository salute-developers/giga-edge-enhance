#include "tests/src/utils.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <catch.hpp>


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
  FILE* file = fopen(path, "r");
  if (file == nullptr) {
    std::string errorMsg = "Can't open file: " + std::string(path);
    throw std::invalid_argument(errorMsg.c_str());
  }

  WavHeader header;
  int bytesRead = fread(&header, 1, 44, file);

  wavData.clear();
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
        wavData.push_back(buffer[i]);
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
}

void IsVectorsClose(const std::vector<float>& v1, const std::vector<float>& v2, float eps) {
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
