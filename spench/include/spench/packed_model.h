#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spench {

class PackedModelReader {
 public:
  PackedModelReader(const PackedModelReader&) = default;
  PackedModelReader& operator=(const PackedModelReader&) = default;
  PackedModelReader(PackedModelReader&&) = default;
  PackedModelReader& operator=(PackedModelReader&&) = default;
  explicit PackedModelReader(const std::string& file_path);
  explicit PackedModelReader(const std::vector<uint8_t>& data);
  PackedModelReader(const uint8_t* data, size_t data_size);
  PackedModelReader(uintptr_t source_ptr, uint32_t size);

  const std::string& config() const noexcept;
  const std::vector<uint8_t>& model() const noexcept;

 protected:
  std::string config_;
  std::vector<uint8_t> model_;
  void parse_packed_data(const uint8_t* data, size_t data_size);
};

}  // namespace spench
