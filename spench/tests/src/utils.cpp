#include "utils.h"
#include <vector>
#include <string>
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
  // int bytesRead = fread(header, 1, 44, file);

  std::vector<float> wavData;
  if (bytesRead > 0) {
    static const uint16_t BUFFER_SIZE = 4096;
    uint16_t* buffer = new uint16_t[BUFFER_SIZE];

    bytesRead = fread(
        buffer,
        sizeof buffer[0],
        BUFFER_SIZE / (sizeof buffer[0]),
        file
    );
    while (bytesRead > 0) {
      for (int i = 0; i < bytesRead; i++) {
        // std::cout << buffer[i] << " | " << buffer[i] / 32767.0 << " ";
        wavData.push_back(buffer[i] / 32767.0);
      }
      // std::cout << std::endl;
      bytesRead = fread(
          buffer,
          sizeof buffer[0],
          BUFFER_SIZE / (sizeof buffer[0]),
          file
      );
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
