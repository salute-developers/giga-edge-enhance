#include <cstdint>
#include <string>
#include <vector>

namespace spench {

static uint32_t read_uint32_le(const uint8_t* ptr) {
  return static_cast<uint32_t>(ptr[0]) |
        (static_cast<uint32_t>(ptr[1]) << 8) |
        (static_cast<uint32_t>(ptr[2]) << 16) |
        (static_cast<uint32_t>(ptr[3]) << 24);
}

class PackedModelReader {
 public:
  explicit PackedModelReader(const std::vector<uint8_t>& data) { parse_packed_data(data); }

  std::string get_config() const noexcept { return config; }
  std::vector<uint8_t> get_model() const noexcept { return model; }

 private:
  std::string config;
  std::vector<uint8_t> model;

  void parse_packed_data(const std::vector<uint8_t>& data) {
    if (data.size() < 8)
      throw std::runtime_error("packed data too short");

    size_t offset = 0;
    auto config_size = read_uint32_le(&data[offset]); offset += 4;
    if (offset + config_size + 4 > data.size())
      throw std::runtime_error("malformed packed data (config)");

    config.assign(reinterpret_cast<const char*>(&data[offset]), config_size);
    offset += config_size;

    auto model_size = read_uint32_le(&data[offset]); offset += 4;
    if (offset + model_size > data.size())
      throw std::runtime_error("malformed packed data (model)");

    model.assign(&data[offset], &data[offset] + model_size);
    offset += model_size;
  }
};

}  // namespace spench
