#include <yaml-cpp/yaml.h>

#include <catch.hpp>
#include <filesystem>  // NOLINT
#include <fstream>
#include <random>
#include <string>

#include "src/config_reader.h"
#include "util/packed_model/packed_model_writer.h"

namespace fs = std::filesystem;

namespace {

// Helper to create & clean up a temp YAML file
struct TempYamlFile {
  std::string path;
  explicit TempYamlFile(const std::string& content) {
    auto base = fs::temp_directory_path();
    std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    for (int i = 0; i < 10; ++i) {
      auto cand = base / ("enhance_cfg_" + std::to_string(dist(gen)) + ".yaml");
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

// A minimal concrete reader so we can call merge_yaml(...)
struct DummyReader : public giga_edge::enhance::ConfigReader {};

struct MiniNetworkReader final : public giga_edge::enhance::NetworkConfig {
  using giga_edge::enhance::NetworkConfig::NetworkConfig;
};

}  // namespace

// ------------------------------------------------------------------------------
// merge_yaml
// ------------------------------------------------------------------------------
TEST_CASE("SUCCESS merge_yaml deep-merges maps and overrides scalars/sequences [config reader]") {
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

  giga_edge::enhance::merge_yaml(dest, src);

  REQUIRE(dest["a"]["b"].as<int>() == 10);  // overridden
  REQUIRE(dest["a"]["c"].as<int>() == 2);   // preserved
  REQUIRE(dest["d"].IsSequence());
  REQUIRE(dest["d"].size() == 2);
  REQUIRE(dest["d"][0].as<int>() == 3);
  REQUIRE(dest["x"].as<int>() == 5);  // untouched
  REQUIRE(dest["e"].as<int>() == 7);  // added
}

TEST_CASE("SUCCESS merge_yaml with non-map source is a no-op [config reader]") {
  YAML::Node dest1 = YAML::Load("{ a: 1, b: { c: 2 } }");
  const YAML::Node src_scalar = YAML::Load("42");
  giga_edge::enhance::merge_yaml(dest1, src_scalar);
  REQUIRE(dest1["a"].as<int>() == 1);
  REQUIRE(dest1["b"]["c"].as<int>() == 2);

  YAML::Node dest2 = YAML::Load("{ k: [1,2,3] }");
  const YAML::Node src_seq = YAML::Load("[4,5,6]");
  giga_edge::enhance::merge_yaml(dest2, src_seq);
  REQUIRE(dest2["k"].IsSequence());
  REQUIRE(dest2["k"].size() == 3);
  REQUIRE(dest2["k"][0].as<int>() == 1);
}

// ------------------------------------------------------------------------------
// StringConfigReader
// ------------------------------------------------------------------------------
TEST_CASE("SUCCESS StringConfigReader reads from string [config reader]") {
  const std::string yaml = R"(
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

  std::unique_ptr<giga_edge::enhance::StringConfigReader> reader;
  REQUIRE_NOTHROW(reader = std::make_unique<giga_edge::enhance::StringConfigReader>(yaml));

  const YAML::Node& p = reader->params();
  // Overridden by snippet
  REQUIRE(p["model"]["model_path"].as<std::string>() == "my_model.onnx");
  REQUIRE(p["inference"]["engine_num_threads"].as<int>() == 4);
  REQUIRE(p["feature_params"]["sampling_freq"].as<int>() == 16000);
  REQUIRE(p["model"]["model_inputs"].IsSequence());
  REQUIRE(p["model"]["model_inputs"].size() == 2);

  // New sections carried over
  REQUIRE(p["extra_section"]["flag"].as<bool>() == true);
}

TEST_CASE("FAIL StringConfigReader throws on malformed YAML", "[config reader]") {
  const std::string bad_yaml = "model: [in1, in2\n";  // missing ]
  std::unique_ptr<giga_edge::enhance::StringConfigReader> reader;
  REQUIRE_THROWS_AS(reader = std::make_unique<giga_edge::enhance::StringConfigReader>(bad_yaml), std::runtime_error);
}

// ------------------------------------------------------------------------------
// FileConfigReader
// ------------------------------------------------------------------------------
TEST_CASE("SUCCESS FileConfigReader loads file and merges over defaults [config reader]") {
  const std::string file_yaml = R"(
model:
  model_path: "file_model.onnx"
inference:
  engine_num_threads: 8
)";

  TempYamlFile tmp(file_yaml);
  std::unique_ptr<giga_edge::enhance::FileConfigReader> reader;
  REQUIRE_NOTHROW(reader = std::make_unique<giga_edge::enhance::FileConfigReader>(tmp.path));

  const YAML::Node& p = reader->params();
  REQUIRE(p["model"]["model_path"].as<std::string>() == "file_model.onnx");
  REQUIRE(p["inference"]["engine_num_threads"].as<int>() == 8);

  // Keys not in the file should not exist
  REQUIRE_FALSE(p["inference"]["model_type"]);
  REQUIRE_FALSE(p["model"]["model_outputs"]);
}

TEST_CASE("FAIL FileConfigReader throws if file missing [config reader]") {
  std::unique_ptr<giga_edge::enhance::FileConfigReader> reader;
  REQUIRE_THROWS(reader = std::make_unique<giga_edge::enhance::FileConfigReader>("/does/not/exist.yaml"));
}

TEST_CASE("FAIL FileConfigReader throws on malformed YAML in file [config reader]") {
  TempYamlFile tmp("inference: [1, 2\n");  // bad YAML
  std::unique_ptr<giga_edge::enhance::FileConfigReader> reader;
  REQUIRE_THROWS_AS(reader = std::make_unique<giga_edge::enhance::FileConfigReader>(tmp.path), std::runtime_error);
}

// ------------------------------------------------------------------------------
// DenoiserConfig
// ------------------------------------------------------------------------------
TEST_CASE("SUCCESS DenoiserConfig getters return correct override and default values [denoiser config reader]") {
  const std::string yaml = R"(
model:
  model_path: "mod.onnx"
feature_params:
  sampling_freq: 12345
  frame_size_ms:   64
  frame_stride_ms: 32
  window_type:    "hamming"
inference:
  gpu:                true
  engine_num_threads: 7
  model_type:        "denoiser_discr_hq_model"
)";

  auto base = giga_edge::enhance::StringConfigReader("");
  REQUIRE_NOTHROW(base = giga_edge::enhance::StringConfigReader(yaml));
  giga_edge::enhance::DenoiserConfig cfg(base);

  REQUIRE(cfg.model_path() == "mod.onnx");
  REQUIRE(cfg.sampling_freq() == 12345u);
  REQUIRE(cfg.frame_size_ms() == 64u);
  REQUIRE(cfg.frame_stride_ms() == 32u);
  REQUIRE(cfg.window_type() == "hamming");
  REQUIRE(cfg.use_gpu() == true);
  REQUIRE(cfg.num_threads() == 7u);
  REQUIRE(cfg.model_type() == "denoiser_discr_hq_model");
}

TEST_CASE("SUCCESS DenoiserConfig uses defaults when fields are missing [denoiser config reader]") {
  const std::string yaml = R"(
model:
  model_path: "onlypath.onnx"
)";

  auto base = giga_edge::enhance::StringConfigReader("");
  REQUIRE_NOTHROW(base = giga_edge::enhance::StringConfigReader(yaml));
  giga_edge::enhance::DenoiserConfig cfg(base);

  REQUIRE(cfg.model_path() == "onlypath.onnx");
  REQUIRE(cfg.sampling_freq() == 48000u);
  REQUIRE(cfg.frame_size_ms() == 32u);
  REQUIRE(cfg.frame_stride_ms() == 16u);
  REQUIRE(cfg.window_type() == "hanning");
  REQUIRE(cfg.use_gpu() == false);
  REQUIRE(cfg.num_threads() == 1u);
  REQUIRE(cfg.model_type() == "denoiser_discr_web_model");
}

TEST_CASE("SUCCESS DenoiserConfig throws if model_path is missing [denoiser config reader]") {
  const std::string yaml = R"(
model: {}
)";
  auto base = giga_edge::enhance::StringConfigReader(yaml);
  std::unique_ptr<giga_edge::enhance::DenoiserConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::DenoiserConfig>(base), std::runtime_error);
}

TEST_CASE("SUCCESS NetworkConfig accepts optional scalar model_input_ctx_len [network config reader]") {
  const std::string yaml = R"(
model:
  model_path: "net.onnx"
  model_input_ctx_len: 64
feature_params:
  sampling_freq:    24000
  frame_size_ms:    32
  frame_stride_ms:  16
  window_type:      hanning
)";

  auto base = giga_edge::enhance::StringConfigReader(yaml);
  std::unique_ptr<MiniNetworkReader> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<MiniNetworkReader>(base));
}

TEST_CASE("FAIL NetworkConfig missing 'model' section throws [network config reader]") {
  const std::string yaml = R"(
inference:
  engine_num_threads: 2
)";
  auto base = giga_edge::enhance::StringConfigReader(yaml);
  std::unique_ptr<MiniNetworkReader> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<MiniNetworkReader>(base), std::runtime_error);
}

TEST_CASE("FAIL NetworkConfig missing 'feature_params' section throws [network config reader]") {
  const std::string yaml = R"(
model:
  model_path: "net.onnx"
  model_input_ctx_len: 64
)";
  auto base = giga_edge::enhance::StringConfigReader(yaml);
  std::unique_ptr<MiniNetworkReader> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<MiniNetworkReader>(base), std::runtime_error);
}

TEST_CASE("FAIL NetworkConfig missing or empty model_path throws [network config reader]") {
  SECTION("model section present but model_path missing") {
    const std::string yaml = R"(
model: {}
)";
    auto base = giga_edge::enhance::StringConfigReader(yaml);
    std::unique_ptr<MiniNetworkReader> cfg;
    REQUIRE_THROWS_AS(cfg = std::make_unique<MiniNetworkReader>(base), std::runtime_error);
  }

  SECTION("model_path is an empty string") {
    const std::string yaml = R"(
model:
  model_path: ""
)";
    auto base = giga_edge::enhance::StringConfigReader(yaml);
    std::unique_ptr<MiniNetworkReader> cfg;
    REQUIRE_THROWS_AS(cfg = std::make_unique<MiniNetworkReader>(base), std::runtime_error);
  }
}

TEST_CASE("FAIL NetworkConfig model_input_ctx_len must be a scalar when present [network config reader]") {
  const std::string yaml = R"(
model:
  model_path: "ok.onnx"
  model_input_ctx_len: { not: "scalar" }
)";
  auto base = giga_edge::enhance::StringConfigReader(yaml);
  std::unique_ptr<MiniNetworkReader> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<MiniNetworkReader>(base), std::runtime_error);
}

// ------------------------------------------------------------------------------
// AudioProcessorConfig
// ------------------------------------------------------------------------------
TEST_CASE("SUCCESS AudioProcessorConfig generic getter returns defaults [audio processing reader]") {
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>());

  REQUIRE(cfg->get<uint32_t>("range_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("range_ctrl", "target_level_db") == Approx(-3.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "max_gain_db") == Approx(10.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "attack_time") == Approx(0.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "release_time") == Approx(300.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "hold_time") == Approx(5.0f));

  REQUIRE(cfg->get<float>("auto_gain_ctrl", "target_level") == Approx(1.0f));
  REQUIRE(cfg->get<float>("auto_gain_ctrl", "max_gain") == Approx(20.0f));
  REQUIRE(cfg->get<uint32_t>("auto_gain_ctrl", "average_len") == 6000u);
  REQUIRE(cfg->get<float>("auto_gain_ctrl", "step_size_rise") == Approx(0.01f));
  REQUIRE(cfg->get<float>("auto_gain_ctrl", "step_size_fall") == Approx(0.01f));

  REQUIRE(cfg->get<uint32_t>("noise_gate_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "threshold") == Approx(-40.0f));
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "attack_time") == Approx(0.1f));
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "release_time") == Approx(0.02f));
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "hold_time") == Approx(0.05f));

  REQUIRE(cfg->get<uint32_t>("dynamic_range_compressor_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "threshold") == Approx(-10.0f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "ratio") == Approx(20.0f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "knee_width") == Approx(0.0f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "attack_time") == Approx(0.01f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "release_time") == Approx(0.2f));

  REQUIRE(cfg->get<uint32_t>("loudness_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("loudness_ctrl", "target_level_db") == Approx(-15.0f));
  REQUIRE(cfg->get<float>("loudness_ctrl", "bypass_time") == Approx(0.1f));
  REQUIRE(cfg->get<float>("loudness_ctrl", "max_gain_db") == Approx(30.0f));
  REQUIRE(cfg->get<bool>("loudness_ctrl", "post_processing") == true);

  REQUIRE(cfg->get<uint32_t>("limiter_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("limiter_ctrl", "threshold") == Approx(-3.0f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "knee_width") == Approx(0.0f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "attack_time") == Approx(0.0f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "release_time") == Approx(0.2f));
}

TEST_CASE("SUCCESS AudioProcessorConfig overrides some fields [audio processing reader]") {
  const std::string override_yaml = R"(
sound_processing:
  loudness_ctrl:
    bypass_time: 4.5
  limiter_ctrl:
    threshold: -1.5
)";

  auto base = giga_edge::enhance::StringConfigReader(override_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base));

  REQUIRE(cfg->get<float>("loudness_ctrl", "bypass_time") == Approx(4.5f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "threshold") == Approx(-1.5f));
  REQUIRE(cfg->get<uint32_t>("loudness_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<uint32_t>("limiter_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<bool>("loudness_ctrl", "post_processing") == true);
}

TEST_CASE("SUCCESS AudioProcessorConfig loudness_ctrl post_processing can be set false [audio processing reader]") {
  const std::string override_yaml = R"(
sound_processing:
  loudness_ctrl:
    post_processing: false
)";

  auto base = giga_edge::enhance::StringConfigReader(override_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base));

  REQUIRE(cfg->get<bool>("loudness_ctrl", "post_processing") == false);
}

TEST_CASE("SUCCESS AudioProcessorConfig getters return defaults [audio processing reader]") {
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>());

  REQUIRE(cfg->get<uint32_t>("range_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("range_ctrl", "target_level_db") == Approx(-3.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "max_gain_db") == Approx(10.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "attack_time") == Approx(0.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "release_time") == Approx(300.0f));
  REQUIRE(cfg->get<float>("range_ctrl", "hold_time") == Approx(5.0f));

  REQUIRE(cfg->get<float>("auto_gain_ctrl", "target_level") == Approx(1.0f));
  REQUIRE(cfg->get<float>("auto_gain_ctrl", "max_gain") == Approx(20.0f));
  REQUIRE(cfg->get<uint32_t>("auto_gain_ctrl", "average_len") == 6000u);
  REQUIRE(cfg->get<float>("auto_gain_ctrl", "step_size_rise") == Approx(0.01f));
  REQUIRE(cfg->get<float>("auto_gain_ctrl", "step_size_fall") == Approx(0.01f));

  REQUIRE(cfg->get<uint32_t>("noise_gate_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "threshold") == Approx(-40.0f));
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "attack_time") == Approx(0.1f));
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "release_time") == Approx(0.02f));
  REQUIRE(cfg->get<float>("noise_gate_ctrl", "hold_time") == Approx(0.05f));

  REQUIRE(cfg->get<uint32_t>("dynamic_range_compressor_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "threshold") == Approx(-10.0f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "ratio") == Approx(20.0f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "knee_width") == Approx(0.0f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "attack_time") == Approx(0.01f));
  REQUIRE(cfg->get<float>("dynamic_range_compressor_ctrl", "release_time") == Approx(0.2f));

  REQUIRE(cfg->get<uint32_t>("loudness_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("loudness_ctrl", "target_level_db") == Approx(-15.0f));
  REQUIRE(cfg->get<float>("loudness_ctrl", "bypass_time") == Approx(0.1f));
  REQUIRE(cfg->get<float>("loudness_ctrl", "max_gain_db") == Approx(30.0f));
  REQUIRE(cfg->get<bool>("loudness_ctrl", "post_processing") == true);

  REQUIRE(cfg->get<uint32_t>("limiter_ctrl", "sample_rate") == 48000u);
  REQUIRE(cfg->get<float>("limiter_ctrl", "threshold") == Approx(-3.0f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "knee_width") == Approx(0.0f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "attack_time") == Approx(0.0f));
  REQUIRE(cfg->get<float>("limiter_ctrl", "release_time") == Approx(0.2f));
}

TEST_CASE("SUCCESS AudioProcessorConfig ParamBlockView works [audio processing reader]") {
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>());

  auto ng = cfg->block("noise_gate_ctrl");
  REQUIRE(ng.get<uint32_t>("sample_rate") == 48000u);
  REQUIRE(ng.get<float>("threshold") == Approx(-40.0f));
}

TEST_CASE("FAIL AudioProcessorConfig detects non-scalar for required field [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  range_ctrl:
    sample_rate: [48000]
    target_level_db: -3.0
    max_gain_db: 10
    attack_time: 0.0
    release_time: 300
    hold_time: 5
  auto_gain_ctrl:
    target_level: 1
    max_gain: 20.0
    average_len: 6000
    step_size_rise: 0.01
    step_size_fall: 0.01
  noise_gate_ctrl:
    sample_rate: 48000
    threshold: -15.0
    attack_time: 0.05
    release_time: 0.02
    hold_time: 0.05
  dynamic_range_compressor_ctrl:
    sample_rate: 48000
    threshold: -15.0
    ratio: 5
    knee_width: 0.0
    attack_time: 0.01
    release_time: 0.2
  loudness_ctrl:
    sample_rate: 48000
    target_level_db: -23.0
    bypass_time: 3.0
    max_gain_db: 40
  limiter_ctrl:
    sample_rate: 48000
    threshold: -3.0
    knee_width: 0.0
    attack_time: 0.0
    release_time: 0.2
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("SUCCESS AudioProcessorConfig defaults missing key in limiter_ctrl [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  limiter_ctrl:
    sample_rate: 48000
    knee_width: 0.0
    attack_time: 0.0
    release_time: 0.2
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_NOTHROW(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base));

  REQUIRE(cfg->get<float>("limiter_ctrl", "threshold") == Approx(-3.0f));
}

// ------------------------------------------------------------------------------
// FAIL value/range
// ------------------------------------------------------------------------------
TEST_CASE("FAIL AudioProcessorConfig RC invalid sample_rate [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  range_ctrl:
    sample_rate: 200000
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessorConfig AGC non-positive target_level [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  auto_gain_ctrl:
    target_level: -0.01
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessorConfig NG non-positive times [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  noise_gate_ctrl:
    attack_time: -0.01
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessorConfig DRC non-positive release [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  dynamic_range_compressor_ctrl:
    release_time: -0.01
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessorConfig LC non-positive bypass_time [audio processing reader]") {
  const std::string bad_yaml = R"(
sound_processing:
  loudness_ctrl:
    bypass_time: -0.01
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessorConfig Limiter threshold not numeric [audio processing reader]") {
  // Scalar but not numeric → YAML::BadConversion during validation → rethrown as runtime_error by load()
  const std::string bad_yaml = R"(
sound_processing:
  limiter_ctrl:
    threshold: abc
)";
  auto base = giga_edge::enhance::StringConfigReader(bad_yaml);
  std::unique_ptr<giga_edge::enhance::AudioProcessorConfig> cfg;
  REQUIRE_THROWS_AS(cfg = std::make_unique<giga_edge::enhance::AudioProcessorConfig>(base), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessorConfig rejects unsafe DSP state sizes [audio processing reader]") {
  SECTION("zero Range Control sample rate") {
    auto base = giga_edge::enhance::StringConfigReader("sound_processing:\n  range_ctrl:\n    sample_rate: 0\n");
    CHECK_THROWS_AS(giga_edge::enhance::AudioProcessorConfig(base), std::runtime_error);
  }

  SECTION("zero compressor ratio") {
    auto base =
        giga_edge::enhance::StringConfigReader("sound_processing:\n  dynamic_range_compressor_ctrl:\n    ratio: 0\n");
    CHECK_THROWS_AS(giga_edge::enhance::AudioProcessorConfig(base), std::runtime_error);
  }

  SECTION("negative limiter knee") {
    auto base = giga_edge::enhance::StringConfigReader("sound_processing:\n  limiter_ctrl:\n    knee_width: -1\n");
    CHECK_THROWS_AS(giga_edge::enhance::AudioProcessorConfig(base), std::runtime_error);
  }
}


TEST_CASE("SUCCESS NetworkConfig constructed from bundled model uses bundle [network config]") {
  // Config that would normally reference a model file
  const std::string cfg = "model:\n  model_bundled: true\n";
  const std::vector<uint8_t> model_data = {0xAA, 0xBB, 0xCC};

  // Create packed bundle with bundled flag
  std::vector<uint8_t> packed = giga_edge::enhance::util::write_packed_model(cfg, model_data);
  auto bundle = std::make_shared<giga_edge::enhance::PackedModelReader>(packed);

  // Construct NetworkConfig with bundle
  giga_edge::enhance::DenoiserConfig denoiser(bundle);

  // Validate that model path is ignored and model data comes from bundle
  REQUIRE(denoiser.model_path().empty());
  REQUIRE(denoiser.get_model() == model_data);
  REQUIRE(denoiser.is_model_bundled());
}
