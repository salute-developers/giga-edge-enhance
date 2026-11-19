#include <enhance/packed_model.h>
#include <util/packed_model/packed_model_writer.h>

#include <catch.hpp>
#include <string>
#include <vector>

TEST_CASE("SUCCEED write_packed_model and PackedModelReader work correctly [packed_model]") {
  std::string config = "myconfig";
  std::vector<uint8_t> model_bytes = {10, 20, 30};

  auto packed_data = gigaedge::enhance::util::write_packed_model(config, model_bytes);

  // Expected size: 8 bytes header + config size + model size
  REQUIRE(packed_data.size() == 8 + config.size() + model_bytes.size());

  // Verify header layout: config size, config, model size, model

  uint32_t config_size = gigaedge::enhance::util::read_uint32_le(packed_data.data());
  REQUIRE(config_size == config.size());
  std::string cfg_str(reinterpret_cast<const char*>(packed_data.data() + 4), config_size);
  REQUIRE(cfg_str == config);
  uint32_t model_size = gigaedge::enhance::util::read_uint32_le(packed_data.data() + 4 + config_size);
  REQUIRE(model_size == model_bytes.size());
  std::vector<uint8_t> model_back(packed_data.begin() + 4 + config_size + 4, packed_data.end());
  REQUIRE(model_back == model_bytes);

  // Use PackedModelReader to retrieve data
  gigaedge::enhance::PackedModelReader reader(packed_data);
  REQUIRE(reader.config() == config);
  REQUIRE(reader.model() == model_bytes);
}

TEST_CASE("FAIL PackedModelReader throws on insufficient data length [packed_model]") {
  std::vector<uint8_t> data(7, 0);  // less than 8 bytes
  REQUIRE_THROWS_AS(gigaedge::enhance::PackedModelReader(data), std::runtime_error);
}

TEST_CASE("FAIL PackedModelReader throws on invalid config size [packed_model]") {
  // config_size = 10, but only 2 bytes of config provided
  std::vector<uint8_t> data;
  auto size_bytes = gigaedge::enhance::util::write_uint32_le(10);

  data.insert(data.end(), size_bytes.begin(), size_bytes.end());
  data.push_back('x');
  data.push_back('y');  // 2 bytes config

  // model_size 0 -> 00 00 00 00
  auto model_bytes = gigaedge::enhance::util::write_uint32_le(0);
  data.insert(data.end(), model_bytes.begin(), model_bytes.end());
  REQUIRE_THROWS_AS(gigaedge::enhance::PackedModelReader(data), std::runtime_error);
}

TEST_CASE("FAIL PackedModelReader throws on invalid model size [packed_model]") {
  // config_size 2, config bytes 'a','b', model_size 10 but no model data
  std::vector<uint8_t> data;
  auto size_bytes = gigaedge::enhance::util::write_uint32_le(2);

  data.insert(data.end(), size_bytes.begin(), size_bytes.end());
  data.push_back('x');
  data.push_back('y');  // 2 bytes config
  auto model_bytes = gigaedge::enhance::util::write_uint32_le(10);
  data.insert(data.end(), model_bytes.begin(), model_bytes.end());
  REQUIRE_THROWS_AS(gigaedge::enhance::PackedModelReader(data), std::runtime_error);
}

TEST_CASE("FAIL PackedModelReader rejects a null buffer [packed_model]") {
  REQUIRE_THROWS_AS(gigaedge::enhance::PackedModelReader(nullptr, 8), std::runtime_error);
}

TEST_CASE("FAIL PackedModelReader rejects trailing data [packed_model]") {
  auto data = gigaedge::enhance::util::write_packed_model("config", {1, 2, 3});
  data.push_back(0);

  REQUIRE_THROWS_AS(gigaedge::enhance::PackedModelReader(data), std::runtime_error);
}

TEST_CASE("FAIL packed integer reader rejects a null buffer [packed_model]") {
  REQUIRE_THROWS_AS(gigaedge::enhance::util::read_uint32_le(nullptr), std::invalid_argument);
}
