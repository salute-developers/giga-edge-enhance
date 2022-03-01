#include <memory>
#include <cstring>
#include <catch.hpp>
#include "include/spench_c_wrapper.h"
#include "tests/src/utils.h"
#include <chrono> // NOLINT

struct TestCAPI {
  std::vector<float> m_wav_data;
  SpenchModel m_denoiser = nullptr;

  std::string m_kBase = "data/";

  TestCAPI() {}
  TestCAPI& operator=(const TestCAPI&) = delete;
  TestCAPI(const TestCAPI&) = delete;
  TestCAPI(TestCAPI&& orig) :
          m_wav_data(std::move(orig.m_wav_data)),
          m_denoiser(std::move(orig.m_denoiser)) {
    orig.m_wav_data.clear();
    orig.m_denoiser = nullptr;
  }

  TestCAPI& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data, false);
    return *this;
  }

  TestCAPI& InitModel(const std::string config_name, SpenchRetCode& ret) {
    std::string config_path = m_kBase + "config/" + config_name;
    ret = SpenchModelInit(config_path.c_str(), &m_denoiser);
    return *this;
  }

  ~TestCAPI() {
    if (m_denoiser != nullptr) {
      SpenchModelFree(&m_denoiser);
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
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  SpenchCtx ctx = nullptr;
  ret_code = SpenchModelCreateContext(nullptr, &ctx);
  REQUIRE(ctx == nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Create and destroy context [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ctx != nullptr);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("FAIL Get a context id with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetContextId(nullptr, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a context id with nullptr context [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = SpenchModelGetContextId(test_data.m_denoiser, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a context id with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetContextId(test_data.m_denoiser, ctx, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Set a context id with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(nullptr, ctx, 1000);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Set a context id with nullptr context [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx, 1000);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Check that a default context id is zero [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetContextId(test_data.m_denoiser, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ret_id == 0);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Set and retrieve a context id [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  uint64_t ret_id = 0;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx, 1000);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetContextId(test_data.m_denoiser, ctx, &ret_id);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ret_id == 1000);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get a model sample rate with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  uint64_t sample_rate = 0;
  ret_code = SpenchModelGetSampleRate(nullptr, &sample_rate);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a model sample rate with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetSampleRate(test_data.m_denoiser, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get a model sample rate [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  uint64_t sample_rate = 0;

  ret_code = SpenchModelGetSampleRate(test_data.m_denoiser, &sample_rate);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(sample_rate == 16000);
}

TEST_CASE("FAIL Get a number of inference engine threads with nullptr model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  uint64_t ret_threads_num = 0;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetEngineThreads(nullptr, &ret_threads_num);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Get a number of inference engine threads with nullptr returning argument [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetEngineThreads(test_data.m_denoiser, nullptr);
  REQUIRE(ret_code == SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("SUCCEED Get a number of inference engine threads [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  SpenchCtx ctx = nullptr;
  uint64_t ret_threads_num = 0;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelGetEngineThreads(test_data.m_denoiser, &ret_threads_num);
  REQUIRE(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(ret_threads_num == 4);

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx);
}

TEST_CASE("FAIL Run inference with invalid chunk size with the best quality model 16kHz [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                             SetWav("rec_noise_001_16k.wav"));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 20;
  const uint32_t kChunk_stride = 10;
  unsigned char* ret;
  uint64_t ret_size = 0;
  std::vector<float> predictions(256);
  SpenchCtx ctx = nullptr;
  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);


  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(1);

  ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
                                     ctx,
                                     (const unsigned char*) chunks[0].data(),
                                     sizeof(float) * chunks[0].size(),
                                     &ret,
                                     &ret_size);

  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
}

TEST_CASE("SUCCEED Run inference with the best quality model [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                             SetWav("rec_noise_001_16k.wav"));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  SpenchCtx ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_16k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    std::vector<float>  input_frame;
    auto& chunk = chunks[i];
    if (i == 0) {
      input_frame.resize(chunk.size() * 2);
      ctools::SpenchMakeFirstInputFrame((unsigned char*)chunk.data(),
                                        chunk.size() * sizeof(float),
                                        (unsigned char*)input_frame.data(),
                                        input_frame.size() * sizeof(float));
    } else {
      auto& prev_chunk = chunks[i-1];
      // concat two chunks
      input_frame.assign(prev_chunk.begin(), prev_chunk.end());
      input_frame.insert(input_frame.end(), chunk.begin(), chunk.end());
    }
    std::vector<float> ret;
    ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
                                       ctx,
                                       (const unsigned char*) input_frame.data(),
                                       input_frame.size() * sizeof(float),
                                       &ret_mem,
                                       &ret_size);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}

TEST_CASE("SUCCEED Run inference with 100 tensors for 2 contexts and compare result [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                             SetWav("rec_noise_001_16k.wav"));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  SpenchCtx ctx1 = nullptr;
  SpenchCtx ctx2 = nullptr;
  std::vector<float> predictions1;
  std::vector<float> predictions2;

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx1);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx2);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx1, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx2, 2000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);
  CHECK(ctx1 != ctx2);

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    unsigned char* ret_mem1;
    uint64_t ret_size1 = 0;
    unsigned char* ret_mem2;
    uint64_t ret_size2 = 0;
    ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
                                     ctx1,
                                     (const unsigned char*) chunk.data(),
                                     sizeof(float) * chunk.size(),
                                     &ret_mem1,
                                     &ret_size1);
    CHECK(ret_code == SpenchRetCode::SUCCESS);

    ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
                                     ctx2,
                                     (const unsigned char*) chunk.data(),
                                     sizeof(float) * chunk.size(),
                                     &ret_mem2,
                                     &ret_size2);
    CHECK(ret_code == SpenchRetCode::SUCCESS);

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

  SpenchModelDestroyContext(test_data.m_denoiser, &ctx1);
  CHECK(ctx1 == nullptr);
  SpenchModelDestroyContext(test_data.m_denoiser, &ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference for the best quality model 16 khz and compare time with threshold [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                             SetWav("rec_noise_001_16k.wav"));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  // average and max inference time in milli seconds for 16ms chunk
  float avg_upperbound = 20.0f;  // ms
  float max_upperbound = 35.0f;  // ms

  SpenchCtx ctx = nullptr;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 16000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  std::vector<uint32_t> stat;

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;

    auto start = std::chrono::high_resolution_clock::now();
    ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
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

  uint32_t sum = std::accumulate(stat.begin(), stat.end(), 0);
  float avg_meausred = static_cast<float>(sum / stat.size()) / 1000000;
  float max_meausred = static_cast<float>(*max_element(stat.begin(), stat.end())) / 1000000;

  std::cout << "avg_measured: " << avg_meausred << " ms" << std::endl;
  std::cout << "max_measured: " << max_meausred << " ms" << std::endl;

  REQUIRE(avg_upperbound > avg_meausred);
  REQUIRE(max_upperbound > max_meausred);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model on a part of .wav file [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                             SetWav("rec_noise_001_24k.wav"));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  SpenchCtx ctx = nullptr;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    std::vector<float>  input_frame;
    auto& chunk = chunks[i];
    if (i == 0) {
      input_frame.resize(chunk.size() * 2);
      ctools::SpenchMakeFirstInputFrame((unsigned char*)chunk.data(),
                                        chunk.size() * sizeof(float),
                                        (unsigned char*)input_frame.data(),
                                        input_frame.size() * sizeof(float));
    } else {
      auto& prev_chunk = chunks[i-1];
      // concat two chunks
      input_frame.assign(prev_chunk.begin(), prev_chunk.end());
      input_frame.insert(input_frame.end(), chunk.begin(), chunk.end());
    }
    std::vector<float> ret;
    ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
                                       ctx,
                                       (const unsigned char*) input_frame.data(),
                                       input_frame.size() * sizeof(float),
                                       &ret_mem,
                                       &ret_size);
    CHECK(ret_code == SpenchRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

    std::free(ret_mem);
    ret_mem = nullptr;
  }

  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
}

TEST_CASE("SUCCEED Run inference for the fast 24 kHz model and compare time with threshold [spench c api]") {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().
                             InitModel("default_configuration_fast_onnx_24khz.yaml", ret_code).
                             SetWav("rec_noise_001_24k.wav"));
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  // average and max inference time in milli seconds for 16ms chunk
  float avg_upperbound = 10.0f;  // ms
  float max_upperbound = 12.0f;  // ms

  SpenchCtx ctx = nullptr;

  auto chunks = chunkWav<float>(test_data.m_wav_data, 24000, kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  ret_code = SpenchModelCreateContext(test_data.m_denoiser, &ctx);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  ret_code = SpenchModelSetContextId(test_data.m_denoiser, ctx, 1000);
  CHECK(ret_code == SpenchRetCode::SUCCESS);

  std::vector<uint32_t> stat;

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;

    auto start = std::chrono::high_resolution_clock::now();
    ret_code = SpenchModelProcessChunk(test_data.m_denoiser,
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

  uint32_t sum = std::accumulate(stat.begin(), stat.end(), 0);
  float avg_meausred = static_cast<float>(sum / stat.size()) / 1000000;
  float max_meausred = static_cast<float>(*max_element(stat.begin(), stat.end())) / 1000000;

  std::cout << "avg_measured: " << avg_meausred << " ms" << std::endl;
  std::cout << "max_measured: " << max_meausred << " ms" << std::endl;

  REQUIRE(avg_upperbound > avg_meausred);
  REQUIRE(max_upperbound > max_meausred);
}
