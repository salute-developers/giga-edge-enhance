#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <catch.hpp>
#include <chrono>  // NOLINT
#include <cmath>
#include <future>  // NOLINT
#include <memory>

#include "src/config_reader.h"
#include "src/feature_extractor_runner.h"
#include "tests/src/utils.h"

struct TestFERunner {
  std::unique_ptr<gigaedge::enhance::FeatureExtractorRunner> fe_runner_ptr;
  std::shared_ptr<gigaedge::enhance::DenoiserConfig> cfg_ptr;
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
    cfg_ptr = std::make_shared<gigaedge::enhance::DenoiserConfig>(gigaedge::enhance::FileConfigReader(file_path));
    // return YAML::LoadFile(file_path);
  }

  TestFERunner& InitFERunner(const std::string& yaml_config_name) {
    InitYAMLReader(yaml_config_name);
    fe_runner_ptr = std::unique_ptr<gigaedge::enhance::FeatureExtractorRunner>(
        new gigaedge::enhance::FeatureExtractorRunner(cfg_ptr));
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

TEST_CASE("SUCCEED Shared feature runner preserves independent stream transforms", "[fe runner]") {
  using namespace gigaedge::enhance;
  const auto sample_rate = GENERATE(8000u, 48000u);
  const bool hybrid = GENERATE(false, true);
  const std::string model_type = hybrid ? "denoiser_hybrid_web_model" : "denoiser_discr_web_model";
  auto config = std::make_shared<DenoiserConfig>(StringConfigReader(
      "model:\n  model_path: unused.onnx\n"
      "feature_params:\n  sampling_freq: " +
      std::to_string(sample_rate) +
      "\n  frame_size_ms: 32\n  frame_stride_ms: 16\n  window_type: hanning\n"
      "inference:\n  model_type: " +
      model_type + "\n"));
  FeatureExtractorRunner runner(config);

  struct Result {
    std::vector<ComplexNumber<float>> spectra;
    std::vector<std::vector<float>> audio;
  };
  auto process = [&](Context<float>& context, uint32_t stream) {
    Result result;
    for (uint32_t chunk = 0; chunk < 24; ++chunk) {
      const uint32_t frames = 1 + (stream + chunk) % 3;
      Task task;
      task.data.context = &context;
      task.data.waveform.resize(runner.get_frame_size_samples() + (frames - 1) * runner.get_frame_hop_size_samples());
      for (size_t i = 0; i < task.data.waveform.size(); ++i) {
        const float phase = static_cast<float>(i + chunk * runner.get_frame_hop_size_samples());
        task.data.waveform[i] = 0.1f * std::sin(phase * 0.01f * static_cast<float>(stream + 1));
      }
      runner.run(&task);
      result.spectra.insert(result.spectra.end(), task.data.input_spectrogram.begin(),
                            task.data.input_spectrogram.end());

      const auto bins = static_cast<uint32_t>(runner.get_pow_spec_feat_size_samples());
      task.data.new_spectrogram = hybrid ? ComplexNumber<float>::concat_parts(task.data.input_spectrogram)
                                         : ComplexNumber<float>(0.2f, bins * frames, 0.1f, bins * frames);
      task.state.advance_to(PipelineStage::model_output_ready);
      runner.run(&task);
      result.audio.push_back(std::move(task.data.enhanced_wav));
    }
    return result;
  };

  constexpr uint32_t streams = 8;
  std::array<Result, streams> expected;
  for (uint32_t stream = 0; stream < streams; ++stream) {
    Context<float> context;
    expected[stream] = process(context, stream);
  }

  std::array<Context<float>, streams> contexts;
  std::array<fe::FeatureWorkspace<float>*, streams> workspaces;
  std::array<std::future<Result>, streams> pending;
  std::promise<void> ready;
  const auto start = ready.get_future().share();
  for (uint32_t stream = 0; stream < streams; ++stream) {
    runner.initialize_context(contexts[stream]);
    workspaces[stream] = contexts[stream].feature_workspace.get();
    for (uint32_t previous = 0; previous < stream; ++previous) {
      REQUIRE(workspaces[stream] != workspaces[previous]);
    }
  }
  for (uint32_t stream = 0; stream < streams; ++stream) {
    pending[stream] = std::async(std::launch::async, [&, stream, start] {
      start.wait();
      return process(contexts[stream], stream);
    });
  }
  ready.set_value();
  for (uint32_t stream = 0; stream < streams; ++stream) {
    const auto actual = pending[stream].get();
    REQUIRE(contexts[stream].feature_workspace.get() == workspaces[stream]);
    REQUIRE(actual.spectra.size() == expected[stream].spectra.size());
    for (size_t frame = 0; frame < actual.spectra.size(); ++frame) {
      const auto& a = actual.spectra[frame];
      const auto& e = expected[stream].spectra[frame];
      REQUIRE(a.size() == e.size());
      CHECK(std::equal(std::begin(a.real), std::end(a.real), std::begin(e.real)));
      CHECK(std::equal(std::begin(a.imag), std::end(a.imag), std::begin(e.imag)));
    }
    CHECK(actual.audio == expected[stream].audio);
  }
}

TEST_CASE("FAIL Feature runner rejects a missing stream context", "[fe runner]") {
  auto test_data = std::move(TestFERunner().InitFERunner("test_configuration_with_no_default_values.yaml"));
  gigaedge::enhance::Task task;
  REQUIRE_THROWS_AS(test_data.fe_runner_ptr->run(&task), std::invalid_argument);
}
