#include "spench/packed_model.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "spench/util/packed_model/packed_model_writer.h"

namespace spench::util {

uint32_t read_uint32_le(const uint8_t* ptr) {
  if (ptr == nullptr) {
    throw std::invalid_argument("cannot read an integer from a null buffer");
  }
  return static_cast<uint32_t>(ptr[0]) |
         (static_cast<uint32_t>(ptr[1]) << 8) |
         (static_cast<uint32_t>(ptr[2]) << 16) |
         (static_cast<uint32_t>(ptr[3]) << 24);
}

std::vector<uint8_t> write_uint32_le(uint32_t value) {
  return {(uint8_t)(value & 0xFF),
          (uint8_t)((value >> 8) & 0xFF),
          (uint8_t)((value >> 16) & 0xFF),
          (uint8_t)((value >> 24) & 0xFF)};
}

std::vector<uint8_t> write_packed_model(const std::string& config, const std::vector<uint8_t>& model) {
  constexpr size_t header_size = 2 * sizeof(uint32_t);
  constexpr size_t max_field_size = std::numeric_limits<uint32_t>::max();
  if (config.size() > max_field_size || model.size() > max_field_size) {
    throw std::length_error("packed model fields cannot exceed UINT32_MAX bytes");
  }

  std::vector<uint8_t> buffer;
  if (config.size() > buffer.max_size() - header_size ||
      model.size() > buffer.max_size() - header_size - config.size()) {
    throw std::length_error("packed model is too large");
  }
  buffer.reserve(header_size + config.size() + model.size());

  auto config_size = write_uint32_le(static_cast<uint32_t>(config.size()));
  buffer.insert(buffer.end(), config_size.begin(), config_size.end());
  buffer.insert(buffer.end(), config.begin(), config.end());

  auto model_size = write_uint32_le(static_cast<uint32_t>(model.size()));
  buffer.insert(buffer.end(), model_size.begin(), model_size.end());
  buffer.insert(buffer.end(), model.begin(), model.end());

  return buffer;
}

}  // namespace spench::util

namespace spench {

PackedModelReader::PackedModelReader(const std::string& file_path) {
  std::ifstream stream(file_path, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open packed model file: " + file_path);
  }

  const std::ifstream::pos_type end_position = stream.tellg();
  if (end_position < 0) {
    throw std::runtime_error("failed to read packed model file size: " + file_path);
  }

  const size_t data_size = static_cast<size_t>(end_position);
  std::vector<uint8_t> data(data_size);

  stream.seekg(0, std::ios::beg);
  if (data_size > 0 && !stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data_size))) {
    throw std::runtime_error("failed to read packed model file: " + file_path);
  }

  parse_packed_data(data.data(), data.size());
}

PackedModelReader::PackedModelReader(const std::vector<uint8_t>& data) {
  parse_packed_data(data.data(), data.size());
}

PackedModelReader::PackedModelReader(const uint8_t* data, size_t data_size) {
  parse_packed_data(data, data_size);
}

PackedModelReader::PackedModelReader(uintptr_t source_ptr, uint32_t size)
    : PackedModelReader(reinterpret_cast<const uint8_t*>(source_ptr), size) {}

const std::string& PackedModelReader::config() const noexcept { return config_; }

const std::vector<uint8_t>& PackedModelReader::model() const noexcept {
  return model_;
}

void PackedModelReader::parse_packed_data(const uint8_t* data, size_t data_size) {
  if (data == nullptr || data_size < 8) {
    throw std::runtime_error("packed data too short");
  }

  size_t offset = 0;
  auto config_size = util::read_uint32_le(&data[offset]);
  offset += 4;
  if (config_size > data_size - offset || data_size - offset - config_size < sizeof(uint32_t)) {
    throw std::runtime_error("malformed packed data (config)");
  }

  config_.assign(reinterpret_cast<const char*>(&data[offset]), config_size);
  offset += config_size;

  auto model_size = util::read_uint32_le(&data[offset]);
  offset += 4;
  if (model_size > data_size - offset) {
    throw std::runtime_error("malformed packed data (model)");
  }

  model_.assign(&data[offset], &data[offset] + model_size);
  offset += model_size;
  if (offset != data_size) {
    throw std::runtime_error("malformed packed data (trailing bytes)");
  }
}

}  // namespace spench
