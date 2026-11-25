#include <spdlog/spdlog.h>

#include <catch.hpp>
#include <future>  // NOLINT
#include <string>
#include <vector>

#include "src/inference_runner.h"
#include "tests/src/utils.h"
#include "util/tflite/tflite_runner.h"

struct TestTfliteRunner {
  std::unique_ptr<util::tf_lite::BaseTfliteRunner> denoiser_runner;

  const std::string data_root = "data/";

 public:
  uint16_t frame_size_ms = 16;
  uint64_t sample_rate = 16000;
  TestTfliteRunner() = default;

  TestTfliteRunner& InitRunner(const std::string& yaml_config_name, uint16_t num_threads = 1) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    auto config = std::make_shared<giga_edge::enhance::DenoiserConfig>(giga_edge::enhance::FileConfigReader(file_path));

    denoiser_runner = std::make_unique<util::tf_lite::DenoiserDiscriminativeHqTfliteRunner>(config, num_threads);
    frame_size_ms = config->frame_size_ms();
    sample_rate = config->sampling_freq();
    return *this;
  }

  TestTfliteRunner& LoadDataFromBin(const std::string& dump, std::vector<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestTfliteRunner& LoadDataFromBin(const std::string& dump, std::valarray<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }
};

TEST_CASE("FAIL TFLite rejects unsupported model type before inference [tflite]") {
  const auto config = std::make_shared<giga_edge::enhance::DenoiserConfig>(
      giga_edge::enhance::StringConfigReader("model:\n  model_path: unused.tflite\n"
                                             "inference:\n  model_type: denoiser_discr_hq_model\n"));
  CHECK_THROWS_WITH(giga_edge::enhance::InferenceRunner(config, 1, false),
                    "Model type is not supported by the selected inference engine.");
}

TEST_CASE("SUCCEED Run relu model dummy inference [tflite]") {
  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::BuildFromFile("data/models/dummy_relu_model.tflite");
  REQUIRE(model != nullptr);

  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*model, resolver);
  std::unique_ptr<tflite::Interpreter> interpreter;
  builder(&interpreter);
  REQUIRE(interpreter != nullptr);

  REQUIRE(interpreter->AllocateTensors() == kTfLiteOk);
  std::vector<float> input_vector{-1.3, 0.3, 1.0};
  std::vector<float> gt_vector{0, 0.3, 1.0};

  float* input_data_ptr = interpreter->typed_input_tensor<float>(0);
  for (auto const& value : input_vector) {
    *(input_data_ptr) = value;
    input_data_ptr++;
  }

  interpreter->Invoke();

  float* output_data_ptr = interpreter->typed_output_tensor<float>(0);
  std::vector<float> output;
  output.assign(output_data_ptr, output_data_ptr + input_vector.size());

  IsVectorsClose(output, gt_vector, 0.0000001);
}

TEST_CASE("FAIL Read config with corrupted model [tflite runner]") {
  auto config = std::make_shared<giga_edge::enhance::DenoiserConfig>(
      giga_edge::enhance::StringConfigReader("model:\n  model_path: data/models/invalid_model.tflite\n"));
  CHECK_THROWS_WITH(util::tf_lite::DenoiserDiscriminativeHqTfliteRunner(config, 1),
                    "Model reading error. There is problem with checkpoint file.");
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 48kHz and with different number of threads [tflite]") {
  const std::size_t nattempts = 100;

  for (uint16_t num_threads : {1, 2, 4, 8}) {
    auto data =
        std::move(TestTfliteRunner().InitRunner("default_configuration_discr_web_tflite_48khz.yaml", num_threads));

    std::vector<size_t> timings(nattempts);
    std::vector<std::vector<float>*> inputs;
    for (auto s : data.denoiser_runner->input_tensor_sizes) {
      inputs.push_back(new std::vector<float>(s, static_cast<float>(0.)));
    }

    auto ts_1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < nattempts; ++i) {
      auto start = std::chrono::steady_clock::now();
      data.denoiser_runner->test_run<float>(inputs);
      auto stop = std::chrono::steady_clock::now();
      timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
    }
    auto ts_2 = std::chrono::steady_clock::now();

    auto tot = std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();

    spdlog::info("48kHz quality model infer with {} threads completed: {} | {} ms per infer", num_threads, tot,
                 static_cast<double>(tot) / nattempts);

    for (uint16_t i = 0; i < data.denoiser_runner->input_tensor_sizes.size(); i++) {
      delete inputs[i];
    }
  }
}

TEST_CASE("Shared TFLite runner serializes concurrent inference [tflite]") {
  auto data = std::move(TestTfliteRunner().InitRunner("default_configuration_discr_web_tflite_48khz.yaml"));

  auto run = [&data]() {
    std::vector<std::vector<float>> storage;
    std::vector<std::vector<float>*> inputs;
    storage.reserve(data.denoiser_runner->input_tensor_sizes.size());
    inputs.reserve(data.denoiser_runner->input_tensor_sizes.size());
    for (const uint64_t size : data.denoiser_runner->input_tensor_sizes) {
      storage.emplace_back(size, 0.0F);
      inputs.push_back(&storage.back());
    }
    for (size_t i = 0; i < 10; ++i) {
      data.denoiser_runner->test_run<float>(inputs);
    }
  };

  auto first = std::async(std::launch::async, run);
  auto second = std::async(std::launch::async, run);
  REQUIRE_NOTHROW(first.get());
  REQUIRE_NOTHROW(second.get());
}
