#pragma once

#include <string>
#include <vector>

namespace spench {

class PackedModelReader {
 public:
  PackedModelReader(const PackedModelReader&) = default;
  PackedModelReader& operator=(const PackedModelReader&) = default;
  PackedModelReader(PackedModelReader&&) = default;
  PackedModelReader& operator=(PackedModelReader&&) = default;
  explicit PackedModelReader(const std::vector<uint8_t>& data);
  PackedModelReader(const uint8_t* data, size_t data_size);
  PackedModelReader(const uintptr_t src_ptr, uint32_t size);

  const std::string& getConfig() const noexcept;
  const std::vector<uint8_t>& getModel() const noexcept;
 protected:
  std::string config;
  std::vector<uint8_t> model;
  void parsePackedData(const uint8_t* data, size_t data_size);
};

}  // namespace spench
