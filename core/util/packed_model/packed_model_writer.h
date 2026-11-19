#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gigaedge::enhance::util {

uint32_t read_uint32_le(const uint8_t* ptr);
std::vector<uint8_t> write_uint32_le(uint32_t value);
std::vector<uint8_t> write_packed_model(const std::string& config, const std::vector<uint8_t>& model);

}  // namespace gigaedge::enhance::util
