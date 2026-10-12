#include <yaml-cpp/yaml.h>

#include <catch.hpp>
#include <chrono>  // NOLINT
#include <memory>

#include "src/config_reader.h"
#include "src/feature_extractor_runner.h"
#include "tests/src/utils.h"

struct TestFERunner {
  std::unique_ptr<gigaedge::spench::FeatureExtractorRunner> fe_runner_ptr;
  std::shared_ptr<gigaedge::spench::DenoiserConfig> cfg_ptr;
  const std::size_t num_attempts = 1000;
  std::vector<float> wav_data;
  const std::string data_root = "data/";

  TestFERunner() {}
  TestFERunner& operator=(const TestFERunner&) = delete;
  TestFERunner(const TestFERunner&) = delete;
  TestFERunner(TestFERunner&& orig) : fe_runner_ptr(std::move(orig.fe_runner_ptr)), wav_data(std::move(orig.wav_data)) {
    orig.wav_data.clear();
    orig.fe_runner_ptr = nullptr;
  }

  TestFERunner& SetWav(const std::string& wav_path) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_data);
    return *this;
  }

  TestFERunner& LoadDataFromBin(const std::string& dump, uint32_t step_size_samples,
                                std::vector<std::vector<float>>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFERunner& LoadSpecDataFromBin(const std::string& dump, std::vector<std::vector<float>>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    uint32_t step_size_samples = fe_runner_ptr.get()->get_pow_spec_feat_size_samples();
    LoadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFERunner& LoadDataFromBin(const std::string& dump, std::vector<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestFERunner& LoadDataFromBin(const std::string& dump, std::valarray<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }

  YAML::Node GetYAML(const std::string& yaml_config_name) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    return YAML::LoadFile(file_path);
  }

  void InitYAMLReader(const std::string& yaml_config_name) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    cfg_ptr = std::make_shared<gigaedge::spench::DenoiserConfig>(gigaedge::spench::FileConfigReader(file_path));
    // return YAML::LoadFile(file_path);
  }

  TestFERunner& InitFERunner(const std::string& yaml_config_name) {
    InitYAMLReader(yaml_config_name);
    fe_runner_ptr = std::unique_ptr<gigaedge::spench::FeatureExtractorRunner>(
        new gigaedge::spench::FeatureExtractorRunner(cfg_ptr));
    return *this;
  }
};

TEST_CASE("SUCCEED Read config for runner with modified .yaml file and check values [fe runner]") {
  auto test_data = std::move(TestFERunner().InitFERunner("test_configuration_with_no_default_values.yaml"));

  REQUIRE(test_data.fe_runner_ptr.get()->get_sampling_freq() == 8000);
  REQUIRE(test_data.fe_runner_ptr.get()->get_frame_size_ms() == 40);
}

TEST_CASE("FAIL Read config for runner with incorrect sampling frequency value [fe runner]") {
  CHECK_THROWS(TestFERunner().InitFERunner("test_configuration_with_incorrect_sampling_freq.yaml"));
}

TEST_CASE("FAIL Read config for runner with incorrect frame size value [fe runner]") {
  CHECK_THROWS_WITH(TestFERunner().InitFERunner("test_configuration_with_incorrect_frame_size.yaml"),
                    "Config reading error. Frame size is not a scalar!");
}

TEST_CASE("FAIL Read config for runner with incorrect window hop value [fe runner]") {
  CHECK_THROWS_WITH(TestFERunner().InitFERunner("test_configuration_with_incorrect_window_hop.yaml"),
                    "Config reading error. Frame size stide is not a scalar!");
}

TEST_CASE("FAIL Read config for runner with incorrect window type value [fe runner]") {
  CHECK_THROWS_WITH(TestFERunner().InitFERunner("test_configuration_with_incorrect_window_type.yaml"),
                    "Window type must be in [\"hanning\", \"hamming\"]");
}
