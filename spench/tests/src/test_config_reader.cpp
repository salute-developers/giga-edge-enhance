#include <catch.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>  // NOLINT
#include <random>
#include <string>

#include "spench/src/spench_config_reader.h"

namespace fs = std::filesystem;

namespace {

// Helper to create & clean up a temp YAML file
struct TempYamlFile {
  std::string path;
  explicit TempYamlFile(const std::string& content) {
    auto base = fs::temp_directory_path();
    std::mt19937_64 gen {std::random_device {}()};
    std::uniform_int_distribution<uint64_t> dist;
    for (int i = 0; i < 10; ++i) {
      auto cand = base / ("spench_cfg_" + std::to_string(dist(gen)) + ".yaml");
      if (!fs::exists(cand)) {
        path = cand.string();
        break;
      }
    }
    REQUIRE_FALSE(path.empty());
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    REQUIRE(ofs.good());
    ofs << content;
  }
  ~TempYamlFile() {
    std::error_code ec;
    if (!path.empty()) fs::remove(path, ec);
  }
};

// A minimal concrete reader so we can call mergeYaml(...)
struct DummyReader : public spench::ConfigReader {
  void load() override {}
};

}  // namespace

// ------------------------------------------------------------------------------
// mergeYaml
// ------------------------------------------------------------------------------
TEST_CASE("mergeYaml deep-merges maps and overrides scalars/sequences [config reader]") {
  DummyReader r;

  YAML::Node dest = YAML::Load(R"(
a:
  b: 1
  c: 2
d: [1, 2]
x: 5
)");

  const YAML::Node src = YAML::Load(R"(
a:
  b: 10
d: [3, 4]
e: 7
)");

  r.mergeYaml(dest, src);

  REQUIRE(dest["a"]["b"].as<int>() == 10);      // overridden
  REQUIRE(dest["a"]["c"].as<int>() == 2);       // preserved
  REQUIRE(dest["d"].IsSequence());
  REQUIRE(dest["d"].size() == 2);
  REQUIRE(dest["d"][0].as<int>() == 3);
  REQUIRE(dest["x"].as<int>() == 5);            // untouched
  REQUIRE(dest["e"].as<int>() == 7);            // added
}

TEST_CASE("mergeYaml with non-map source is a no-op [config reader]") {
  DummyReader r;

  YAML::Node dest1 = YAML::Load("{ a: 1, b: { c: 2 } }");
  const YAML::Node src_scalar = YAML::Load("42");
  r.mergeYaml(dest1, src_scalar);
  REQUIRE(dest1["a"].as<int>() == 1);
  REQUIRE(dest1["b"]["c"].as<int>() == 2);

  YAML::Node dest2 = YAML::Load("{ k: [1,2,3] }");
  const YAML::Node src_seq = YAML::Load("[4,5,6]");
  r.mergeYaml(dest2, src_seq);
  REQUIRE(dest2["k"].IsSequence());
  REQUIRE(dest2["k"].size() == 3);
  REQUIRE(dest2["k"][0].as<int>() == 1);
}

// ------------------------------------------------------------------------------
// StringConfigReader
// ------------------------------------------------------------------------------
TEST_CASE("StringConfigReader merges overrides and keeps built-in defaults [config reader]") {
  const std::string override_yaml = R"(
model:
  model_path: "my_model.onnx"
  model_inputs: [in1, in2]
inference:
  engine_num_threads: 4
feature_params:
  sampling_freq: 16000
extra_section:
  flag: true
)";

  spench::StringConfigReader reader(override_yaml);
  REQUIRE_NOTHROW(reader.load());

  const YAML::Node& p = reader.params();
  // Overridden by snippet
  REQUIRE(p["model"]["model_path"].as<std::string>() == "my_model.onnx");
  REQUIRE(p["inference"]["engine_num_threads"].as<int>() == 4);
  REQUIRE(p["feature_params"]["sampling_freq"].as<int>() == 16000);
  REQUIRE(p["model"]["model_inputs"].IsSequence());
  REQUIRE(p["model"]["model_inputs"].size() == 2);

  // Defaults intact
  REQUIRE(p["inference"]["model_type"].as<std::string>() == "best_speed");
  REQUIRE(p["inference"]["gpu"].as<bool>() == false);

  // New sections carried over
  REQUIRE(p["extra_section"]["flag"].as<bool>() == true);
}

TEST_CASE("StringConfigReader throws on malformed YAML", "[config reader]") {
  const std::string bad_yaml = "model: [in1, in2\n";  // missing ]
  spench::StringConfigReader reader(bad_yaml);
  REQUIRE_THROWS_AS(reader.load(), YAML::ParserException);
}

// ------------------------------------------------------------------------------
// FileConfigReader
// ------------------------------------------------------------------------------
TEST_CASE("FileConfigReader loads file and merges over defaults [config reader]") {
  const std::string file_yaml = R"(
model:
  model_path: "file_model.onnx"
inference:
  engine_num_threads: 8
)";

  TempYamlFile tmp(file_yaml);
  spench::FileConfigReader reader(tmp.path);
  REQUIRE_NOTHROW(reader.load());

  const YAML::Node& p = reader.params();
  REQUIRE(p["model"]["model_path"].as<std::string>() == "file_model.onnx");
  REQUIRE(p["inference"]["engine_num_threads"].as<int>() == 8);

  // Keys not in the file should not exist
  REQUIRE_FALSE(p["inference"]["model_type"]);
  REQUIRE_FALSE(p["model"]["model_outputs"]);
}

TEST_CASE("FileConfigReader throws if file missing [config reader]") {
  spench::FileConfigReader reader("/does/not/exist.yaml");
  REQUIRE_THROWS(reader.load());
}

TEST_CASE("FileConfigReader throws on malformed YAML in file [config reader]") {
  TempYamlFile tmp("inference: [1, 2\n");  // bad YAML
  spench::FileConfigReader reader(tmp.path);
  REQUIRE_THROWS_AS(reader.load(), std::runtime_error);
}

// ------------------------------------------------------------------------------
// DenoiserConfigReader
// ------------------------------------------------------------------------------
TEST_CASE("DenoiserConfigReader getters return correct override and default values [denoiser config reader]") {
  const std::string yaml = R"(
model:
  model_path: "mod.onnx"
  model_inputs: ["i1","i2"]
  model_outputs: ["o1","o2"]
feature_params:
  sampling_freq: 12345
  frame_size_ms:   64
  frame_stride_ms: 32
  window_type:    "hamming"
inference:
  gpu:                true
  engine_num_threads: 7
  model_type:        "best_quality"
)";

  auto base = std::make_unique<spench::StringConfigReader>(yaml);
  spench::DenoiserConfigReader cfg(std::move(base));
  REQUIRE_NOTHROW(cfg.load());

  REQUIRE(cfg.modelPath()    == "mod.onnx");
  REQUIRE(cfg.modelInputs()  == std::vector<std::string>({"i1", "i2"}));
  REQUIRE(cfg.modelOutputs() == std::vector<std::string>({"o1", "o2"}));
  REQUIRE(cfg.samplingFreq() == 12345u);
  REQUIRE(cfg.frameSizeMs()  == 64u);
  REQUIRE(cfg.frameStrideMs()== 32u);
  REQUIRE(cfg.windowType()   == "hamming");
  REQUIRE(cfg.useGpu()       == true);
  REQUIRE(cfg.numThreads()   == 7u);
  REQUIRE(cfg.modelType()    == "best_quality");
}

TEST_CASE("DenoiserConfigReader uses defaults when fields are missing [denoiser config reader]") {
  const std::string yaml = R"(
model:
  model_path: "onlypath.onnx"
)";

  auto base = std::make_unique<spench::StringConfigReader>(yaml);
  spench::DenoiserConfigReader cfg(std::move(base));
  REQUIRE_NOTHROW(cfg.load());

  REQUIRE(cfg.modelPath()    == "onlypath.onnx");
  REQUIRE(cfg.modelInputs().size() == 4);
  REQUIRE(cfg.modelOutputs().size() == 4);
  REQUIRE(cfg.samplingFreq() == 24000u);
  REQUIRE(cfg.frameSizeMs()  == 32u);
  REQUIRE(cfg.frameStrideMs()== 16u);
  REQUIRE(cfg.windowType()   == "hanning");
  REQUIRE(cfg.useGpu()       == false);
  REQUIRE(cfg.numThreads()   == 1u);
  REQUIRE(cfg.modelType()    == "best_speed");
}

TEST_CASE("DenoiserConfigReader throws if model_path is missing [denoiser config reader]") {
  const std::string yaml = R"(
model: {}
)";
  auto base = std::make_unique<spench::StringConfigReader>(yaml);
  spench::DenoiserConfigReader cfg(std::move(base));
  REQUIRE_THROWS_AS(cfg.load(), std::runtime_error);
}
