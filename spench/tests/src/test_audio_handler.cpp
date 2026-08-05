#include <spench/errors.h>

#include <catch.hpp>
#include <filesystem>

#include "spench/src/denoiser_model_adviser.h"
#include "spench/src/handler/denoiser_audio_handler.h"
#include "tests/src/utils.h"

struct TestHandler {
  std::unique_ptr<spench::NeuralNetworkAudioHandler> handler;

  const std::string data_root = "data/";
  std::vector<float> wav_data;

 public:
  TestHandler() : handler(nullptr) {}
  TestHandler& operator=(const TestHandler&) = delete;
  TestHandler(const TestHandler&) = delete;
  TestHandler(TestHandler&& orig) : handler(std::move(orig.handler)), wav_data(std::move(orig.wav_data)) {
    orig.wav_data.clear();
    orig.handler = nullptr;
  }

  template <class Handler, class NetReader>
  TestHandler& InitHandlerFromFile(const std::filesystem::path& rel_cfg) {
    return initWith<Handler, NetReader, spench::FileConfigReader>(cfgPath(rel_cfg));
  }

  template <class Handler, class NetReader>
  TestHandler& InitHandlerFromYaml(std::string yaml_config) {
    return initWith<Handler, NetReader, spench::StringConfigReader>(std::move(yaml_config));
  }

  TestHandler& SetWav(const std::string& wav_path) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_data);
    return *this;
  }

  TestHandler& LoadDataFromBin(const std::string& dump, uint32_t step_size_samples,
                               std::vector<std::vector<float>>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }

  TestHandler& LoadDataFromBin(const std::string& dump, std::vector<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }

  TestHandler& LoadDataFromBin(const std::string& dump, std::valarray<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }

  spench::DenoiserModelAdviser<float>* GetAdviser() {
    if (auto* denoiser_handler = dynamic_cast<spench::DenoiserAudioHandler*>(handler.get())) {
      return denoiser_handler->adviser();
    } else {
      return nullptr;
    }
  }

 private:
  template <class Handler, class NetReader, class Reader, class... Args>
  TestHandler& initWith(Args&&... args) {
    static_assert(std::is_base_of_v<spench::NeuralNetworkAudioHandler, Handler>,
                  "Handler must derive from NeuralNetworkAudioHandler");
    handler = std::make_unique<Handler>(std::make_shared<NetReader>(Reader(std::forward<Args>(args)...)));
    return *this;
  }

  [[nodiscard]] std::string cfgPath(const std::filesystem::path& rel) const {
    std::filesystem::path base{data_root};
    return (base / "config" / rel).string();
  }
};

TEST_CASE("FAIL Run denoiser handler with exception from fe runner [denoiser handler]") {
  REQUIRE_THROWS_WITH((TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
                          "test_configuration_with_incorrect_sampling_freq.yaml")),
                      "Config reading error. Sampling frequency is not a scalar!");
}

TEST_CASE("FAIL Run denoiser handler with exception from inference runner [denoiser handler]") {
  REQUIRE_THROWS_WITH((TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
                          "test_configuration_with_incorrect_model_path.yaml")),
                      "Config reading error. Model path parameter is not a single string!");
}

TEST_CASE("FAIL Run denoiser handler with exception for incorrect config path [denoiser handler]") {
  REQUIRE_THROWS_WITH(
      (TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>("config.json")),
      "Config file load error: bad file: data/config/config.json");
}

TEST_CASE("SUCCEED Create frame [denoiser handler]") {
  std::vector<float> input_chunk = {249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> gt_reversed_chunk = {0,     255.0, 254.0, 253.0, 252.0, 251.0, 250.0,
                                          249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> reversed_chunk;
  spench::tools::prepare_first_input_frame(input_chunk, reversed_chunk, input_chunk.size());
  IsVectorsClose(reversed_chunk, gt_reversed_chunk, 1.0);
}

TEST_CASE("FAIL Run denoiser handler with error for small input length [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  spench::SpenchContext<float>* ctx = scheduler_test_env.handler->create_context();

  const std::vector<float> input(128, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = scheduler_test_env.handler->process_frame(ctx, input, output);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
  scheduler_test_env.handler->destroy_context(&ctx);
}

TEST_CASE("FAIL Run denoiser handler with error for incorrect input length [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  spench::SpenchContext<float>* ctx = scheduler_test_env.handler->create_context();

  const std::vector<float> input(800, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = scheduler_test_env.handler->process_frame(ctx, input, output);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
  scheduler_test_env.handler->destroy_context(&ctx);
}

// TEST_CASE("SUCCEED Change number of engine threads [denoiser handler]") {
// #ifdef BUILD_TFLITE_ENGINE
//   auto scheduler_test_env = std::move(
//       TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler,
//                                         spench::DenoiserConfig>(
//           "default_configuration_best_quality_tflite_16khz.yaml"));
// #else
//   auto scheduler_test_env = std::move(
//       TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler,
//                                         spench::DenoiserConfig>(
//           "default_configuration_best_quality_onnx_16khz.yaml"));
// #endif

//   const uint32_t default_thread_count = 4;
//   CHECK(default_thread_count == scheduler_test_env.handler->get_engine_threads_num());
//   REQUIRE_NOTHROW(scheduler_test_env.handler->reload_config());
// }

#ifdef BUILD_ONNX_ENGINE
TEST_CASE("SUCCEED Run denoiser 48kHz model with yaml string config [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  const char* model_path = "data/models/quality_model_48khz.tflite";
#else
  const char* model_path = "data/models/quality_model_48khz.onnx";
#endif
  const std::string yaml_conf =
      std::string(
          "model:\n"
          " model_path: ") +
      model_path +
      "\n"
      " model_inputs: [noisy_mag_in, prev_fb_hidden_in,"
      " prev_fb_snr_noise_psd_in, prev_fb_snr_noise_pg_in,"
      " prev_sb_snr_noise_psd_in, prev_sb_snr_noise_pg_in,"
      " prev_fb_conv1_buf_in, prev_fb_conv3_buf_in]\n"
      " model_outputs: [cur_fb_hidden_out, cur_fb_snr_noise_psd_out,"
      " cur_fb_snr_noise_pg_out, cur_sb_snr_noise_psd_out,"
      " cur_sb_snr_noise_pg_out, cur_fb_conv1_buf_out, cur_fb_conv3_buf_out, predicted_mask_out]\n"
      "\n"
      "feature_params:\n"
      " sampling_freq: 48000\n"
      " frame_size_ms: 32\n"
      " frame_stride_ms: 16\n"
      " window_type: hanning\n"
      "\n"
      "inference:\n"
      " gpu: false\n"
      " engine_num_threads: 1\n"
      " model_type: best_quality\n";

  TestHandler scheduler_test_env;
  REQUIRE_NOTHROW(scheduler_test_env.InitHandlerFromYaml<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
      std::string(yaml_conf)));
}

TEST_CASE(
    "SUCCEED Check finalize chunk for the best quality model "
    "[denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  spench::SpenchContext<float>* ctx = scheduler_test_env.handler->create_context();
  uint16_t input_frame_size = ctx->input_sound_stream_ptr->input.frame_size_samples();
  std::vector<float> output;

  SpenchRetCode ret_code = scheduler_test_env.handler->finalize_inference(ctx, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  REQUIRE(input_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(input_frame_size * 1, 0.0) == output);
  scheduler_test_env.handler->destroy_context(&ctx);
}

TEST_CASE("SUCCEED Run denoise and check finalize a chunk for the best quality model [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  spench::SpenchContext<float>* ctx = scheduler_test_env.handler->create_context();
  spench::DenoiseContextQ<float>* ctx_q = dynamic_cast<spench::DenoiseContextQ<float>*>(ctx);
  uint16_t input_frame_size = ctx->input_sound_stream_ptr->input.frame_size_samples();
  const std::vector<float> input1(input_frame_size, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = scheduler_test_env.handler->process_frame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = scheduler_test_env.handler->process_frame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = scheduler_test_env.handler->process_frame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  output.resize(0);

  ret_code = scheduler_test_env.handler->finalize_inference(ctx_q, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(input_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(input_frame_size * 1, 0.0) != output);
  scheduler_test_env.handler->destroy_context(&ctx);
}

TEST_CASE("SUCCEED Run denoise and check finalize chunk for the fast model [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::SpenchContext<float>* ctx = scheduler_test_env.handler->create_context();
  spench::DenoiseContextF<float>* ctx_f = dynamic_cast<spench::DenoiseContextF<float>*>(ctx);
  constexpr uint16_t input_frame_size = 384;
  const std::vector<float> input1(input_frame_size, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  output.resize(0);

  ret_code = scheduler_test_env.handler->finalize_inference(ctx_f, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(input_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(input_frame_size * 1, 0.0) != output);
  scheduler_test_env.handler->destroy_context(&ctx);
}
#endif

TEST_CASE("SUCCEED Run denoise and check result with yaml string config [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  static constexpr char embedded_yaml[] = R"(
model:
  model_path: data/models/fast_model_24khz.tflite)";
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromYaml<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          std::string(embedded_yaml)));
#else
  static constexpr char embedded_yaml[] = R"(
model:
  model_path: data/models/fast_model_24khz.onnx)";
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromYaml<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          std::string(embedded_yaml)));
#endif

  spench::SpenchContext<float>* ctx = scheduler_test_env.handler->create_context();
  spench::DenoiseContextF<float>* ctx_f = dynamic_cast<spench::DenoiseContextF<float>*>(ctx);
  constexpr uint16_t input_frame_size = 384;
  const std::vector<float> input1(input_frame_size, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  ret_code = scheduler_test_env.handler->process_frame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  output.resize(0);

  ret_code = scheduler_test_env.handler->finalize_inference(ctx_f, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(input_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(input_frame_size * 1, 0.0) != output);
  scheduler_test_env.handler->destroy_context(&ctx);
}

TEST_CASE("SUCCEED Run calc_snr_estimation with one chunk [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  std::valarray<float> test_noisy_real;
  std::valarray<float> test_noisy_imag;
  std::valarray<float> test_enhanced_real;
  std::valarray<float> test_enhanced_imag;
  spench::StreamCurrentMetrics<float> metrics;

  scheduler_test_env.LoadDataFromBin("calc_snr_enhanced_real.dump", test_enhanced_real);
  scheduler_test_env.LoadDataFromBin("calc_snr_enhanced_imag.dump", test_enhanced_imag);
  scheduler_test_env.LoadDataFromBin("calc_snr_noisy_real.dump", test_noisy_real);
  scheduler_test_env.LoadDataFromBin("calc_snr_noisy_imag.dump", test_noisy_imag);

  spench::ComplexNumber<float> test_noisy_mag(test_noisy_real, test_noisy_imag);
  spench::ComplexNumber<float> test_enhanced_mag(test_enhanced_real, test_enhanced_imag);
  std::vector<float> gt_test_snr_result;

  scheduler_test_env.LoadDataFromBin("calc_snr_result.dump", gt_test_snr_result);
  float test_snr_result =
      scheduler_test_env.GetAdviser()->calc_snr_estimation(test_noisy_mag, test_enhanced_mag, metrics);

  IsVectorsClose(std::vector{test_snr_result}, std::vector{gt_test_snr_result[0]}, 0.001);
}

TEST_CASE("SUCCEED Run calc_snr_estimation with dumped recording [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  std::valarray<float> test_noisy_real;
  std::valarray<float> test_noisy_imag;
  std::valarray<float> test_enhanced_real;
  std::valarray<float> test_enhanced_imag;
  spench::StreamCurrentMetrics<float> metrics;

  scheduler_test_env.LoadDataFromBin("calc_snr_enhanced_real.dump", test_enhanced_real);
  scheduler_test_env.LoadDataFromBin("calc_snr_enhanced_imag.dump", test_enhanced_imag);
  scheduler_test_env.LoadDataFromBin("calc_snr_noisy_real.dump", test_noisy_real);
  scheduler_test_env.LoadDataFromBin("calc_snr_noisy_imag.dump", test_noisy_imag);

  std::vector<float> test_snr_result;
  scheduler_test_env.LoadDataFromBin("calc_snr_result.dump", test_snr_result);

  for (uint32_t i = 0; i < test_enhanced_real.size(); ++i) {
    spench::ComplexNumber<float> test_noisy_mag(test_noisy_real[0], 1, test_noisy_imag[0], 1);
    spench::ComplexNumber<float> test_enhanced_mag(test_enhanced_real[0], 1, test_enhanced_imag[0], 1);
    test_snr_result.emplace_back(
        scheduler_test_env.GetAdviser()->calc_snr_estimation(test_noisy_mag, test_enhanced_mag, metrics));
  }

  IsVectorsClose(test_snr_result, test_snr_result, 0.000001);
}

TEST_CASE("SUCCEED Run calc_snr_estimation with zero recording [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  std::valarray<float> test_noisy_real = {0};
  std::valarray<float> test_noisy_imag = {0};
  std::valarray<float> test_enhanced_real = {0};
  std::valarray<float> test_enhanced_imag = {0};
  spench::StreamCurrentMetrics<float> metrics;
  std::vector<float> test_snr_result = {0};

  for (uint32_t i = 0; i < test_enhanced_real.size(); ++i) {
    spench::ComplexNumber<float> test_noisy_mag(test_noisy_real[0], 1, test_noisy_imag[0], 1);
    spench::ComplexNumber<float> test_enhanced_mag(test_enhanced_real[0], 1, test_enhanced_imag[0], 1);
    test_snr_result.emplace_back(
        scheduler_test_env.GetAdviser()->calc_snr_estimation(test_noisy_mag, test_enhanced_mag, metrics));
  }

  IsVectorsClose(test_snr_result, test_snr_result, 0.000001);
}

TEST_CASE("SUCCEED Run calc_snr_estimation with empty recording [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  std::valarray<float> test_noisy_real = {};
  std::valarray<float> test_noisy_imag = {};
  std::valarray<float> test_enhanced_real = {};
  std::valarray<float> test_enhanced_imag = {};
  spench::StreamCurrentMetrics<float> metrics;
  std::vector<float> test_snr_result = {};

  for (uint32_t i = 0; i < test_enhanced_real.size(); ++i) {
    spench::ComplexNumber<float> test_noisy_mag(test_noisy_real[0], 1, test_noisy_imag[0], 1);
    spench::ComplexNumber<float> test_enhanced_mag(test_enhanced_real[0], 1, test_enhanced_imag[0], 1);
    test_snr_result.emplace_back(
        scheduler_test_env.GetAdviser()->calc_snr_estimation(test_noisy_mag, test_enhanced_mag, metrics));
  }

  IsVectorsClose(test_snr_result, test_snr_result, 0.000001);
}

TEST_CASE("FAIL calc_snr_estimation with overflow values [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  float big_val = 1e38f;
  std::valarray<float> test_real(big_val, 10);
  std::valarray<float> test_imag(big_val, 10);

  spench::ComplexNumber<float> spec(test_real, test_imag);
  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calc_snr_estimation with NaN in input [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> test_real(1.0f, 10);
  test_real[3] = std::nanf("");
  std::valarray<float> test_imag(0.0f, 10);

  spench::ComplexNumber<float> spec(test_real, test_imag);
  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calc_snr_estimation with mismatched input sizes [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> real(1.0f, 10);
  std::valarray<float> imag(1.0f, 3);

  spench::ComplexNumber<float> spec(real, imag);
  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calc_snr_estimation with mostly zeros [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> real(0.0f, 100);
  real[0] = 1e-10f;
  std::valarray<float> imag = real;

  spench::ComplexNumber<float> input_spec(real, imag);
  spench::ComplexNumber<float> enhanced_spec(real, imag);
  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(input_spec, enhanced_spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calc_snr_estimation with empty complex numbers [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> empty_real{};
  std::valarray<float> empty_imag{};

  spench::ComplexNumber<float> spec(empty_real, empty_imag);
  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calc_snr_estimation with mismatched real/imag sizes [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> noisy_real(1.0f, 8);
  std::valarray<float> noisy_imag(1.0f, 10);

  std::valarray<float> enhanced_real(1.0f, 10);
  std::valarray<float> enhanced_imag(1.0f, 10);

  spench::ComplexNumber<float> input_spec(noisy_real, noisy_imag);
  spench::ComplexNumber<float> enhanced_spec(enhanced_real, enhanced_imag);

  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(input_spec, enhanced_spec, metrics);

  CHECK(snr == 0.0f);
}

TEST_CASE("SUCCEED calc_snr_estimation with exit from practical range [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> noisy_real(0.01f, 10);
  std::valarray<float> noisy_imag(0.01f, 10);

  std::valarray<float> enhanced_real(10000.0f, 10);
  std::valarray<float> enhanced_imag(10000.0f, 10);

  spench::ComplexNumber<float> input_spec(noisy_real, noisy_imag);
  spench::ComplexNumber<float> enhanced_spec(enhanced_real, enhanced_imag);

  float snr = scheduler_test_env.GetAdviser()->calc_snr_estimation(input_spec, enhanced_spec, metrics);

  CHECK(snr == 100.0f);
}

TEST_CASE("FAIL average_snr_db with no input values [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_tflite_24khz.yaml"));
#else
  auto scheduler_test_env =
      std::move(TestHandler().InitHandlerFromFile<spench::DenoiserAudioHandler, spench::DenoiserConfig>(
          "default_configuration_fast_onnx_24khz.yaml"));
#endif

  float avg = scheduler_test_env.GetAdviser()->average_snr_db();

  CHECK(std::isnan(avg));
}
