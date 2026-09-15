#include "tests/src/utils.h"

#include <algorithm>
#include <catch.hpp>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

void LoadWavFx(const char* path, std::vector<float>& wav_data, float keep_unnorm) {
  std::vector<int16_t> pcm16;
  wav_data.clear();
  LoadWavFx(path, pcm16);

  std::for_each(pcm16.begin(), pcm16.end(), [&wav_data, keep_unnorm](const int16_t& item) {
    if (keep_unnorm) {
      wav_data.push_back(static_cast<float>(item));
    } else {
      float value = (item < 0) ? static_cast<float>(item) / 32768.0f : static_cast<float>(item) / 32767.0f;
      wav_data.push_back(static_cast<float>(value));
    }
  });
}

void LoadWavFx(const char* path, std::vector<int16_t>& wav_data) {
  FILE* file = fopen(path, "rb");
  if (!file) throw std::invalid_argument(std::string("Can't open file: ") + path);

  WavHeader riff_header;
  if (fread(&riff_header, sizeof(WavHeader), 1, file) != 1) throw std::runtime_error("Unable to read WAV header");

  if (std::strncmp(reinterpret_cast<char*>(riff_header.chunk_id), "RIFF", 4) ||
      std::strncmp(reinterpret_cast<char*>(riff_header.format), "WAVE", 4))
    throw std::runtime_error("Invalid WAV file format");

  WavFmtChunk fmt_chunk{};
  bool fmt_chunk_found = false;
  WavDataChunk data_chunk{};
  bool data_chunk_found = false;

  while (!fmt_chunk_found || !data_chunk_found) {
    uint8_t chunk_id[4];
    uint32_t chunk_size;
    if (fread(chunk_id, sizeof(chunk_id), 1, file) != 1 || fread(&chunk_size, sizeof(chunk_size), 1, file) != 1)
      throw std::runtime_error("Unexpected end of file when reading chunk headers");

    if (!std::strncmp(reinterpret_cast<char*>(chunk_id), "fmt ", 4)) {
      if (chunk_size < sizeof(WavFmtChunk) - 8) throw std::runtime_error("Invalid fmt chunk size");

      fmt_chunk.subchunk1_size = chunk_size;
      if (fread(&fmt_chunk.audio_format, chunk_size, 1, file) != 1)
        throw std::runtime_error("Could not read fmt chunk");

      if (fmt_chunk.audio_format != 1 || fmt_chunk.bits_per_sample != 16)
        throw std::runtime_error("Unsupported WAV format (only 16-bit PCM supported)");

      fmt_chunk_found = true;
    } else if (!std::strncmp(reinterpret_cast<char*>(chunk_id), "data", 4)) {
      data_chunk.subchunk2_size = chunk_size;
      data_chunk_found = true;
      break;
    } else {
      fseek(file, chunk_size, SEEK_CUR);
    }
  }

  if (!fmt_chunk_found || !data_chunk_found) throw std::runtime_error("Required chunks not found (fmt or data)");

  size_t samples_count = data_chunk.subchunk2_size / sizeof(int16_t);
  wav_data.resize(samples_count);

  if (fread(wav_data.data(), sizeof(int16_t), samples_count, file) != samples_count)
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
                [&v1_float](const int16_t& item) { v1_float.push_back(static_cast<float>(item)); });
  std::for_each(v2.begin(), v2.end(),
                [&v2_float](const int16_t& item) { v2_float.push_back(static_cast<int16_t>(item)); });
  IsVectorsClose(v1_float, v2_float, eps);
}

dsplib::arr_real DumbRir(int n) {
  auto rir = dsplib::rand({-1, 1}, 2 * n) * dsplib::window::gauss(2 * n, 20);
  rir = rir / dsplib::sum(rir);
  rir[n] = 0.5;
  rir = rir.slice(n, 2 * n);
  return rir;
}

dsplib::arr_real Tone(uint32_t fs, int32_t n, float f_hz, float amp) {
  const auto t = dsplib::arange(n) / static_cast<float>(fs);
  return amp * sin(2 * dsplib::pi * f_hz * t);
}

dsplib::arr_real Noise(int32_t n, float amp) {
  const auto i = dsplib::arange(n);
  auto x = dsplib::zeros(n);
  x += sin(2 * dsplib::pi * (0.013f) * i);
  x += 0.7f * sin(2 * dsplib::pi * (0.017f) * i);
  x += 0.5f * sin(2 * dsplib::pi * (0.019f) * i);
  x /= dsplib::max(dsplib::abs(x)) + 1e-9f;
  return amp * x;
}

float RmsDb(const dsplib::arr_real& x) { return dsplib::mag2db(dsplib::rms(x) + 1e-12f); }

float MaxAbs(const dsplib::arr_real& x) { return dsplib::max(dsplib::abs(x)); }
