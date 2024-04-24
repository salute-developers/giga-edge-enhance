#include <memory>
#include <unordered_map>
#include <catch.hpp>
#include "include/spench.h"
#include "tests/src/utils.h"
#include <future> // NOLINT

struct TestFixture {
  std::vector<float> m_wav_data;
  std::unique_ptr<spench::SpenchDenoise> m_denoiser;
  std::unique_ptr<spench::SpenchAudioProcess> m_dsp;

  const std::string m_kBase = "data/";

  TestFixture() :
          m_denoiser(nullptr),
          m_dsp(nullptr) {}
  ~TestFixture() {
    m_denoiser.reset();
    m_dsp.reset();
  }
  TestFixture& operator=(const TestFixture&) = delete;
  TestFixture(const TestFixture&) = delete;
  TestFixture(TestFixture&& orig) :
          m_wav_data(std::move(orig.m_wav_data)),
          m_denoiser(std::move(orig.m_denoiser)),
          m_dsp(std::move(orig.m_dsp)) {
    orig.m_wav_data.clear();
    orig.m_denoiser = nullptr;
    orig.m_dsp = nullptr;
  }
  TestFixture& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data, false);
    return *this;
  }

  TestFixture& InitDenoiser(const std::string& config_name,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    std::string config_path = m_kBase + "config/" + config_name;
    m_denoiser = std::make_unique<spench::SpenchDenoise>(config_path.c_str(),
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitAudioProcessor(const std::string& config_name) {
    std::string config_path = m_kBase + "config/" + config_name;
    m_dsp = std::make_unique<spench::SpenchAudioProcess>(config_path.c_str());
    return *this;
  }

  TestFixture& InitAudioProcessor(const bool agc = true,
                                  const bool rc  = true) {
    m_dsp = std::make_unique<spench::SpenchAudioProcess>(agc, rc);
    return *this;
  }
};

TEST_CASE("SUCCEED Convert pcm to float point [spench c++ api]") {
  std::vector<int16_t> pcm16 = {0, 32767, -32768, 2000, -5000};
  std::vector<float> ref_float = {0.0, 1.0, -1.0, 0.06103, -0.1526};
  std::vector<float> converted_float;

  spench::tools::convertPCM16ToFloat32(pcm16, converted_float);

  IsVectorsClose(ref_float, converted_float, 0.001);
}

TEST_CASE("SUCCEED Convert float point to pcm [spench c++ api]") {
  std::vector<float> float_point32 = {0, 1, -1, 0.06103, -0.1526};
  std::vector<int16_t> ref_pcm = {0, 32767, -32768, 2000, -5000};
  std::vector<int16_t> converted_pcm;

  spench::tools::convertFloat32ToPCM16(float_point32, converted_pcm);

  REQUIRE(ref_pcm == converted_pcm);
}

TEST_CASE("SUCCEED Create context [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  spench::DenoiseContext<float>* ctx = nullptr;
  ctx = test_data.m_denoiser->CreateContext();
  REQUIRE(ctx != nullptr);

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Retrieve model sample rate [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  REQUIRE(test_data.m_denoiser->GetModelSampleRate() == 16000);
}

TEST_CASE("SUCCEED Retrieve model input frame size [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  REQUIRE(test_data.m_denoiser->GetModelInputFrameSize() == (16000 * 16) / 1000);
}

TEST_CASE("SUCCEED Set and retrieve a context id [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml"));
#endif

  spench::DenoiseContext<float>* ctx = nullptr;
  ctx = test_data.m_denoiser->CreateContext();

  test_data.m_denoiser->SetContextId(ctx, 1000);
  REQUIRE(test_data.m_denoiser->GetContextId(ctx) == 1000);
  test_data.m_denoiser->SetContextId(ctx, 5000);
  REQUIRE(test_data.m_denoiser->GetContextId(ctx) == 5000);

  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Get empty statistic information with nullptr instead of context [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  auto stat = test_data.m_denoiser->GetStreamStat(nullptr);
  REQUIRE(stat.m_snr == 0.0);
}

TEST_CASE("SUCCEED Run inference with 100 tensors [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.m_denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    REQUIRE(ret_code >= SpenchRetCode::SUCCESS);
    REQUIRE(ret_code <= SpenchRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Get statistic information [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    auto ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    test_data.m_denoiser->GetStreamStat(ctx);

    REQUIRE(ret_code >= SpenchRetCode::SUCCESS);
    REQUIRE(ret_code <= SpenchRetCode::CONTEXT_ACCUMULATION);
  }

  auto stat = test_data.m_denoiser->GetStreamStat(nullptr);
  CHECK(stat.m_snr == 0.0);

  stat = test_data.m_denoiser->GetStreamStat(ctx);
  REQUIRE(stat.m_snr != 0.0);

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Run inference with 100 tensors and default context argumetns [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext(0, 0);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.m_denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    REQUIRE(ret_code >= SpenchRetCode::SUCCESS);
    REQUIRE(ret_code <= SpenchRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Reload configuration from disk [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  REQUIRE_NOTHROW(test_data.m_denoiser->ReloadConfig());
}

TEST_CASE("SUCCEED Run inference with the best quality 16 kHz model and 2 contexts [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  std::vector<float> predictions1;
  std::vector<float> predictions2;
  spench::DenoiseContext<float>* ctx1 = test_data.m_denoiser->CreateContext();
  spench::DenoiseContext<float>* ctx2 = test_data.m_denoiser->CreateContext();
  CHECK(ctx1 != ctx2);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx1, chunk, ret1);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    ret_code = test_data.m_denoiser->ProcessChunk(ctx2, chunk, ret2);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    predictions1.insert(predictions1.end(), ret1.begin(), ret1.end());
    predictions2.insert(predictions2.end(), ret2.begin(), ret2.end());
  }
  IsVectorsClose(predictions1, predictions2, 0.000001);

  test_data.m_denoiser->DestroyContext(&ctx1);
  CHECK(ctx1 == nullptr);
  test_data.m_denoiser->DestroyContext(&ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with 16 kHz model and 2 context with default parameters [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  std::vector<float> predictions1;
  std::vector<float> predictions2;
  spench::DenoiseContext<float>* ctx1 = test_data.m_denoiser->CreateContext();
  spench::DenoiseContext<float>* ctx2 = test_data.m_denoiser->CreateContext(0, 0);
  CHECK(ctx1 != ctx2);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx1, chunk, ret1);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    ret_code = test_data.m_denoiser->ProcessChunk(ctx2, chunk, ret2);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    predictions1.insert(predictions1.end(), ret1.begin(), ret1.end());
    predictions2.insert(predictions2.end(), ret2.begin(), ret2.end());
  }
  IsVectorsClose(predictions1, predictions2, 0.000001);

  test_data.m_denoiser->DestroyContext(&ctx1);
  CHECK(ctx1 == nullptr);
  test_data.m_denoiser->DestroyContext(&ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with the best quality 16 kHz model and 2 contexts concurently [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  const uint16_t kNumber = 2;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<spench::DenoiseContext<float>*> contexts;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          spench::DenoiseContext<float>* ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
      CHECK(ret_code >= SpenchRetCode::SUCCESS);
      predictions.insert(predictions.end(), ret.begin(), ret.end());
    }
  };

  for (uint32_t i = 0; i < kNumber; ++i) {
    spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();
    test_data.m_denoiser->SetContextId(ctx, i);
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
    test_data.m_denoiser->DestroyContext(&contexts[i]);
  }
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model and 2 contexts concurently [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  const uint16_t kNumber = 2;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<spench::DenoiseContext<float>*> contexts;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          spench::DenoiseContext<float>* ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
      CHECK(ret_code >= SpenchRetCode::SUCCESS);
      predictions.insert(predictions.end(), ret.begin(), ret.end());
    }
  };

  for (uint32_t i = 0; i < kNumber; ++i) {
    spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();
    test_data.m_denoiser->SetContextId(ctx, i);
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
    test_data.m_denoiser->DestroyContext(&contexts[i]);
  }
}

TEST_CASE("FAIL Run inference with invalid chunk size [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  std::vector<float> predictions;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext(16000, 16000);
  const uint32_t kChunk_size = 20;
  const uint32_t kChunk_stride = 10;
  const uint32_t kChunks_to_compare = 5;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  std::vector<float> ret;
  SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunks[0], ret);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the best quality model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 30;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.m_denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.m_denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("SUCCEED Run inference with encrypted fast 24 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_encrypted_24khz.yaml", "test").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_encrypted_24khz.yaml", "test").
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.m_denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}

TEST_CASE("SUCCEED Run inference with licenced encrypted fast 24 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_lic_encrypted_24khz.yaml",
                                         "qwerty",
                                         "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I=").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_lic_encrypted_24khz.yaml",
                                         "qwerty",
                                         "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU=").
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.m_denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}
#else
TEST_CASE("FAIL Run with password when the framework built without encryption [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser("default_configuration_fast_tflite_encrypted_24khz.yaml", "test"),
      "Due to errors denoiser could not be created! ret_code: -8");
  #else
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser("default_configuration_fast_onnx_encrypted_24khz.yaml", "test"),
      "Due to errors denoiser could not be created! ret_code: -8");
  #endif
}

TEST_CASE("FAIL Run with password and checksum when the framework built without encryption [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser("default_configuration_fast_tflite_lic_encrypted_24khz.yaml",
                "qwerty",
                "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I="),
      "Due to errors denoiser could not be created! ret_code: -8");
  #else
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser("default_configuration_fast_onnx_lic_encrypted_24khz.yaml",
                "qwerty",
                "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU="),
      "Due to errors denoiser could not be created! ret_code: -8");
  #endif
}
#endif

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with the final inference chunk [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  SpenchRetCode ret_code;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 10;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                test_data.m_denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
  std::vector<float> ret;
  ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  test_data.m_denoiser->DestroyContext(&ctx);
}

#ifndef BUILD_WITHOUT_SOXR
TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with input resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 200;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
                                           * kAudio_dst_sample_rate / 1000;
  SpenchRetCode ret_code;
  std::vector<float> prediction;
  std::vector<float> reference;
  spench::DenoiseContext<float>* ctx =
                          test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_24_enhanced.wav", reference);
  chunks.resize(kChunks_to_compare);
  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  std::vector<float> ret;
  ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);
  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.005);
  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with output resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_24_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 200;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
                                           * kAudio_dst_sample_rate / 1000;
  SpenchRetCode ret_code;
  std::vector<float> prediction;
  std::vector<float> reference;
  spench::DenoiseContext<float>* ctx =
                          test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);
  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);

  std::vector<float> ret;
  ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.008);
  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with input and output resampling 48 kHz [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
                                           * kAudio_dst_sample_rate / 1000;
  SpenchRetCode ret_code;
  std::vector<float> prediction;
  std::vector<float> reference;
  spench::DenoiseContext<float>* ctx =
                          test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  std::vector<float> ret;
  ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.005);
  test_data.m_denoiser->DestroyContext(&ctx);
}
#else
TEST_CASE("FAIL Run inference with the fast 24 kHz model with input resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;

  REQUIRE_THROWS_WITH(test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
}

TEST_CASE("FAIL Run inference with the fast 24 kHz model with output resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_24_noised.wav"));
  #endif

  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;

  REQUIRE_THROWS_WITH(test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
}

TEST_CASE("FAIL Run inference with the fast 24 kHz model with input and output resampling 48 kHz [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;

  REQUIRE_THROWS_WITH(test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
}
#endif

TEST_CASE("SUCCEED Run AudioProcessing with RC default parameters [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(false, true).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(false, true).
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.m_wav_data.begin(),
                               test_data.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.75
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with AGC default parameters [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(true, false).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(true, false).
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.m_wav_data.begin(),
                               test_data.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });

  REQUIRE(greater_than_1.size() == 0);
  // AGC could increase signal upto 1.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });
  REQUIRE(greater_than_1.size() != 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with changed AGC parameters [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(true, false).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(true, false).
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.m_wav_data.begin(),
                               test_data.m_wav_data.begin() + source_data_size);
  chunks.resize(kChunks_to_compare);

  test_data.m_dsp->SetAgcParameters(48000, 0.9, 20.0);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::vector<float> greater_than_09;
  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_09),
    [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_09.size() != 0);
  // AGC could increase signal upto 0.8
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_09),
    [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_09.size() != 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with changed RC parameters [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(false, true).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitAudioProcessor(false, true).
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.m_wav_data.begin(),
                               test_data.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_8;
  chunks.resize(kChunks_to_compare);

  test_data.m_dsp->SetRcParameters(48000, -2, 1e-4);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_8),
               [](float& i) { return std::fabs(i) > 0.8; });

  REQUIRE(greater_than_0_8.size() != 0);
  greater_than_0_8.resize(0);
  REQUIRE(greater_than_0_8.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.8
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_8),
               [](float& i) { return std::fabs(i) > 0.8; });
  REQUIRE(greater_than_0_8.size() == 0);
}

TEST_CASE("FAIL Run AudioProcessing with incorrect config file [spench c++ api]") {
  REQUIRE_THROWS_WITH(TestFixture().
                      InitAudioProcessor(std::string("config.json")),
                      "Due to errors audio processing could not be created! ret_code: -10");
}

TEST_CASE("SUCCEED Run AudioProcessing with reading RC parameters from config [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor(std::string("default_configuration_fast_tflite_rc_enabled_24khz.yaml")).
                             SetWav("test1_48_noised.wav"));

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.m_wav_data.begin(),
                               test_data.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.75
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading AGC parameters from config [spench c++ api]") {
  auto test_data =
                std::move(TestFixture().
                          InitAudioProcessor(std::string("default_configuration_fast_tflite_agc_enabled_24khz.yaml")).
                          SetWav("test1_48_noised.wav"));

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.m_wav_data.begin(),
                               test_data.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.m_dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });

  REQUIRE(greater_than_1.size() == 0);
  // AGC could increase signal upto 1.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });
  REQUIRE(greater_than_1.size() != 0);
}
