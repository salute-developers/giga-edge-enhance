#include "demo/utils.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace audio {

namespace {

template <typename T>
void read_le(std::ifstream& ifs, T& value) {
  static_assert(std::is_integral<T>::value, "Integral type required");
  std::array<unsigned char, sizeof(T)> buf{};
  ifs.read(reinterpret_cast<char*>(buf.data()), buf.size());
  if (!ifs) {
    throw std::runtime_error("Unexpected EOF while reading integer");
  }
  value = 0;
  for (std::size_t i = 0; i < buf.size(); ++i) {
    value |= static_cast<T>(buf[i]) << (8 * i);
  }
}

template <typename T>
void write_le(std::ofstream& ofs, T value) {
  static_assert(std::is_integral<T>::value, "Integral type required");
  using UnsignedT = typename std::make_unsigned<T>::type;
  const UnsignedT unsigned_value = static_cast<UnsignedT>(value);
  std::array<unsigned char, sizeof(T)> buf{};
  for (std::size_t i = 0; i < buf.size(); ++i) {
    buf[i] = static_cast<unsigned char>(unsigned_value >> (8 * i));
  }
  ofs.write(reinterpret_cast<const char*>(buf.data()), buf.size());
}

bool has_id(const std::array<char, 4>& id, const char (&expected)[5]) {
  return std::memcmp(id.data(), expected, id.size()) == 0;
}

}  // namespace

void load_wav(const std::string& path, std::vector<int16_t>& out_samples, WavInfo& out_info) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    throw std::runtime_error("Cannot open file: " + path);
  }

  std::array<char, 4> riff_id{};
  std::array<char, 4> wave_id{};
  uint32_t riff_size = 0;
  ifs.read(riff_id.data(), riff_id.size());
  read_le(ifs, riff_size);
  ifs.read(wave_id.data(), wave_id.size());
  if (!ifs || !has_id(riff_id, "RIFF") || !has_id(wave_id, "WAVE")) {
    throw std::runtime_error("Not a valid RIFF/WAVE file");
  }
  (void)riff_size;

  bool found_format = false;
  bool found_data = false;
  uint16_t block_align = 0;

  while (ifs && !found_data) {
    std::array<char, 4> chunk_id{};
    uint32_t chunk_size = 0;
    ifs.read(chunk_id.data(), chunk_id.size());
    if (!ifs) {
      break;
    }
    read_le(ifs, chunk_size);

    if (has_id(chunk_id, "fmt ")) {
      if (chunk_size < 16) {
        throw std::runtime_error("Invalid fmt chunk");
      }

      uint16_t audio_format = 0;
      uint32_t byte_rate = 0;
      read_le(ifs, audio_format);
      read_le(ifs, out_info.num_channels);
      read_le(ifs, out_info.sample_rate);
      read_le(ifs, byte_rate);
      read_le(ifs, block_align);
      read_le(ifs, out_info.bits_per_sample);
      ifs.seekg(static_cast<std::streamoff>(chunk_size - 16), std::ios::cur);

      if (audio_format != 1 || out_info.bits_per_sample != 16 || out_info.num_channels == 0 ||
          out_info.sample_rate == 0 || block_align != out_info.num_channels * sizeof(int16_t) ||
          byte_rate != out_info.sample_rate * block_align) {
        throw std::runtime_error("Only valid 16-bit PCM WAV files are supported");
      }
      found_format = true;
    } else if (has_id(chunk_id, "data")) {
      if (!found_format || chunk_size % block_align != 0) {
        throw std::runtime_error("Invalid WAV data chunk");
      }

      const size_t sample_count = chunk_size / sizeof(int16_t);
      out_samples.resize(sample_count);
      for (size_t i = 0; i < sample_count; ++i) {
        uint16_t sample = 0;
        read_le(ifs, sample);
        out_samples[i] = static_cast<int16_t>(sample);
      }
      found_data = true;
    } else {
      ifs.seekg(static_cast<std::streamoff>(chunk_size), std::ios::cur);
    }

    if (chunk_size % 2 != 0) {
      ifs.seekg(1, std::ios::cur);
    }
  }

  if (!ifs || !found_data) {
    throw std::runtime_error("No valid data chunk found");
  }
}

void load_wav(const std::string& path, std::vector<float>& out_samples, bool keep_unnormalized, WavInfo& out_info) {
  std::vector<int16_t> tmp;
  load_wav(path, tmp, out_info);

  out_samples.clear();
  out_samples.reserve(tmp.size());

  if (keep_unnormalized) {
    for (auto s : tmp) {
      out_samples.push_back(static_cast<float>(s));
    }
  } else {
    const float scale = static_cast<float>(std::numeric_limits<int16_t>::max());
    for (auto s : tmp) {
      out_samples.push_back(static_cast<float>(s) / scale);
    }
  }
}

void write_wav(const std::string& path, const std::vector<float>& data, const WavInfo& info) {
  if (info.num_channels == 0 || info.sample_rate == 0 || info.bits_per_sample != 16 ||
      data.size() % info.num_channels != 0) {
    throw std::invalid_argument("WAV output must be interleaved 16-bit PCM audio with valid metadata");
  }

  constexpr uint16_t audio_format = 1;
  const uint16_t channels = info.num_channels;
  constexpr uint16_t bits_per_sample = 16;
  const uint16_t block_align = channels * sizeof(int16_t);
  const uint32_t byte_rate = info.sample_rate * block_align;
  const uint64_t data_bytes = static_cast<uint64_t>(data.size()) * sizeof(int16_t);
  if (data_bytes > std::numeric_limits<uint32_t>::max() - 36) {
    throw std::overflow_error("WAV output exceeds RIFF size limit");
  }

  std::ofstream ofs(path, std::ios::binary);
  if (!ofs) {
    throw std::runtime_error("Cannot create file: " + path);
  }

  ofs.write("RIFF", 4);
  write_le(ofs, static_cast<uint32_t>(36 + data_bytes));
  ofs.write("WAVE", 4);

  ofs.write("fmt ", 4);
  write_le(ofs, uint32_t{16});
  write_le(ofs, audio_format);
  write_le(ofs, channels);
  write_le(ofs, info.sample_rate);
  write_le(ofs, byte_rate);
  write_le(ofs, block_align);
  write_le(ofs, bits_per_sample);

  ofs.write("data", 4);
  write_le(ofs, static_cast<uint32_t>(data_bytes));

  for (auto f : data) {
    const float clamped = std::max(-1.0f, std::min(1.0f, f));
    write_le(ofs, static_cast<int16_t>(clamped * std::numeric_limits<int16_t>::max()));
  }
  if (!ofs) {
    throw std::runtime_error("Failed to write WAV data");
  }
}

}  // namespace audio

std::string make_audio_process_config_yaml(uint32_t sample_rate) {
  std::ostringstream out;
  out << "sound_processing:\n"
      << "  noise_gate_ctrl:\n"
      << "    sample_rate: " << sample_rate << '\n'
      << "  loudness_ctrl:\n"
      << "    sample_rate: " << sample_rate << '\n'
      << "  limiter_ctrl:\n"
      << "    sample_rate: " << sample_rate;
  return out.str();
}

void parse_arguments(int32_t argc, char* argv[], Args& args,
                     std::function<void(int32_t, char* argv[], Args&)> help_func) {
  help_func(argc, argv, args);
}
