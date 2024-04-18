#include <memory>
#include <cstring>
#include <catch.hpp>
#include "include/spench_c.h"
#include "tests/src/utils.h"
#include <chrono> // NOLINT
#include <thread> // NOLINT

struct TestCAPI {
  std::vector<float> m_wav_data;
  std::vector<int16_t> m_pcm_wav_data;
  SpenchDenoise m_denoiser = nullptr;

  std::string m_kBase = "data/";

  TestCAPI() {}
  TestCAPI& operator=(const TestCAPI&) = delete;
  TestCAPI(const TestCAPI&) = delete;
  TestCAPI(TestCAPI&& orig) :
          m_wav_data(std::move(orig.m_wav_data)),
          m_pcm_wav_data(std::move(orig.m_pcm_wav_data)),
          m_denoiser(std::move(orig.m_denoiser)) {
    orig.m_wav_data.clear();
    orig.m_pcm_wav_data.clear();
    orig.m_denoiser = nullptr;
  }

  TestCAPI& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data, false);
    return *this;
  }

  TestCAPI& SetPCMWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_pcm_wav_data);
    return *this;
  }

  TestCAPI& InitModel(const std::string& config_name,
                      SpenchRetCode& ret,
                      const std::string& password = "",
                      const std::string& checksum = "") {
    std::string config_path = m_kBase + "config/" + config_name;
    ret = password.empty() ?
      SpenchDenoiseInit(config_path.c_str(), &m_denoiser):
      SpenchEncryptedDenoiseInit(config_path.c_str(), password.c_str(),
                               checksum.c_str(), &m_denoiser);
    return *this;
  }

  ~TestCAPI() {
    if (m_denoiser != nullptr) {
      SpenchDenoiseFree(&m_denoiser);
    }
  }
};

TEST_CASE("FAIL Create model with incorrect config name [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("config.yaml", ret_code));
  REQUIRE(ret_code == SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}

TEST_CASE("FAIL Create model with incorrect model path [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("test_configuration_with_incorrect_model_path.yaml", ret_code));
  REQUIRE(ret_code == SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}

TEST_CASE("FAIL Create context with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  SpenchContext ctx = nullptr;
  ret_code = SpenchDenoiseCreateContext(nullptr, &ctx, 0, 0);
  REQUIRE(ctx == nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Create and destroy context [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ctx != nullptr);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("FAIL Get a context id with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetContextId(nullptr, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get a context id with nullptr context [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = SpenchDenoiseGetContextId(test_data.m_denoiser, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get a context id with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetContextId(test_data.m_denoiser, ctx, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Set a context id with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(nullptr, ctx, 1000);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Set a context id with nullptr context [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Check that a default context id is zero [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetContextId(test_data.m_denoiser, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ret_id == 0);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Set and retrieve a context id [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  uint64_t ret_id = 0;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetContextId(test_data.m_denoiser, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ret_id == 1000);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get a model sample rate with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  uint64_t sample_rate = 0;
  ret_code = SpenchDenoiseGetSampleRate(nullptr, &sample_rate);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a model sample rate with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetSampleRate(test_data.m_denoiser, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get a model sample rate [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  uint64_t sample_rate = 0;

  ret_code = SpenchDenoiseGetSampleRate(test_data.m_denoiser, &sample_rate);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(sample_rate == 16000);
}

TEST_CASE("FAIL Get a model input size with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  uint64_t input_size = 0;
  ret_code = SpenchDenoiseGetInputFrameSize(nullptr, &input_size);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a model input size with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetInputFrameSize(test_data.m_denoiser, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get a model input size [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  uint64_t input_size = 0;

  ret_code = SpenchDenoiseGetInputFrameSize(test_data.m_denoiser, &input_size);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(input_size == (16000 * 16) / 1000);
}

TEST_CASE("FAIL Get a number of inference engine threads with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  uint64_t ret_threads_num = 0;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetEngineThreads(nullptr, &ret_threads_num);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get a number of inference engine threads with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetEngineThreads(test_data.m_denoiser, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Get a number of inference engine threads [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchContext ctx = nullptr;
  uint64_t ret_threads_num = 0;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetEngineThreads(test_data.m_denoiser, &ret_threads_num);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ret_threads_num == 4);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Run inference with invalid chunk size with the best quality model 16kHz [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 20;
  const uint32_t kChunk_stride = 10;
  unsigned char* ret;
  uint64_t ret_size = 0;
  std::vector<float> predictions(256);
  SpenchContext ctx = nullptr;
  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);


  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(1);

  ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                       ctx,
                                       (const unsigned char*) chunks[0].data(),
                                       sizeof(float) * chunks[0].size(),
                                       &ret,
                                       &ret_size);

  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Run inference with the best quality model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  SpenchContext ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Run inference with the PCM data [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetPCMWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetPCMWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  SpenchContext ctx = nullptr;
  std::vector<int16_t> prediction;
  std::vector<float> prediction_fp;
  std::vector<float> reference_prediction_fp;

  auto chunks = chunkWav<int16_t>(test_data.m_pcm_wav_data, 16000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference_prediction_fp);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<int16_t> ret;
    ret_code = SpenchDenoiseProcessChunkInt(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(int16_t),
                                         &ret_mem,
                                         &ret_size);

    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }
  // convert from pcm16 to fp
  for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
    float f = prediction[i] > 0 ?
          static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max():
          static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
    prediction_fp.emplace_back(f);
  }
  //
  reference_prediction_fp.resize(prediction_fp.size());
  IsVectorsClose(reference_prediction_fp, prediction_fp, 0.0001);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}


TEST_CASE("SUCCEED Get a last chunk with the best quality model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 10;
  const uint32_t kAudio_sample_rate = 16000;
  SpenchContext ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference;

  auto chunks = chunkWav<float>(test_data.m_wav_data, kAudio_sample_rate, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }
  reference.resize(prediction.size());
  IsVectorsClose(reference, prediction, 0.0001);

  unsigned char* final_ret_mem;
  uint64_t final_ret_size = 0;
  ret_code = SpenchDenoiseGetFinalChunkFp(test_data.m_denoiser,
                                        ctx,
                                        &final_ret_mem,
                                        &final_ret_size);
  CHECK(ret_code >= SpenchRetCode::SUCCESS);
  std::vector<float> final_vec(final_ret_size / sizeof(float));
  std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

  std::free(final_ret_mem);
  final_ret_mem = nullptr;

  REQUIRE(std::vector<float>(final_vec.size(), 0.0) != final_vec);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Get a last chunk with the best quality model and with the PCM data [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetPCMWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetPCMWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 10;
  SpenchContext ctx = nullptr;
  std::vector<int16_t> prediction;
  std::vector<float> prediction_fp;
  std::vector<float> reference_prediction_fp;

  auto chunks = chunkWav<int16_t>(test_data.m_pcm_wav_data, 16000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference_prediction_fp);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<int16_t> ret;
    ret_code = SpenchDenoiseProcessChunkInt(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(int16_t),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }
  // convert from pcm16 to fp
  for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
    float f = prediction[i] > 0 ?
          static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max():
          static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
    prediction_fp.emplace_back(f);
  }
  //
  reference_prediction_fp.resize(prediction_fp.size());
  IsVectorsClose(reference_prediction_fp, prediction_fp, 0.0001);

  unsigned char* final_ret_mem;
  uint64_t final_ret_size = 0;
  ret_code = SpenchDenoiseGetFinalChunkInt(test_data.m_denoiser,
                                        ctx,
                                        &final_ret_mem,
                                        &final_ret_size);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  std::vector<int16_t> final_vec(final_ret_size / sizeof(int16_t));
  std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

  std::free(final_ret_mem);
  final_ret_mem = nullptr;

  REQUIRE(std::vector<int16_t>(final_vec.size(), 0) != final_vec);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get statistic information with nullptr stat structure[spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseGetStreamStat(test_data.m_denoiser, ctx, nullptr);

  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get statistic information with nullptr context[spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  StreamStatStruct stream_stat;
  ret_code = SpenchDenoiseGetStreamStat(test_data.m_denoiser, nullptr, &stream_stat);

  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get statistic information [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  SpenchContext ctx = nullptr;
  std::vector<float> predictions1;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         sizeof(float) * chunk.size(),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    StreamStatStruct stream_stat;
    ret_code = SpenchDenoiseGetStreamStat(test_data.m_denoiser, ctx, &stream_stat);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);

    free(ret_mem);
    ret_mem = NULL;
  }

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
  CHECK(ctx == nullptr);
}

TEST_CASE("SUCCEED Run inference with 100 tensors for 2 contexts and compare result [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  SpenchContext ctx1 = nullptr;
  SpenchContext ctx2 = nullptr;
  std::vector<float> predictions1;
  std::vector<float> predictions2;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx1, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx2, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx1, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx2, 2000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  CHECK(ctx1 != ctx2);

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    unsigned char* ret_mem1;
    uint64_t ret_size1 = 0;
    unsigned char* ret_mem2;
    uint64_t ret_size2 = 0;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx1,
                                         (const unsigned char*) chunk.data(),
                                         sizeof(float) * chunk.size(),
                                         &ret_mem1,
                                         &ret_size1);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);

    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx2,
                                         (const unsigned char*) chunk.data(),
                                         sizeof(float) * chunk.size(),
                                         &ret_mem2,
                                         &ret_size2);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);

    std::vector<float> ret_vec1(ret_size1 / sizeof(float));
    std::vector<float> ret_vec2(ret_size2 / sizeof(float));
    std::memcpy(ret_vec1.data(), ret_mem1, ret_size1);
    std::memcpy(ret_vec2.data(), ret_mem2, ret_size2);
    predictions1.insert(predictions1.end(), ret_vec1.begin(), ret_vec1.end());
    predictions2.insert(predictions2.end(), ret_vec2.begin(), ret_vec2.end());
    free(ret_mem1);
    free(ret_mem2);
    ret_mem1 = NULL;
    ret_mem2 = NULL;
  }
  REQUIRE(predictions1 == predictions2);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx1);
  CHECK(ctx1 == nullptr);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with the best quality 16 kHz model and 5 contexts concurently [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint16_t kNumber = 5;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<SpenchContext> contexts;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          SpenchContext ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
      unsigned char* ret_mem;
      uint64_t ret_size = 0;
      std::vector<float> ret;
      ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                           ctx,
                                           (const unsigned char*) chunk.data(),
                                           sizeof(float) * chunk.size(),
                                           &ret_mem,
                                           &ret_size);
      CHECK(ret_code >= SpenchRetCode::SUCCESS);
      std::vector<float> ret_vec(ret_size / sizeof(float));
      std::memcpy(ret_vec.data(), ret_mem, ret_size);
      predictions.insert(predictions.end(), ret_vec.begin(), ret_vec.end());

      std::free(ret_mem);
      ret_mem = nullptr;
    }
  };

  for (uint32_t i = 0; i < kNumber; ++i) {
    SpenchContext ctx = nullptr;
    ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, i);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    contexts.emplace_back(ctx);
    pool.emplace_back(std::thread(functor, contexts.back(), std::ref(predictions.at(i))));
  }

  for (uint32_t i = 0; i < kNumber; ++i) {
    if (pool[i].joinable()) {
        pool[i].join();
    }
  }

  for (uint32_t i = 0; i < kNumber - 1; ++i) {
    REQUIRE(predictions[i] == predictions[i+1]);
  }

  for (uint32_t i = 0; i < kNumber; ++i) {
    SpenchDenoiseDestroyContext(test_data.m_denoiser, &(contexts[i]));
    CHECK(contexts[i] == nullptr);
  }
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model and 5 contexts concurently [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("rec_noise_001_24k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint16_t kNumber = 5;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<SpenchContext> contexts;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          SpenchContext ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
      unsigned char* ret_mem;
      uint64_t ret_size = 0;
      std::vector<float> ret;
      ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                           ctx,
                                           (const unsigned char*) chunk.data(),
                                           sizeof(float) * chunk.size(),
                                           &ret_mem,
                                           &ret_size);
      CHECK(ret_code >= SpenchRetCode::SUCCESS);
      std::vector<float> ret_vec(ret_size / sizeof(float));
      std::memcpy(ret_vec.data(), ret_mem, ret_size);
      predictions.insert(predictions.end(), ret_vec.begin(), ret_vec.end());

      std::free(ret_mem);
      ret_mem = nullptr;
    }
  };

  for (uint32_t i = 0; i < kNumber; ++i) {
    SpenchContext ctx = nullptr;
    ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, i);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    contexts.emplace_back(ctx);
    pool.emplace_back(std::thread(functor, contexts.back(), std::ref(predictions.at(i))));
  }

  for (uint32_t i = 0; i < kNumber; ++i) {
    if (pool[i].joinable()) {
        pool[i].join();
    }
  }

  for (uint32_t i = 0; i < kNumber - 1; ++i) {
    REQUIRE(predictions[i] == predictions[i+1]);
  }

  for (uint32_t i = 0; i < kNumber; ++i) {
    SpenchDenoiseDestroyContext(test_data.m_denoiser, &(contexts[i]));
    CHECK(contexts[i] == nullptr);
  }
}
/*
#ifndef BUILD_WITH_ADDRESS_SANITIZER
TEST_CASE("SUCCEED Run inference for the best quality model 16 khz and compare time with threshold [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  // average and max inference time in milli seconds for 16ms chunk
  float avg_upperbound = 25.0f;  // ms
  float max_upperbound = 40.0f;  // ms

  SpenchContext ctx = nullptr;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  std::vector<uint32_t> stat;

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;

    auto start = std::chrono::high_resolution_clock::now();
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         sizeof(float) * chunk.size(),
                                         &ret_mem,
                                         &ret_size);
    auto end = std::chrono::high_resolution_clock::now();
    stat.push_back((end - start).count());
    free(ret_mem);
    ret_mem = NULL;
  }
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
  uint32_t sum = std::accumulate(stat.begin(), stat.end(), 0);
  float avg_meausred = static_cast<float>(sum / stat.size()) / 1000000;
  float max_meausred = static_cast<float>(*max_element(stat.begin(), stat.end())) / 1000000;

  std::cout << "avg_measured: " << avg_meausred << " ms" << std::endl;
  std::cout << "max_measured: " << max_meausred << " ms" << std::endl;

  REQUIRE(avg_upperbound > avg_meausred);
  REQUIRE(max_upperbound > max_meausred);
}
#endif
*/
TEST_CASE("SUCCEED Run inference with the fast 24 kHz model on a part of .wav file [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("rec_noise_001_24k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  SpenchContext ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }

  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("SUCCEED Run inference with the encrypted fast 24 kHz model on a part of .wav file [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_encrypted_24khz.yaml", ret_code, "test").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_encrypted_24khz.yaml", ret_code, "test").
                               SetWav("rec_noise_001_24k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  SpenchContext ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }

  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Run inference with the licensed encrypted fast 24 kHz model on a part of .wav file [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_lic_encrypted_24khz.yaml",
                                         ret_code, "qwerty", "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I=").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_lic_encrypted_24khz.yaml",
                                         ret_code, "qwerty", "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU=").
                               SetWav("rec_noise_001_24k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  SpenchContext ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }

  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}
#else
TEST_CASE("FAIL Run with password when the framework built without encryption [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_encrypted_24khz.yaml",
                                         ret_code, "test"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_encrypted_24khz.yaml",
                                         ret_code, "test"));
  #endif
  CHECK(ret_code == SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}

TEST_CASE("FAIL Run with password and checksum when the framework built without encryption [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_encrypted_24khz.yaml",
                                         ret_code, "qwerty",
                                         "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I="));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_encrypted_24khz.yaml",
                                         ret_code, "qwerty",
                                         "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU="));
  #endif
  CHECK(ret_code == SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}
#endif
/*
#ifndef BUILD_WITH_ADDRESS_SANITIZER
TEST_CASE("SUCCEED Run inference for the fast 24 kHz model and compare time with threshold [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("rec_noise_001_24k.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  // average and max inference time in milli seconds for 16ms chunk
  float avg_upperbound = 15.0f;  // ms
  float max_upperbound = 20.0f;  // ms

  SpenchContext ctx = nullptr;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, 0, 0);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  std::vector<uint32_t> stat;

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;

    auto start = std::chrono::high_resolution_clock::now();
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         sizeof(float) * chunk.size(),
                                         &ret_mem,
                                         &ret_size);
    auto end = std::chrono::high_resolution_clock::now();
    stat.push_back((end - start).count());
    free(ret_mem);
    ret_mem = NULL;
  }
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
  uint32_t sum = std::accumulate(stat.begin(), stat.end(), 0);
  float avg_meausred = static_cast<float>(sum / stat.size()) / 1000000;
  float max_meausred = static_cast<float>(*max_element(stat.begin(), stat.end())) / 1000000;

  std::cout << "avg_measured: " << avg_meausred << " ms" << std::endl;
  std::cout << "max_measured: " << max_meausred << " ms" << std::endl;

  REQUIRE(avg_upperbound > avg_meausred);
  REQUIRE(max_upperbound > max_meausred);
}
#endif
*/
#ifndef BUILD_WITHOUT_SOXR
TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with input resampling [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 200;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
                                           * kAudio_dst_sample_rate / 1000;
  std::vector<float> prediction;
  std::vector<float> reference;

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, kAudio_src_sample_rate, kAudio_dst_sample_rate);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_24_enhanced.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  unsigned char* final_ret_mem;
  uint64_t final_ret_size = 0;
  ret_code = SpenchDenoiseGetFinalChunkFp(test_data.m_denoiser,
                                        ctx,
                                        &final_ret_mem,
                                        &final_ret_size);
  CHECK(ret_code >= SpenchRetCode::SUCCESS);
  std::vector<float> final_vec(final_ret_size / sizeof(float));
  std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

  std::free(final_ret_mem);
  final_ret_mem = nullptr;

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(final_vec.size(), 0.0) != final_vec);
  prediction.insert(prediction.end(), final_vec.begin(), final_vec.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.005);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with output resampling [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("test1_24_noised.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 200;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
                                           * kAudio_dst_sample_rate / 1000;
  std::vector<float> prediction;
  std::vector<float> reference;

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, kAudio_src_sample_rate, kAudio_dst_sample_rate);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);

  unsigned char* final_ret_mem;
  uint64_t final_ret_size = 0;
  ret_code = SpenchDenoiseGetFinalChunkFp(test_data.m_denoiser,
                                        ctx,
                                        &final_ret_mem,
                                        &final_ret_size);
  CHECK(ret_code >= SpenchRetCode::SUCCESS);
  std::vector<float> final_vec(final_ret_size / sizeof(float));
  std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

  std::free(final_ret_mem);
  final_ret_mem = nullptr;

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(final_vec.size(), 0.0) != final_vec);

  prediction.insert(prediction.end(), final_vec.begin(), final_vec.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.008);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with input and output resampling 48 kHz [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
                                           * kAudio_dst_sample_rate / 1000;
  std::vector<float> prediction;
  std::vector<float> reference;

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, kAudio_src_sample_rate, kAudio_dst_sample_rate);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  ret_code = SpenchDenoiseSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = SpenchDenoiseProcessChunkFp(test_data.m_denoiser,
                                         ctx,
                                         (const unsigned char*) chunk.data(),
                                         chunk.size() * sizeof(float),
                                         &ret_mem,
                                         &ret_size);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  unsigned char* final_ret_mem;
  uint64_t final_ret_size = 0;
  ret_code = SpenchDenoiseGetFinalChunkFp(test_data.m_denoiser,
                                        ctx,
                                        &final_ret_mem,
                                        &final_ret_size);
  CHECK(ret_code >= SpenchRetCode::SUCCESS);
  std::vector<float> final_vec(final_ret_size / sizeof(float));
  std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

  std::free(final_ret_mem);
  final_ret_mem = nullptr;

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(final_vec.size(), 0.0) != final_vec);

  prediction.insert(prediction.end(), final_vec.begin(), final_vec.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.005);
  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}
#else

TEST_CASE("FAIL Run inference with the fast 24 kHz model with input resampling [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, kAudio_src_sample_rate, kAudio_dst_sample_rate);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Run inference with the fast 24 kHz model with output resampling [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("test1_24_noised.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, kAudio_src_sample_rate, kAudio_dst_sample_rate);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Run inference with the fast 24 kHz model with input and output resampling 48 kHz [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_tflite_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                               SetWav("test1_48_noised.wav"));
  #endif
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;

  SpenchContext ctx = nullptr;

  ret_code = SpenchDenoiseCreateContext(test_data.m_denoiser, &ctx, kAudio_src_sample_rate, kAudio_dst_sample_rate);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchDenoiseDestroyContext(test_data.m_denoiser, &ctx);
}
#endif
