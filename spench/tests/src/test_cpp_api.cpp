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

  TestFixture() {}
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

  TestFixture& InitModel(const std::string config_name) {
    std::string config_path = m_kBase + "config/" + config_name;
    m_denoiser = std::unique_ptr<spench::AudioDenoise>(new spench::AudioDenoise(config_path.c_str()));
    return *this;
  }
};

TEST_CASE("SUCCEED Create frame", "[spench c++ api]") {
  std::vector<float> input_chunk = {249.0, 250.0, 251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> gt_reversed_chunk = {0,     255.0, 254.0, 253.0, 252.0,
                                          251.0, 250.0, 249.0, 250.0,
                                          251.0, 252.0, 253.0, 254.0, 255.0};
  std::vector<float> reversed_chunk;
  spench::tools::MakeFirstInputFrame(input_chunk, reversed_chunk);
  IsVectorsClose(reversed_chunk, gt_reversed_chunk, 1.0);
}

TEST_CASE("SUCCEED Create context [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
  spench::DenoiseContext<float>* ctx = nullptr;
  ctx = test_data.m_denoiser->CreateContext();
  REQUIRE(ctx != nullptr);

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Retrieve model sample rate [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
  REQUIRE(test_data.m_denoiser->GetModelSampleRate() == 16000);
}

TEST_CASE("SUCCEED Set and retrieve a context id [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
  spench::DenoiseContext<float>* ctx = nullptr;
  ctx = test_data.m_denoiser->CreateContext();

  test_data.m_denoiser->SetContextId(ctx, 1000);
  REQUIRE(test_data.m_denoiser->GetContextId(ctx) == 1000);
  test_data.m_denoiser->SetContextId(ctx, 5000);
  REQUIRE(test_data.m_denoiser->GetContextId(ctx) == 5000);

  test_data.m_denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with 100 tensors [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml").
                        SetWav("rec_noise_001_16k.wav"));
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == 512);
    REQUIRE_NOTHROW(test_data.m_denoiser->ProcessChunk(ctx, chunk, ret));
  }

  test_data.m_denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Reload configuration from disk [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml"));
  REQUIRE_NOTHROW(test_data.m_denoiser->ReloadConfig());
}

TEST_CASE("SUCCEED Run inference with the best quality 16 kHz model and 2 contexts [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml").
                        SetWav("rec_noise_001_16k.wav"));
  std::vector<float> predictions1;
  std::vector<float> predictions2;
  spench::DenoiseContext<float>* ctx1 = test_data.m_denoiser->CreateContext();
  spench::DenoiseContext<float>* ctx2 = test_data.m_denoiser->CreateContext();
  CHECK(ctx1 != ctx2);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    test_data.m_denoiser->ProcessChunk(ctx1, chunk, ret1);
    test_data.m_denoiser->ProcessChunk(ctx2, chunk, ret2);
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
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml").
                        SetWav("rec_noise_001_16k.wav"));
  const uint16_t kNumber = 2;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<spench::DenoiseContext<float>*> contexts;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          spench::DenoiseContext<float>* ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
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
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_fast_onnx_24khz.yaml").
                        SetWav("rec_noise_001_16k.wav"));
  const uint16_t kNumber = 2;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<spench::DenoiseContext<float>*> contexts;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          spench::DenoiseContext<float>* ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      test_data.m_denoiser->ProcessChunk(ctx, chunk, ret);
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

TEST_CASE("FAIL Run inference with invalid chunk size", "[spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml").
                        SetWav("rec_noise_001_16k.wav"));
  std::vector<float> predictions;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();
  const uint32_t kChunks_to_compare = 1;
  const uint32_t kChunk_size = 20;
  const uint32_t kChunk_stride = 10;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  std::vector<float> ret;
  SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, chunks[0], ret);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
}

TEST_CASE("SUCCEED Run inference with the best quality model on a part of .wav file [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_best_quality_onnx_16khz.yaml").
                        SetWav("rec_noise_001_16k.wav"));
  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    std::vector<float>  input_frame;
    auto& chunk = chunks[i];
    if (i == 0) {
      spench::tools::MakeFirstInputFrame(chunk, input_frame);
    } else {
      auto& prev_chunk = chunks[i-1];
      // concat two chunks
      input_frame.assign(prev_chunk.begin(), prev_chunk.end());
      input_frame.insert(input_frame.end(), chunk.begin(), chunk.end());
    }
    std::vector<float> ret;
    test_data.m_denoiser->ProcessChunk(ctx, input_frame, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model on a part of .wav file [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                        InitModel("default_configuration_fast_onnx_24khz.yaml").
                        SetWav("rec_noise_001_24k.wav"));
  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::DenoiseContext<float>* ctx = test_data.m_denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    std::vector<float>  input_frame;
    auto& chunk = chunks[i];
    if (i == 0) {
      spench::tools::MakeFirstInputFrame(chunk, input_frame);
    } else {
      auto& prev_chunk = chunks[i-1];
      // concat two chunks
      input_frame.assign(prev_chunk.begin(), prev_chunk.end());
      input_frame.insert(input_frame.end(), chunk.begin(), chunk.end());
    }
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.m_denoiser->ProcessChunk(ctx, input_frame, ret);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}
