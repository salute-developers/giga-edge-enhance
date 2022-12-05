#include <memory>
#include <unordered_map>
#include <catch.hpp>
#include "include/spench.h"
#include "tests/src/utils.h"
#include <future> // NOLINT

struct TestFixture {
  std::vector<float> m_wav_data;
  std::unique_ptr<spench::AudioDenoise> m_denoiser;

  const std::string m_kBase = "data/";

  TestFixture() :
          m_denoiser(nullptr) {}
  ~TestFixture() {
    m_denoiser.reset();
  }
  TestFixture& operator=(const TestFixture&) = delete;
  TestFixture(const TestFixture&) = delete;
  TestFixture(TestFixture&& orig) :
          m_wav_data(std::move(orig.m_wav_data)),
          m_denoiser(std::move(orig.m_denoiser)) {
    orig.m_wav_data.clear();
    orig.m_denoiser = nullptr;
  }
  TestFixture& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data, false);
    return *this;
  }

  TestFixture& InitModel(const std::string config_name,
                         const std::string password = "") {
    std::string config_path = m_kBase + "config/" + config_name;
    m_denoiser = std::make_unique<spench::AudioDenoise>(config_path.c_str(),
                                                        password.c_str());
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
                        InitModel("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
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
                        InitModel("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  REQUIRE(test_data.m_denoiser->GetModelSampleRate() == 16000);
}

TEST_CASE("SUCCEED Retrieve model input frame size [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  REQUIRE(test_data.m_denoiser->GetModelInputFrameSize() == (16000 * 16) / 1000);
}

TEST_CASE("SUCCEED Set and retrieve a context id [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
#endif

  spench::DenoiseContext<float>* ctx = nullptr;
  ctx = test_data.m_denoiser->CreateContext();

  test_data.m_denoiser->SetContextId(ctx, 1000);
  REQUIRE(test_data.m_denoiser->GetContextId(ctx) == 1000);
  test_data.m_denoiser->SetContextId(ctx, 5000);
  REQUIRE(test_data.m_denoiser->GetContextId(ctx) == 5000);

  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with 100 tensors [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml").
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
    REQUIRE_NOTHROW(test_data.m_denoiser->ProcessChunk(ctx, chunk, ret));
  }

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Reload configuration from disk [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_tflite_16khz.yaml"));
#else
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
#endif
  REQUIRE_NOTHROW(test_data.m_denoiser->ReloadConfig());
}

TEST_CASE("SUCCEED Run inference with the best quality 16 kHz model and 2 contexts [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml").
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
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml").
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
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml").
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
                               InitModel("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_24khz.yaml").
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
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml").
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
                               InitModel("default_configuration_best_quality_tflite_16khz.yaml").
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_best_quality_onnx_16khz.yaml").
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
                               InitModel("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_24khz.yaml").
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
                               InitModel("default_configuration_fast_tflite_encrypted_24khz.yaml", "test").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_encrypted_24khz.yaml", "test").
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
#endif

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with the final inference chunk [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_24khz.yaml").
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

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with forward and backward resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  // const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
  //                                          * kAudio_dst_sample_rate / 1000;
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
  // std::vector<float> ret;
  // ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  // CHECK(ret_code == SpenchRetCode::SUCCESS);
  // REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);
  // std::cout << prediction.size() << std::endl;
  // prediction.insert(prediction.end(), ret.begin(), ret.end());
  // std::cout << prediction.size() << " and reference " << reference.size() << std::endl;
  // reference.resize(kTotal_size);
  // prediction.resize(kTotal_size);
  reference.resize(prediction.size());
  // std::cout << prediction.size() << " and reference " << reference.size() << std::endl;
  IsVectorsClose(reference, prediction, 0.005);
  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with forward resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  // const uint32_t kTotal_size = kChunk_size * kChunks_to_compare
  //                                          * kAudio_dst_sample_rate / 1000;
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
  // std::vector<float> ret;
  // ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  // CHECK(ret_code == SpenchRetCode::SUCCESS);
  // REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  // prediction.insert(prediction.end(), ret.begin(), ret.end());
  // reference.resize(kTotal_size);
  // prediction.resize(kTotal_size);
  reference.resize(prediction.size());
  IsVectorsClose(reference, prediction, 0.004);


  // reference_prediction.resize(prediction.size());
  // IsVectorsClose(reference_prediction, prediction, 0.003);
  // std::vector<float> ret;
  // ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  // CHECK(ret_code == SpenchRetCode::SUCCESS);
  // REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with backward resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_tflite_24khz.yaml").
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitModel("default_configuration_fast_onnx_24khz.yaml").
                               SetWav("test1_24_noised.wav"));
  #endif

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 10;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  SpenchRetCode ret_code;
  std::vector<float> prediction;
  std::vector<float> reference;
  spench::DenoiseContext<float>* ctx =
                          test_data.m_denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.m_wav_data,
                                24000,
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

  reference.resize(prediction.size());
  IsVectorsClose(reference, prediction, 0.003);
  // std::vector<float> ret;
  // ret_code = test_data.m_denoiser->GetFinalChunk(ctx, ret);

  // CHECK(ret_code == SpenchRetCode::SUCCESS);
  // REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  test_data.m_denoiser->DestroyContext(&ctx);
}
