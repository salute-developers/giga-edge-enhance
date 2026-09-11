#include <spdlog/spdlog.h>

#include <catch.hpp>
#include <future>  // NOLINT
#include <string>
#include <vector>

#include "spench/src/inference_runner.h"
#include "spench/util/tflite/tflite_runner.h"
#include "tests/src/utils.h"

struct TestTfliteRunner {
  std::unique_ptr<util::tf_lite::BaseTfliteRunner> denoiser_runner;

  const std::string data_root = "data/";

 public:
  uint16_t frame_size_ms = 16;
  uint64_t sample_rate = 16000;
  TestTfliteRunner() = default;

  TestTfliteRunner& InitRunner(const std::string& yaml_config_name, uint16_t num_threads = 1) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    auto config = std::make_shared<spench::DenoiserConfig>(spench::FileConfigReader(file_path));
    std::string model_type = config->model_type();

    denoiser_runner = (model_type == "best_quality")
                          ? std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                  new util::tf_lite::SpenchQTfliteRunner(config, num_threads))
                           : std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                  new util::tf_lite::SpenchFTfliteRunner(config, num_threads));
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
  const auto config = std::make_shared<spench::DenoiserConfig>(
      spench::StringConfigReader("model:\n  model_path: unused.tflite\n"
                                 "inference:\n  model_type: best_speed_new\n"));
  CHECK_THROWS_WITH(spench::InferenceRunner(config, 1, false),
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
  CHECK_THROWS_WITH(TestTfliteRunner().InitRunner("test_configuration_with_corrupted_fast_tflite_24khz.yaml"),
                    "Model reading error. There is problem with checkpoint file.");
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 48kHz and with different number of threads [tflite]") {
  const std::size_t nattempts = 100;

  for (uint16_t num_threads : {1, 2, 4, 8}) {
    auto data = std::move(
        TestTfliteRunner().InitRunner("default_configuration_best_quality_tflite_48khz.yaml", num_threads));

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
  auto data = std::move(TestTfliteRunner().InitRunner("default_configuration_best_quality_tflite_48khz.yaml"));

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

// TEST_CASE(
//  "SUCCEED Run inference via the best quality model and check network outputs "
//  "(audio 16kHz) [tflite runner]")
// {
//   auto data = std::move(TestTfliteRunner()
//       .InitRunner("default_configuration_best_quality_tflite_16khz.yaml"));

//   std::valarray<float> real;
//   std::valarray<float> imag;
//   std::vector<float> gt_real;
//   std::vector<float> gt_imag;
//   std::vector<float> gt_fullband_hidden;
//   std::vector<float> gt_fullband_cell_state;
//   std::vector<float> gt_subband_hidden;
//   std::vector<float> gt_subband_cell_state;
//   data.LoadDataFromBin("1_real_wav_stft.dump", real);
//   data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
//   data.LoadDataFromBin("1_real_network_output_for_zeroed_hiddens_16khz.dump", gt_real);
//   data.LoadDataFromBin("1_imag_network_output_for_zeroed_hiddens_16khz.dump", gt_imag);
//   data.LoadDataFromBin("1_predicted_full_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_fullband_hidden);
//   data.LoadDataFromBin("1_predicted_full_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_fullband_cell_state);
//   data.LoadDataFromBin("1_predicted_sub_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_subband_hidden);
//   data.LoadDataFromBin("1_predicted_sub_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_subband_cell_state);

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
//   ctx.get()->fullband_hidden.resize(data.denoiser_runner->input_tensor_sizes[1], 0);
//   ctx.get()->fullband_cell_state.resize(data.denoiser_runner->input_tensor_sizes[2], 0);
//   ctx.get()->subband_hidden.resize(data.denoiser_runner->input_tensor_sizes[3], 0);
//   ctx.get()->subband_cell_state.resize(data.denoiser_runner->input_tensor_sizes[4], 0);

//   denoise_task.data.context = ctx.get();
//   denoise_task.data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));
//   denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;

//   data.denoiser_runner->infer(&denoise_task);

//   REQUIRE(denoise_task.stage == spench::PipelineStage::skipped_chunk);
//   IsVectorsClose(gt_real, denoise_task.data.new_spectrogram.real, 0.000001);

//   IsVectorsClose(gt_imag, denoise_task.data.new_spectrogram.imag, 0.000001);
//   CHECK(std::vector<float>(denoise_task.data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.data.enhanced_wav);
//   IsVectorsClose(gt_fullband_hidden, ctx.get()->fullband_hidden, 0.000001);
//   IsVectorsClose(gt_fullband_cell_state, ctx.get()->fullband_cell_state, 0.000001);
//   IsVectorsClose(gt_subband_hidden, ctx.get()->subband_hidden, 0.00001);
//   IsVectorsClose(gt_subband_cell_state, ctx.get()->subband_cell_state, 0.00001);
// }

// TEST_CASE("SUCCEED Run four frames with the best quality model and check states (audio 16kHz) [tflite runner]") {
//   auto data = std::move(TestTfliteRunner()
//       .InitRunner("default_configuration_best_quality_tflite_16khz.yaml"));

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
//   ctx.get()->fullband_hidden.resize(data.denoiser_runner->input_tensor_sizes[1], 0);
//   ctx.get()->fullband_cell_state.resize(data.denoiser_runner->input_tensor_sizes[2], 0);
//   ctx.get()->subband_hidden.resize(data.denoiser_runner->input_tensor_sizes[3], 0);
//   ctx.get()->subband_cell_state.resize(data.denoiser_runner->input_tensor_sizes[4], 0);
//   denoise_task.data.context = ctx.get();

//   std::valarray<float> real;
//   std::valarray<float> imag;
//   data.LoadDataFromBin("1_real_wav_stft.dump", real);
//   data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
//   denoise_task.data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));
//   denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;
//   uint64_t hop_size = static_cast<uint64_t>(data.sample_rate * data.frame_size_ms / 1000);
//   denoise_task.data.enhanced_wav.assign(hop_size, 1.0);

//   data.denoiser_runner->infer(&denoise_task);
//   REQUIRE(denoise_task.stage == spench::PipelineStage::skipped_chunk);

//   CHECK(std::vector<float>(denoise_task.data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.data.enhanced_wav);
//   CHECK(std::vector<float>(ctx.get()->fullband_hidden.size(), 0.0) !=
//                                                  ctx.get()->fullband_hidden);
//   CHECK(std::vector<float>(ctx.get()->fullband_cell_state.size(), 0.0) !=
//                                                  ctx.get()->fullband_cell_state);
//   CHECK(std::vector<float>(ctx.get()->subband_hidden.size(), 0.0) !=
//                                                  ctx.get()->subband_hidden);
//   CHECK(std::vector<float>(ctx.get()->subband_cell_state.size(), 0.0) !=
//                                                  ctx.get()->subband_cell_state);

//   denoise_task.data.enhanced_wav.assign(hop_size, 1.0);
//   denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;

//   data.denoiser_runner->infer(&denoise_task);
//   REQUIRE(denoise_task.stage == spench::PipelineStage::skipped_chunk);
//   CHECK(std::vector<float>(denoise_task.data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.data.enhanced_wav);
//   denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;
//   denoise_task.data.enhanced_wav.assign(hop_size, 1.0);

//   data.denoiser_runner->infer(&denoise_task);
//   REQUIRE(denoise_task.stage == spench::PipelineStage::predicted_mask);
//   CHECK(std::vector<float>(denoise_task.data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.data.enhanced_wav);

//   denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;
//   denoise_task.data.enhanced_wav.assign(hop_size, 1.0);

//   data.denoiser_runner->infer(&denoise_task);
//   REQUIRE(denoise_task.stage == spench::PipelineStage::predicted_mask);
//   CHECK(std::vector<float>(denoise_task.data.enhanced_wav.size(), 1.0) ==
//                                                  denoise_task.data.enhanced_wav);
// }

// TEST_CASE(
//  "SUCCEED Run dummy multi inference for a quality model 16kHz and with "
//  "different number of threads [tflite]")
// {
//   const std::size_t nattempts = 50;

//   for (uint16_t num_threads : {1, 2, 4, 8}) {
//     auto data = std::move(TestTfliteRunner()
//       .InitRunner("default_configuration_best_quality_tflite_16khz.yaml", num_threads));

//     std::vector<size_t> timings(nattempts);
//     std::vector<std::vector<float>*> inputs;
//     for (auto s : data.denoiser_runner->input_tensor_sizes) {
//       inputs.push_back(new std::vector<float>(s, static_cast<float>(0.)));
//     }

//     auto ts_1 = std::chrono::steady_clock::now();
//     for (size_t i = 0; i < nattempts; ++i) {
//       auto start = std::chrono::steady_clock::now();
//       data.denoiser_runner->test_run<float>(inputs);
//       auto stop = std::chrono::steady_clock::now();
//       timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
//     }
//     auto ts_2 = std::chrono::steady_clock::now();

//     auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
//     spdlog::info("24kHz quality model infer with  {} threads completed: {} | {} ms per infer",
//                num_threads,
//                tot,
//                static_cast<double>(tot) / nattempts);

//     for (uint16_t i = 0; i < data.denoiser_runner->input_tensor_sizes.size(); i++) {
//       delete inputs[i];
//     }
//   }
// }
