#include "spench/src/denoiser_audio_handler.h"
#include "spench/src/denoiser_model_adviser.h"
#include "spench/src/dereverberator_audio_handler.h"
#include <catch.hpp>
#include <iostream>
#include "tests/src/utils.h"
#include "spench/include/errors.h"

struct TestHandler {
  std::unique_ptr<spench::BaseAudioHandler> m_handler;

  const std::string m_kBase = "data/";
  std::vector<float> m_wav_data;

 public:
  TestHandler() = default;
  TestHandler& operator=(const TestHandler&) = delete;
  TestHandler(const TestHandler&) = delete;
  TestHandler(TestHandler&& orig) :
          m_handler(std::move(orig.m_handler)),
          m_wav_data(std::move(orig.m_wav_data)) {
    orig.m_wav_data.clear();
    orig.m_handler = nullptr;
  }

  TestHandler& InitDenoserHandler(const std::string& config_path) {
    std::string config = m_kBase + "config/" + config_path;
    m_handler = std::unique_ptr<spench::BaseAudioHandler>(
                                                new spench::DenoiserAudioHandler(config));
    return *this;
  }

  TestHandler& InitDereverberatorHandler(const std::string& config_path) {
    std::string config = m_kBase + "config/" + config_path;
    m_handler = std::unique_ptr<spench::BaseAudioHandler>(
                                                new spench::DereverberatorAudioHandler(config));
    return *this;
  }

  TestHandler& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data);
    return *this;
  }

  TestHandler& LoadDataFromBin(const std::string& dump, uint32_t step_size_samples,
                               std::vector<std::vector<float>>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }

  TestHandler& LoadDataFromBin(const std::string& dump,
                               std::vector<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }

  TestHandler& LoadDataFromBin(const std::string& dump,
                               std::valarray<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }

  spench::DenoiserModelAdviser<float>* GetAdviser() {
    if (auto* m_denoiser_handler = dynamic_cast<spench::DenoiserAudioHandler*>(m_handler.get())) {
      return m_denoiser_handler->adviser();
    } else {
      return nullptr;
    }
  }
};

TEST_CASE("FAIL Run denoiser handler with exception from fe runner [denoiser handler]") {
  REQUIRE_THROWS_WITH(TestHandler()
      .InitDenoserHandler("test_configuration_with_incorrect_sampling_freq.yaml"),
      "Config reading error. Sampling frequency is not a scalar!");
}

TEST_CASE("FAIL Run denoiser handler with exception from inference runner [denoiser handler]") {
  REQUIRE_THROWS_WITH(TestHandler()
      .InitDenoserHandler("test_configuration_with_incorrect_model_path.yaml"),
      "Config reading error. Model path parameter is not a single string!");
}

TEST_CASE("FAIL Run denoiser handler with exception for incorrect config path [denoiser handler]") {
  REQUIRE_THROWS_WITH(TestHandler()
      .InitDenoserHandler("config.json"), "Due to errors denoiser could not be created! ret_code: -10");
}

TEST_CASE("FAIL Run dereverberator handler with exception from fe runner [dereverberator handler]") {
  REQUIRE_THROWS_WITH(TestHandler()
      .InitDereverberatorHandler("test_configuration_with_incorrect_sampling_freq.yaml"),
      "Config reading error. Sampling frequency is not a scalar!");
}

TEST_CASE("FAIL Run dereverberator handler with exception from inference runner [dereverberator handler]") {
  REQUIRE_THROWS_WITH(TestHandler()
      .InitDereverberatorHandler("test_configuration_with_incorrect_model_path.yaml"),
      "Config reading error. Model path parameter is not a single string!");
}

TEST_CASE("FAIL Run dereverberator handler with exception for incorrect config path [dereverberator handler]") {
  REQUIRE_THROWS_WITH(TestHandler()
      .InitDereverberatorHandler("config.json"),
                      "Due to errors dereverberation module could not be created! ret_code: -10");
}

TEST_CASE("SUCCEED Create frame [denoiser handler]") {
  std::vector<float> input_chunk = {249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> gt_reversed_chunk = {0,     255.0, 254.0, 253.0, 252.0,
                                          251.0, 250.0, 249.0, 250.0,
                                          251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> reversed_chunk;
  spench::tools::PrepareFirstInputFrame(input_chunk, reversed_chunk);
  IsVectorsClose(reversed_chunk, gt_reversed_chunk, 1.0);
}

TEST_CASE("FAIL Run denoiser handler with error for incorrect input lengh [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx = sheduler_test_env.m_handler->CreateContext();

  const std::vector<float> kInput(128, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx, kInput, output);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
  sheduler_test_env.m_handler->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Change number of engine threads [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  const uint32_t kDefaultThreadsNum = 4;
  CHECK(kDefaultThreadsNum == sheduler_test_env.m_handler->GetEngineThreadsNum());
  REQUIRE_NOTHROW(sheduler_test_env.m_handler->ReloadConfig());
}

TEST_CASE("SUCCEED Change number of engine threads [dereverberator handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDereverberatorHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDereverberatorHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  const uint32_t kDefaultThreadsNum = 4;
  CHECK(kDefaultThreadsNum == sheduler_test_env.m_handler->GetEngineThreadsNum());
  REQUIRE_NOTHROW(sheduler_test_env.m_handler->ReloadConfig());
}

TEST_CASE("SUCCEED Run denoise with empty chunk with the model 24kHz [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto handler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto handler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx_ptr = handler_test_env.m_handler->CreateContext();
  spench::DenoiseContextF<float>* denoise_ctx_ptr = dynamic_cast<spench::DenoiseContextF<float>*>(ctx_ptr);
  const std::vector<float> input1;

  std::vector<float> output(5, 0);
  REQUIRE(output.size() != 0);

  SpenchRetCode ret_code = handler_test_env.m_handler->ProcessFrame(denoise_ctx_ptr, input1, output);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(output.size() == 0);
  handler_test_env.m_handler->DestroyContext(&ctx_ptr);
}

TEST_CASE("SUCCEED Run denoise and check zero outputs with the best quality model [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx = sheduler_test_env.m_handler->CreateContext();
  spench::DenoiseContextQ<float>* ctx_q = dynamic_cast<spench::DenoiseContextQ<float>*>(ctx);
  const uint16_t kInput_frame_size = 256;
  const uint16_t kLstm_layers_num = 2;
  const uint16_t kLstm_hidden_size = 2 * kInput_frame_size;
  const uint16_t kSb_lstm_hidden_size = 384;
  const uint16_t kSpec_size = 257;
  const std::vector<float> input1(kInput_frame_size, 0.5);
  const std::vector<float> input2(kInput_frame_size, 0.7);

  const uint64_t kFull_band_hidden_size = kLstm_layers_num * kLstm_hidden_size;
  const uint64_t kSub_band_hidden_size = kLstm_layers_num * kSpec_size * kSb_lstm_hidden_size;
  std::vector<float> gt_fullband_hidden(kFull_band_hidden_size, 0.0);
  std::vector<float> output;

  SpenchRetCode ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  CHECK(std::vector<float>(kInput_frame_size, 0.0) == output);

  REQUIRE(ctx_q->m_prev_features.size() == 1);
  REQUIRE(ctx_q->m_fullband_hidden.size() == kFull_band_hidden_size);
  REQUIRE(ctx_q->m_fullband_cell_state.size() == kFull_band_hidden_size);
  REQUIRE(ctx_q->m_subband_hidden.size() == kSub_band_hidden_size);
  REQUIRE(ctx_q->m_subband_cell_state.size() == kSub_band_hidden_size);
  REQUIRE(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input2, output);
  REQUIRE(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);
  REQUIRE(ctx_q->m_prev_features.size() == 2);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) == output);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  REQUIRE(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);
  REQUIRE(ctx_q->m_prev_features.size() == 3);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) == output);
  sheduler_test_env.m_handler->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run dereverb with empty chunk with the model 48kHz [dereverberator handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    // auto handler_test_env = std::move(TestHandler().
    //                       InitDereverberatorHandler("default_configuration_dereverberation_tflite_48khz.yaml"));
  #else
    auto handler_test_env = std::move(TestHandler().
                          InitDereverberatorHandler("default_configuration_dereverberation_onnx_48khz.yaml"));

  spench::SpenchContext<float>* ctx_ptr = handler_test_env.m_handler->CreateContext();
  spench::DereverbContext<float>* dereverb_ctx_ptr = dynamic_cast<spench::DereverbContext<float>*>(ctx_ptr);
  const std::vector<float> input1;

  std::vector<float> output(5, 0);
  REQUIRE(output.size() != 0);

  SpenchRetCode ret_code = handler_test_env.m_handler->ProcessFrame(dereverb_ctx_ptr, input1, output);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(output.size() == 0);
  handler_test_env.m_handler->DestroyContext(&ctx_ptr);
  #endif
}

TEST_CASE("SUCCEED Run dereverb and check zero outputs with the model 48kHz [dereverberator handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    // auto handler_test_env = std::move(TestHandler().
    //                       InitDereverberatorHandler("default_configuration_dereverberation_tflite_48khz.yaml"));
  #else
    auto handler_test_env = std::move(TestHandler().
                          InitDereverberatorHandler("default_configuration_dereverberation_onnx_48khz.yaml"));


  spench::SpenchContext<float>* ctx_ptr = handler_test_env.m_handler->CreateContext();
  spench::DereverbContext<float>* dereverb_ctx_ptr = dynamic_cast<spench::DereverbContext<float>*>(ctx_ptr);
  const uint16_t kInput_frame_size = 768;
  const std::vector<float> input1(kInput_frame_size, 0.5);

  std::vector<float> output;

  SpenchRetCode ret_code = handler_test_env.m_handler->ProcessFrame(dereverb_ctx_ptr, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = handler_test_env.m_handler->ProcessFrame(dereverb_ctx_ptr, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  // non-zero tensor now. just check size
  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);

  ret_code = handler_test_env.m_handler->ProcessFrame(dereverb_ctx_ptr, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);

  ret_code = handler_test_env.m_handler->ProcessFrame(dereverb_ctx_ptr, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);
  handler_test_env.m_handler->DestroyContext(&ctx_ptr);
  #endif
}

TEST_CASE("SUCCEED Run denoise and check first non-zero output with the best quality model [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx = sheduler_test_env.m_handler->CreateContext();
  spench::DenoiseContextQ<float>* ctx_q = dynamic_cast<spench::DenoiseContextQ<float>*>(ctx);
  uint16_t kInput_frame_size = 256;
  const std::vector<float> input1(kInput_frame_size, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  REQUIRE(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  REQUIRE(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  REQUIRE(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ctx_q->m_prev_features.size() == 3);
  // non-zero tensor now. just check size
  REQUIRE(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);
  sheduler_test_env.m_handler->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Check finalize chunk for the best quality model [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx = sheduler_test_env.m_handler->CreateContext();

  uint16_t kInput_frame_size = 256;
  std::vector<float> output;

  SpenchRetCode ret_code = sheduler_test_env.m_handler->FinalizeInference(ctx, output);
  // as context of model is not filled in we can expect this code returned
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  REQUIRE(kInput_frame_size * 3 == output.size());
  REQUIRE(std::vector<float>(kInput_frame_size * 3, 0.0) == output);
  sheduler_test_env.m_handler->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run denoise and check finalize chunk for the best quality model [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_tflite_16khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_best_quality_onnx_16khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx = sheduler_test_env.m_handler->CreateContext();
  spench::DenoiseContextQ<float>* ctx_q = dynamic_cast<spench::DenoiseContextQ<float>*>(ctx);
  uint16_t kInput_frame_size = 256;
  const std::vector<float> input1(kInput_frame_size, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_q, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  // non-zero tensor now. just check size
  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);

  output.resize(0);

  ret_code = sheduler_test_env.m_handler->FinalizeInference(ctx_q, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(kInput_frame_size * 3 == output.size());
  REQUIRE(std::vector<float>(kInput_frame_size * 3, 0.0) != output);
  sheduler_test_env.m_handler->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run denoise and check finalize chunk for the fast model [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::SpenchContext<float>* ctx = sheduler_test_env.m_handler->CreateContext();
  spench::DenoiseContextF<float>* ctx_f = dynamic_cast<spench::DenoiseContextF<float>*>(ctx);
  uint16_t kInput_frame_size = 384;
  const std::vector<float> input1(kInput_frame_size, 0.5);
  std::vector<float> output;

  SpenchRetCode ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::CONTEXT_ACCUMULATION);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  // non-zero tensor now. just check size
  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);

  ret_code = sheduler_test_env.m_handler->ProcessFrame(ctx_f, input1, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  CHECK(output.size() == kInput_frame_size);
  REQUIRE(std::vector<float>(kInput_frame_size, 0.0) != output);

  output.resize(0);

  ret_code = sheduler_test_env.m_handler->FinalizeInference(ctx_f, output);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  REQUIRE(kInput_frame_size * 1 == output.size());
  REQUIRE(std::vector<float>(kInput_frame_size * 1, 0.0) != output);
  sheduler_test_env.m_handler->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run calcSnrEstimation with one chunk [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  std::valarray<float> test_noisy_real;
  std::valarray<float> test_noisy_imag;
  std::valarray<float> test_enhanced_real;
  std::valarray<float> test_enhanced_imag;
  spench::StreamCurrentMetrics<float> metrics;
  // Load from binary dump
  sheduler_test_env.LoadDataFromBin("calc_snr_enhanced_real.dump", test_enhanced_real);
  sheduler_test_env.LoadDataFromBin("calc_snr_enhanced_imag.dump", test_enhanced_imag);
  sheduler_test_env.LoadDataFromBin("calc_snr_noisy_real.dump", test_noisy_real);
  sheduler_test_env.LoadDataFromBin("calc_snr_noisy_imag.dump", test_noisy_imag);

  spench::ComplexNumber<float> test_noisy_mag(test_noisy_real, test_noisy_imag);
  spench::ComplexNumber<float> test_enhanced_mag(test_enhanced_real, test_enhanced_imag);
  std::vector<float> gt_test_snr_result;

  sheduler_test_env.LoadDataFromBin("calc_snr_result.dump", gt_test_snr_result);
  float test_snr_result = sheduler_test_env.GetAdviser()->calcSnrEstimation(test_noisy_mag,
                                                 test_enhanced_mag,
                                                 metrics);

  IsVectorsClose(std::vector{test_snr_result}, std::vector{gt_test_snr_result[0]}, 0.001);
}

TEST_CASE("SUCCEED Run calcSnrEstimation with dumped recording [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  std::valarray<float> test_noisy_real;
  std::valarray<float> test_noisy_imag;
  std::valarray<float> test_enhanced_real;
  std::valarray<float> test_enhanced_imag;
  spench::StreamCurrentMetrics<float> metrics;
  // Load from binary dump
  sheduler_test_env.LoadDataFromBin("calc_snr_enhanced_real.dump", test_enhanced_real);
  sheduler_test_env.LoadDataFromBin("calc_snr_enhanced_imag.dump", test_enhanced_imag);
  sheduler_test_env.LoadDataFromBin("calc_snr_noisy_real.dump", test_noisy_real);
  sheduler_test_env.LoadDataFromBin("calc_snr_noisy_imag.dump", test_noisy_imag);

  std::vector<float> test_snr_result;
  sheduler_test_env.LoadDataFromBin("calc_snr_result.dump", test_snr_result);

  for (uint32_t i = 0; i < test_enhanced_real.size(); ++i) {
    spench::ComplexNumber<float> test_noisy_mag(test_noisy_real[0], 1, test_noisy_imag[0], 1);
    spench::ComplexNumber<float> test_enhanced_mag(test_enhanced_real[0], 1, test_enhanced_imag[0], 1);
    test_snr_result.emplace_back(sheduler_test_env.GetAdviser()->calcSnrEstimation(test_noisy_mag,
                                                        test_enhanced_mag,
                                                        metrics));
  }

  IsVectorsClose(test_snr_result, test_snr_result, 0.000001);
}

TEST_CASE("SUCCEED Run calcSnrEstimation with zero recording [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
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
    test_snr_result.emplace_back(sheduler_test_env.GetAdviser()->calcSnrEstimation(test_noisy_mag,
                                                        test_enhanced_mag,
                                                        metrics));
  }

  IsVectorsClose(test_snr_result, test_snr_result, 0.000001);
}

TEST_CASE("SUCCEED Run calcSnrEstimation with empty recording [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
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
    test_snr_result.emplace_back(sheduler_test_env.GetAdviser()->calcSnrEstimation(test_noisy_mag,
                                                        test_enhanced_mag,
                                                        metrics));
  }

  IsVectorsClose(test_snr_result, test_snr_result, 0.000001);
}

TEST_CASE("FAIL calcSnrEstimation with overflow values [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::StreamCurrentMetrics<float> metrics;

  float big_val = 1e38f;
  std::valarray<float> test_real(big_val, 10);
  std::valarray<float> test_imag(big_val, 10);

  spench::ComplexNumber<float> spec(test_real, test_imag);
  float snr = sheduler_test_env.GetAdviser()->calcSnrEstimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calcSnrEstimation with NaN in input [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> test_real(1.0f, 10);
  test_real[3] = std::nanf("");
  std::valarray<float> test_imag(0.0f, 10);

  spench::ComplexNumber<float> spec(test_real, test_imag);
  float snr = sheduler_test_env.GetAdviser()->calcSnrEstimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calcSnrEstimation with mismatched input sizes [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> real(1.0f, 10);
  std::valarray<float> imag(1.0f, 3);

  spench::ComplexNumber<float> spec(real, imag);
  float snr = sheduler_test_env.GetAdviser()->calcSnrEstimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calcSnrEstimation with mostly zeros [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> real(0.0f, 100);
  real[0] = 1e-10f;
  std::valarray<float> imag = real;

  spench::ComplexNumber<float> input_spec(real, imag);
  spench::ComplexNumber<float> enhanced_spec(real, imag);
  float snr = sheduler_test_env.GetAdviser()->calcSnrEstimation(input_spec, enhanced_spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calcSnrEstimation with empty complex numbers [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> empty_real{};
  std::valarray<float> empty_imag{};

  spench::ComplexNumber<float> spec(empty_real, empty_imag);
  float snr = sheduler_test_env.GetAdviser()->calcSnrEstimation(spec, spec, metrics);

  CHECK(std::isfinite(snr));
}

TEST_CASE("FAIL calcSnrEstimation with mismatched real/imag sizes [denoiser handler]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_tflite_24khz.yaml"));
  #else
    auto sheduler_test_env = std::move(TestHandler().
                          InitDenoserHandler("default_configuration_fast_onnx_24khz.yaml"));
  #endif

  spench::StreamCurrentMetrics<float> metrics;

  std::valarray<float> noisy_real(1.0f, 8);
  std::valarray<float> noisy_imag(1.0f, 10);

  std::valarray<float> enhanced_real(1.0f, 10);
  std::valarray<float> enhanced_imag(1.0f, 10);

  spench::ComplexNumber<float> input_spec(noisy_real, noisy_imag);
  spench::ComplexNumber<float> enhanced_spec(enhanced_real, enhanced_imag);

  float snr = sheduler_test_env.GetAdviser()->calcSnrEstimation(input_spec, enhanced_spec, metrics);

  CHECK(snr == 0.0f);
}
