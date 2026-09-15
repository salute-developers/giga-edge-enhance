#include <gigadenoiser/errors.h>

#include <catch.hpp>
#include <cmath>
#include <filesystem>
#include <limits>

#include "src/handler/denoiser_audio_handler.h"
#include "src/inference_runner.h"
#include "tests/src/utils.h"

struct TestHandler {
  std::unique_ptr<gigaedge::denoiser::NeuralNetworkAudioHandler> handler;

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
    return initWith<Handler, NetReader, gigaedge::denoiser::FileConfigReader>(cfgPath(rel_cfg));
  }

  template <class Handler, class NetReader>
  TestHandler& InitHandlerFromYaml(std::string yaml_config) {
    return initWith<Handler, NetReader, gigaedge::denoiser::StringConfigReader>(std::move(yaml_config));
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

 private:
  template <class Handler, class NetReader, class Reader, class... Args>
  TestHandler& initWith(Args&&... args) {
    static_assert(std::is_base_of_v<gigaedge::denoiser::NeuralNetworkAudioHandler, Handler>,
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
  REQUIRE_THROWS_WITH(
      (TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "test_configuration_with_incorrect_sampling_freq.yaml")),
      "Config reading error. Sampling frequency is not a scalar!");
}

TEST_CASE("FAIL Run denoiser handler with exception from inference runner [denoiser handler]") {
  REQUIRE_THROWS_WITH(
      (TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "test_configuration_with_incorrect_model_path.yaml")),
      "Config reading error. Model path parameter is not a single string!");
}

TEST_CASE("FAIL Run denoiser handler with exception for incorrect config path [denoiser handler]") {
  REQUIRE_THROWS_WITH(
      (TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "config.json")),
      "Config file load error: bad file: data/config/config.json");
}

TEST_CASE("SUCCEED Create frame [denoiser handler]") {
  std::vector<float> input_chunk = {249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> gt_reversed_chunk = {0,     255.0, 254.0, 253.0, 252.0, 251.0, 250.0,
                                          249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> reversed_chunk;
  gigaedge::denoiser::tools::prepare_first_input_frame(input_chunk, reversed_chunk, input_chunk.size());
  IsVectorsClose(reversed_chunk, gt_reversed_chunk, 1.0);
}

TEST_CASE("FAIL Run denoiser handler with error for small input length [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  gigaedge::denoiser::Context<float>* ctx = scheduler_test_env.handler->create_context();

  const std::vector<float> input(128, 0.5);
  std::vector<float> output{42.0F};

  GigaDenoiserRetCode ret_code = scheduler_test_env.handler->process_frame(ctx, input, output);
  CHECK(ret_code == GigaDenoiserRetCode::ERR_INVALID_FRAME_SIZE);
  CHECK(output.empty());
  scheduler_test_env.handler->destroy_context(&ctx);
}

TEST_CASE("FAIL Run denoiser handler with error for incorrect input length [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  gigaedge::denoiser::Context<float>* ctx = scheduler_test_env.handler->create_context();

  const std::vector<float> input(800, 0.5);
  std::vector<float> output{42.0F};

  GigaDenoiserRetCode ret_code = scheduler_test_env.handler->process_frame(ctx, input, output);
  CHECK(ret_code == GigaDenoiserRetCode::ERR_INVALID_FRAME_SIZE);
  CHECK(output.empty());
  scheduler_test_env.handler->destroy_context(&ctx);
}

// TEST_CASE("SUCCEED Change number of engine threads [denoiser handler]") {
// #ifdef BUILD_TFLITE_ENGINE
//   auto scheduler_test_env = std::move(
//       TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler,
//                                         gigaedge::denoiser::DenoiserConfig>(
//           "default_configuration_best_quality_tflite_16khz.yaml"));
// #else
//   auto scheduler_test_env = std::move(
//       TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler,
//                                         gigaedge::denoiser::DenoiserConfig>(
//           "default_configuration_best_quality_onnx_16khz.yaml"));
// #endif

//   const uint32_t default_thread_count = 4;
//   CHECK(default_thread_count == scheduler_test_env.handler->get_engine_threads_num());
//   REQUIRE_NOTHROW(scheduler_test_env.handler->reload_config());
// }

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
      " model_type: denoiser_discr_web_model\n";

  TestHandler scheduler_test_env;
  REQUIRE_NOTHROW(scheduler_test_env.InitHandlerFromYaml<gigaedge::denoiser::DenoiserAudioHandler,
                                                         gigaedge::denoiser::DenoiserConfig>(std::string(yaml_conf)));
}

TEST_CASE(
    "SUCCEED Check finalize chunk for the best quality model "
    "[denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  gigaedge::denoiser::Context<float>* ctx = scheduler_test_env.handler->create_context();
  uint16_t input_frame_size = ctx->input_sound_stream_ptr->input.frame_size_samples();
  std::vector<float> output;

  GigaDenoiserRetCode ret_code = scheduler_test_env.handler->finalize_inference(ctx, output);
  CHECK(ret_code == GigaDenoiserRetCode::CONTEXT_ACCUMULATION);

  REQUIRE(input_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(input_frame_size * 1, 0.0) == output);
  scheduler_test_env.handler->destroy_context(&ctx);
}

TEST_CASE("SUCCEED Run denoise and check finalize a chunk for the best quality model [denoiser handler]") {
#ifdef BUILD_TFLITE_ENGINE
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_tflite_48khz.yaml"));
#else
  auto scheduler_test_env = std::move(
      TestHandler().InitHandlerFromFile<gigaedge::denoiser::DenoiserAudioHandler, gigaedge::denoiser::DenoiserConfig>(
          "default_configuration_best_quality_onnx_48khz.yaml"));
#endif

  gigaedge::denoiser::Context<float>* ctx = scheduler_test_env.handler->create_context();
  gigaedge::denoiser::DenoiseContextDiscriminativeHq<float>* ctx_q =
      dynamic_cast<gigaedge::denoiser::DenoiseContextDiscriminativeHq<float>*>(ctx);
  uint16_t input_frame_size = ctx->input_sound_stream_ptr->input.frame_size_samples();
  const std::vector<float> input1(input_frame_size, 0.5);
  std::vector<float> output;

  GigaDenoiserRetCode ret_code = scheduler_test_env.handler->process_frame(ctx_q, input1, output);
  CHECK(ret_code == GigaDenoiserRetCode::CONTEXT_ACCUMULATION);

  ret_code = scheduler_test_env.handler->process_frame(ctx_q, input1, output);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = scheduler_test_env.handler->process_frame(ctx_q, input1, output);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
  CHECK(output.size() == input_frame_size);
  REQUIRE(std::vector<float>(input_frame_size, 0.0) != output);

  output.resize(0);

  ret_code = scheduler_test_env.handler->finalize_inference(ctx_q, output);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  REQUIRE(input_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(input_frame_size * 1, 0.0) != output);
  scheduler_test_env.handler->destroy_context(&ctx);
}


TEST_CASE("Inference configuration rejects invalid model type and context length", "[validation]") {
  using namespace gigaedge::denoiser;
#ifdef BUILD_TFLITE_ENGINE
  const std::string model = "data/models/quality_model_48khz.tflite";
#else
  const std::string model = "data/models/quality_model_48khz.onnx";
#endif
  const std::string prefix = "model:\n  model_path: " + model + "\n";
  SECTION("unsupported model type") {
    auto config =
        std::make_shared<DenoiserConfig>(StringConfigReader(prefix + "inference:\n  model_type: unsupported\n"));
    REQUIRE_THROWS_AS(InferenceRunner(config, 1, false), std::invalid_argument);
  }
  SECTION("non-scalar context length") {
    REQUIRE_THROWS_WITH(DenoiserConfig(StringConfigReader(prefix + "  model_input_ctx_len: [1, 2]\n")),
                        "Config reading error. Model context length is not a scalar!");
  }
}

TEST_CASE("SNR adviser validates spectra and maintains its sliding window", "[snr]") {
  using namespace gigaedge::denoiser;
  DenoiserModelAdviser<float> adviser(48, 16);
  StreamCurrentMetrics<float> metrics;
  SECTION("window becomes available and drops old samples") {
    CHECK(std::isnan(adviser.average_snr_db()));
    adviser.add_value(3);
    adviser.add_value(6);
    CHECK(std::isnan(adviser.average_snr_db()));
    adviser.add_value(9);
    CHECK(adviser.average_snr_db() == Approx(6));
    adviser.add_value(12);
    CHECK(adviser.average_snr_db() == Approx(9));
    adviser.reset();
    CHECK(std::isnan(adviser.average_snr_db()));
  }
  SECTION("invalid energies yield zero") {
    for (float value : {1e38f, std::numeric_limits<float>::quiet_NaN()}) {
      ComplexNumber<float> spectrum(value, 10, 0.0f, 10);
      CHECK(adviser.calc_snr_estimation(spectrum, spectrum, metrics) == 0.0f);
    }
  }
  SECTION("mismatched complex components yield zero") {
    ComplexNumber<float> input(1.0f, 8, 1.0f, 10);
    ComplexNumber<float> enhanced(1.0f, 10, 1.0f, 10);
    CHECK(adviser.calc_snr_estimation(input, enhanced, metrics) == 0.0f);
  }
  SECTION("silence and empty spectra remain finite") {
    ComplexNumber<float> empty;
    ComplexNumber<float> zero(0.0f, 10, 0.0f, 10);
    CHECK(std::isfinite(adviser.calc_snr_estimation(empty, empty, metrics)));
    CHECK(std::isfinite(adviser.calc_snr_estimation(zero, zero, metrics)));
  }
  SECTION("extreme enhancement uses the documented saturation value") {
    ComplexNumber<float> input(0.01f, 10, 0.01f, 10);
    ComplexNumber<float> enhanced(10000.0f, 10, 10000.0f, 10);
    CHECK(adviser.calc_snr_estimation(input, enhanced, metrics) == 100.0f);
  }
}

TEST_CASE("AEC configuration rejects unsupported canceler types", "[validation]") {
  REQUIRE_THROWS_WITH(gigaedge::denoiser::AecConfig(
                          gigaedge::denoiser::StringConfigReader("echo_cancellation:\n  type: unsupported\n")),
                      "Invalid echo canceler type: unsupported");
}

TEST_CASE("Removed model identifiers are rejected before loading weights", "[validation]") {
  for (const std::string type : {"best_quality", "best_speed", "best_speed_new"}) {
    auto config = std::make_shared<gigaedge::denoiser::DenoiserConfig>(
        gigaedge::denoiser::StringConfigReader("model:\n  model_path: unused.onnx\ninference:\n  model_type: " + type));
    REQUIRE_THROWS_AS(gigaedge::denoiser::InferenceRunner(config, 1, false), std::invalid_argument);
  }
}
