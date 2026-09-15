#include <gigadenoiser/c_api.h>

#include <catch.hpp>
#include <chrono>  // NOLINT
#include <cstring>
#include <memory>
#include <thread>  // NOLINT

#include "tests/src/utils.h"

TEST_CASE("C API initializes outputs [giga-denoiser c api]") {
  SECTION("failed model initialization clears the handle") {
    GigaDenoiserDenoise model = reinterpret_cast<GigaDenoiserDenoise>(1);
    REQUIRE(GigaDenoiserDenoiseInit(nullptr, &model) == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
    REQUIRE(model == nullptr);
  }

  SECTION("failed scalar query clears the output") {
    uint64_t sample_rate = 123;
    REQUIRE(GigaDenoiserDenoiseGetSampleRate(nullptr, &sample_rate) ==
            GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
    REQUIRE(sample_rate == 0);
  }
}

TEST_CASE("C API validates byte buffers without aligned reads [giga-denoiser c api]") {
  GigaDenoiserAudioProcess processor = nullptr;
  REQUIRE(GigaDenoiserAudioProcessInit("", &processor) == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(processor != nullptr);

  SECTION("non-divisible floating-point byte length is rejected") {
    const uint8_t input[sizeof(float)]{};
    uint8_t* output = reinterpret_cast<uint8_t*>(1);
    uint64_t output_size = 123;
    REQUIRE(GigaDenoiserAudioProcessChunkFp(processor, input, sizeof(input) - 1, &output, &output_size) ==
            GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
    REQUIRE(output == nullptr);
    REQUIRE(output_size == 0);
  }

  SECTION("unaligned floating-point input is copied safely") {
    const std::vector<float> samples{0.25F, -0.5F};
    std::vector<uint8_t> storage(1 + samples.size() * sizeof(float));
    std::memcpy(storage.data() + 1, samples.data(), samples.size() * sizeof(float));
    uint8_t* output = nullptr;
    uint64_t output_size = 0;
    REQUIRE(GigaDenoiserAudioProcessChunkFp(processor, storage.data() + 1, samples.size() * sizeof(float), &output,
                                            &output_size) == GigaDenoiserRetCode::SUCCESS);
    REQUIRE(output_size == samples.size() * sizeof(float));
    REQUIRE(std::memcmp(output, samples.data(), output_size) == 0);
    std::free(output);
  }

  SECTION("DSP constructor exceptions are contained") {
    REQUIRE(GigaDenoiserAudioProcessAddRcWithParam(processor, 0, -3.0F, 10.0F, 0.01F, 1.0F, 0.1F) ==
            GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  }

  GigaDenoiserAudioProcessFree(processor);
}

struct TestCAPI {
  std::vector<float> wav_data;
  std::vector<int16_t> pcm_wav_data;
  GigaDenoiserDenoise denoiser = nullptr;
  GigaDenoiserAudioProcess audio_proc = nullptr;

  std::string data_root = "data/";

  TestCAPI() {}
  TestCAPI& operator=(const TestCAPI&) = delete;
  TestCAPI(const TestCAPI&) = delete;
  TestCAPI(TestCAPI&& orig)
      : wav_data(std::move(orig.wav_data)),
        pcm_wav_data(std::move(orig.pcm_wav_data)),
        denoiser(std::move(orig.denoiser)),
        audio_proc(std::move(orig.audio_proc)) {
    orig.wav_data.clear();
    orig.pcm_wav_data.clear();
    orig.denoiser = nullptr;
    orig.audio_proc = nullptr;
  }

  TestCAPI& SetWav(const std::string& wav_path) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_data, false);
    return *this;
  }

  TestCAPI& SetPCMWav(const std::string& wav_path) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), pcm_wav_data);
    return *this;
  }

  TestCAPI& InitDenoiser(const std::string& config_name, GigaDenoiserRetCode& ret) {
    std::string config_path = data_root + "config/" + config_name;
    ret = GigaDenoiserDenoiseInit(config_path.c_str(), &denoiser);
    return *this;
  }

  TestCAPI& InitAudioProcessor(const std::string& config_name, GigaDenoiserRetCode& ret) {
    std::string config_path = data_root + "config/" + config_name;
    ret = GigaDenoiserAudioProcessInit(config_path.c_str(), &audio_proc);
    return *this;
  }
  ~TestCAPI() {
    if (denoiser != nullptr) {
      GigaDenoiserDenoiseFree(&denoiser);
    }
  }
};

TEST_CASE("FAIL Create model with incorrect config name [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().InitDenoiser("config.yaml", ret_code));
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}

TEST_CASE("FAIL Create model with incorrect model path [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().InitDenoiser("test_configuration_with_incorrect_model_path.yaml", ret_code));
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}

TEST_CASE("FAIL Create context with nullptr model [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif

  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
  GigaDenoiserContext ctx = nullptr;
  ret_code = GigaDenoiserDenoiseCreateContext(nullptr, &ctx, 0, 0);
  REQUIRE(ctx == nullptr);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Create and destroy context [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(ctx != nullptr);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("FAIL Get a context id with nullptr model [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetContextId(nullptr, ctx, &ret_id);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Get a context id with nullptr context [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = GigaDenoiserDenoiseGetContextId(test_data.denoiser, ctx, &ret_id);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Get a context id with nullptr returning argument [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetContextId(test_data.denoiser, ctx, nullptr);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Set a context id with nullptr model [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseSetContextId(nullptr, ctx, 1000);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Set a context id with nullptr context [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;

  ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Check that a default context id is zero [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  uint64_t ret_id = 1;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetContextId(test_data.denoiser, ctx, &ret_id);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(ret_id == 0);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("SUCCEED Set and retrieve a context id [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  uint64_t ret_id = 0;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetContextId(test_data.denoiser, ctx, &ret_id);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(ret_id == 1000);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Get a model sample rate with nullptr model [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  uint64_t sample_rate = 0;
  ret_code = GigaDenoiserDenoiseGetSampleRate(nullptr, &sample_rate);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a model sample rate with nullptr returning argument [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetSampleRate(test_data.denoiser, nullptr);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get a model sample rate [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  uint64_t sample_rate = 0;

  ret_code = GigaDenoiserDenoiseGetSampleRate(test_data.denoiser, &sample_rate);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(sample_rate == 48000);
}

TEST_CASE("FAIL Get a model input size with nullptr model [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  uint64_t input_size = 0;
  ret_code = GigaDenoiserDenoiseGetInputFrameSize(nullptr, &input_size);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("FAIL Get a model input size with nullptr returning argument [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetInputFrameSize(test_data.denoiser, nullptr);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get a model input size [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  uint64_t input_size = 0;

  ret_code = GigaDenoiserDenoiseGetInputFrameSize(test_data.denoiser, &input_size);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(input_size == (48000 * 16) / 1000);
}

TEST_CASE("FAIL Get a number of inference engine threads with nullptr model [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  uint64_t ret_threads_num = 0;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetEngineThreads(nullptr, &ret_threads_num);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Get a number of inference engine threads with nullptr returning argument [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetEngineThreads(test_data.denoiser, nullptr);
  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("SUCCEED Get a number of inference engine threads [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;
  uint64_t ret_threads_num = 0;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetEngineThreads(test_data.denoiser, &ret_threads_num);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(ret_threads_num == 1);

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Run inference with invalid chunk size with the best quality model 48kHz [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  const uint32_t chunk_size = 20;
  const uint32_t chunk_stride = 10;
  unsigned char* ret;
  uint64_t ret_size = 0;
  std::vector<float> predictions(256);
  GigaDenoiserContext ctx = nullptr;
  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);


  auto chunks = ChunkWav<float>(test_data.wav_data, 48000, chunk_size, chunk_stride);
  chunks.resize(1);

  ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser, ctx, (const unsigned char*)chunks[0].data(),
                                               sizeof(float) * chunks[0].size(), &ret, &ret_size);

  CHECK(ret_code == GigaDenoiserRetCode::ERR_INVALID_FRAME_SIZE);
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

// TEST_CASE("SUCCEED Run inference with the best quality model [giga-denoiser c api]") {
//   GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
//   #ifdef BUILD_TFLITE_ENGINE
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code).
//                                SetWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.015;
//   #else
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code).
//                                SetWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.005;
//   #endif
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   const uint32_t chunk_size = 16;
//   const uint32_t chunk_stride = 16;
//   const uint32_t chunks_to_compare = 300;
//   GigaDenoiserContext ctx = nullptr;
//   std::vector<float> prediction;
//   std::vector<float> reference_prediction;

//   auto chunks = ChunkWav<float>(test_data.wav_data, 48000, chunk_size, chunk_stride);
//   LoadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction);
//   chunks.resize(chunks_to_compare);

//   ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   for (uint32_t i = 0; i < chunks.size(); ++i) {
//     unsigned char* ret_mem;
//     uint64_t ret_size = 0;
//     auto& chunk = chunks[i];
//     std::vector<float> ret;
//     ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser,
//                                          ctx,
//                                          (const unsigned char*) chunk.data(),
//                                          chunk.size() * sizeof(float),
//                                          &ret_mem,
//                                          &ret_size);
//     CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
//     std::vector<float> ret_vec(ret_size / sizeof(float));
//     std::memcpy(ret_vec.data(), ret_mem, ret_size);
//     prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

//     std::free(ret_mem);
//     ret_mem = nullptr;
//   }
//   // erase first zero frame
//   const uint32_t warmup_samples = 768;
//   prediction.erase(prediction.begin(), prediction.begin() + warmup_samples);
//   reference_prediction.resize(prediction.size());
//   IsVectorsClose(reference_prediction, prediction, precision_threshold);
//   GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
// }

// TEST_CASE("SUCCEED Run inference with the PCM data [giga-denoiser c api]") {
//   GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
//   #ifdef BUILD_TFLITE_ENGINE
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.01;
//   #else
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.003;
//   #endif
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   const uint32_t chunk_size = 16;
//   const uint32_t chunk_stride = 16;
//   const uint32_t chunks_to_compare = 100;
//   GigaDenoiserContext ctx = nullptr;
//   std::vector<int16_t> prediction;
//   std::vector<float> prediction_fp;
//   std::vector<float> reference_prediction_fp;

//   auto chunks = ChunkWav<int16_t>(test_data.pcm_wav_data, 48000, chunk_size, chunk_stride);
//   LoadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction_fp);
//   chunks.resize(chunks_to_compare);

//   ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   for (uint32_t i = 0; i < chunks.size(); ++i) {
//     unsigned char* ret_mem;
//     uint64_t ret_size = 0;
//     auto& chunk = chunks[i];
//     std::vector<int16_t> ret;
//     ret_code = GigaDenoiserDenoiseProcessChunkInt(test_data.denoiser,
//                                          ctx,
//                                          (const unsigned char*) chunk.data(),
//                                          chunk.size() * sizeof(int16_t),
//                                          &ret_mem,
//                                          &ret_size);

//     CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
//     std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
//     std::memcpy(ret_vec.data(), ret_mem, ret_size);
//     prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

//     std::free(ret_mem);
//     ret_mem = nullptr;
//   }
//   // convert from pcm16 to fp
//   for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
//     float f = prediction[i] > 0 ?
//           static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max():
//           static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
//     prediction_fp.emplace_back(f);
//   }
//   // erase first zero frame
//   const uint32_t warmup_samples = 768;
//   prediction_fp.erase(prediction_fp.begin(), prediction_fp.begin() + warmup_samples);
//   reference_prediction_fp.resize(prediction_fp.size());
//   IsVectorsClose(reference_prediction_fp, prediction_fp, precision_threshold);
//   GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
// }

// TEST_CASE("SUCCEED Get a last chunk with the best quality model [giga-denoiser c api]") {
//   GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
//   #ifdef BUILD_TFLITE_ENGINE
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code).
//                                SetWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.01;
//   #else
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code).
//                                SetWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.003;
//   #endif
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   const uint32_t chunk_size = 16;
//   const uint32_t chunk_stride = 16;
//   const uint32_t chunks_to_compare = 10;
//   const uint32_t audio_sample_rate = 48000;
//   GigaDenoiserContext ctx = nullptr;
//   std::vector<float> prediction;
//   std::vector<float> reference;

//   auto chunks = ChunkWav<float>(test_data.wav_data, audio_sample_rate, chunk_size, chunk_stride);
//   LoadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference);
//   chunks.resize(chunks_to_compare);

//   ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   for (uint32_t i = 0; i < chunks.size(); ++i) {
//     unsigned char* ret_mem;
//     uint64_t ret_size = 0;
//     auto& chunk = chunks[i];
//     std::vector<float> ret;
//     ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser,
//                                          ctx,
//                                          (const unsigned char*) chunk.data(),
//                                          chunk.size() * sizeof(float),
//                                          &ret_mem,
//                                          &ret_size);
//     CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
//     std::vector<float> ret_vec(ret_size / sizeof(float));
//     std::memcpy(ret_vec.data(), ret_mem, ret_size);
//     prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

//     std::free(ret_mem);
//     ret_mem = nullptr;
//   }
//   // erase first zero frame
//   const uint32_t warmup_samples = 768;
//   prediction.erase(prediction.begin(), prediction.begin() + warmup_samples);
//   reference.resize(prediction.size());
//   IsVectorsClose(reference, prediction, precision_threshold);

//   unsigned char* final_ret_mem;
//   uint64_t final_ret_size = 0;
//   ret_code = GigaDenoiserDenoiseGetFinalChunkFp(test_data.denoiser,
//                                         ctx,
//                                         &final_ret_mem,
//                                         &final_ret_size);
//   CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
//   std::vector<float> final_vec(final_ret_size / sizeof(float));
//   std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

//   std::free(final_ret_mem);
//   final_ret_mem = nullptr;

//   REQUIRE(std::vector<float>(final_vec.size(), 0.0) != final_vec);
//   GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
// }

// TEST_CASE("SUCCEED Get a last chunk with the best quality model and with the PCM data [giga-denoiser c api]") {
//   GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
//   #ifdef BUILD_TFLITE_ENGINE
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.01;
//   #else
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.003;
//   #endif
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   const uint32_t chunk_size = 16;
//   const uint32_t chunk_stride = 16;
//   const uint32_t chunks_to_compare = 10;
//   GigaDenoiserContext ctx = nullptr;
//   std::vector<int16_t> prediction;
//   std::vector<float> prediction_fp;
//   std::vector<float> reference_prediction_fp;

//   auto chunks = ChunkWav<int16_t>(test_data.pcm_wav_data, 48000, chunk_size, chunk_stride);
//   LoadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction_fp);
//   chunks.resize(chunks_to_compare);

//   ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   for (uint32_t i = 0; i < chunks.size(); ++i) {
//     unsigned char* ret_mem;
//     uint64_t ret_size = 0;
//     auto& chunk = chunks[i];
//     std::vector<int16_t> ret;
//     ret_code = GigaDenoiserDenoiseProcessChunkInt(test_data.denoiser,
//                                          ctx,
//                                          (const unsigned char*) chunk.data(),
//                                          chunk.size() * sizeof(int16_t),
//                                          &ret_mem,
//                                          &ret_size);
//     CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
//     std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
//     std::memcpy(ret_vec.data(), ret_mem, ret_size);
//     prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

//     std::free(ret_mem);
//     ret_mem = nullptr;
//   }
//   // convert from pcm16 to fp
//   for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
//     float f = prediction[i] > 0 ?
//           static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max():
//           static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
//     prediction_fp.emplace_back(f);
//   }
//   // erase first zero frame
//   const uint32_t warmup_samples = 768;
//   prediction_fp.erase(prediction_fp.begin(), prediction_fp.begin() + warmup_samples);
//   reference_prediction_fp.resize(prediction_fp.size());
//   IsVectorsClose(reference_prediction_fp, prediction_fp, precision_threshold);

//   unsigned char* final_ret_mem;
//   uint64_t final_ret_size = 0;
//   ret_code = GigaDenoiserDenoiseGetFinalChunkInt(test_data.denoiser,
//                                         ctx,
//                                         &final_ret_mem,
//                                         &final_ret_size);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
//   std::vector<int16_t> final_vec(final_ret_size / sizeof(int16_t));
//   std::memcpy(final_vec.data(), final_ret_mem, final_ret_size);

//   std::free(final_ret_mem);
//   final_ret_mem = nullptr;

//   REQUIRE(std::vector<int16_t>(final_vec.size(), 0) != final_vec);

//   GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
// }

// TEST_CASE("SUCCEED Run inference with model 48kHz on a part of .wav with double chunks [giga-denoiser c api]") {
//   GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
//   #ifdef BUILD_TFLITE_ENGINE
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.015;
//   #else
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.005;
//   #endif
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   const uint32_t chunk_size = 32;
//   const uint32_t chunk_stride = chunk_size;
//   const uint32_t chunks_to_compare = 150;
//   GigaDenoiserContext ctx = nullptr;
//   std::vector<int16_t> prediction;
//   std::vector<float> prediction_fp;
//   std::vector<float> reference_prediction_fp;

//   auto chunks = ChunkWav<int16_t>(test_data.pcm_wav_data, 48000, chunk_size, chunk_stride);
//   LoadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction_fp);
//   chunks.resize(chunks_to_compare);

//   ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   for (uint32_t i = 0; i < chunks.size(); ++i) {
//     unsigned char* ret_mem;
//     uint64_t ret_size = 0;
//     auto& chunk = chunks[i];

//     ret_code = GigaDenoiserDenoiseProcessChunkInt(test_data.denoiser,
//                                             ctx,
//                                             (const unsigned char*) chunk.data(),
//                                             chunk.size() * sizeof(int16_t),
//                                             &ret_mem,
//                                             &ret_size);
//     CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

//     std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
//     std::memcpy(ret_vec.data(), ret_mem, ret_size);
//     prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

//     std::free(ret_mem);
//     ret_mem = nullptr;
//   }

//   for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
//     float f = prediction[i] > 0 ?
//           static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max():
//           static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
//     prediction_fp.emplace_back(f);
//   }

//   reference_prediction_fp.resize(prediction_fp.size());
//   IsVectorsClose(reference_prediction_fp, prediction_fp, precision_threshold);

//   GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
// }

// TEST_CASE("SUCCEED Run inference with model 48kHz on a part of .wav with chunks of various size [giga-denoiser c
// api]") {
//   GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
//   #ifdef BUILD_TFLITE_ENGINE
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.015;
//   #else
//     auto test_data = std::move(TestCAPI().
//                                InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code).
//                                SetPCMWav("rec_noise_001_48k.wav"));
//     const float precision_threshold = 0.005;
//   #endif
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   const uint32_t chunk_size = 16;
//   const uint32_t chunk_stride = chunk_size;
//   const uint32_t chunks_to_compare = 300;
//   GigaDenoiserContext ctx = nullptr;
//   std::vector<int16_t> prediction;
//   std::vector<float> prediction_fp;
//   std::vector<float> reference_prediction_fp;

//   auto chunks = ChunkWav<int16_t>(test_data.pcm_wav_data, 48000, chunk_size, chunk_stride);
//   chunks.resize(chunks_to_compare);

//   LoadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction_fp);

//   ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
//   CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

//   // 20*8 + 20*4 + 10*3 + 15*2 = 160 + 80 + 30 + 30 = 300
//   // constexpr std::size_t num_groups = 65;
//   const std::array<uint32_t, 65> merge_factors = [] {
//     std::array<uint32_t, 65> a{};
//     std::size_t idx = 0;

//     for (uint32_t i = 0; i < 20; ++i) a[idx++] = 8;
//     for (uint32_t i = 0; i < 20; ++i) a[idx++] = 4;
//     for (uint32_t i = 0; i < 10; ++i) a[idx++] = 3;
//     for (uint32_t i = 0; i < 15; ++i) a[idx++] = 2;

//     return a;
//   }();

//   std::size_t chunk_pos = 0;
//   for (uint32_t factor : merge_factors) {
//     auto merged_chunk = MergeChunks(chunks, chunk_pos, factor);
//     chunk_pos += factor;

//     unsigned char* ret_mem;
//     uint64_t ret_size = 0;
//     ret_code = GigaDenoiserDenoiseProcessChunkInt(test_data.denoiser,
//                                             ctx,
//                                             (const unsigned char*) merged_chunk.data(),
//                                             merged_chunk.size() * sizeof(int16_t),
//                                             &ret_mem,
//                                             &ret_size);
//     CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

//     std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
//     std::memcpy(ret_vec.data(), ret_mem, ret_size);
//     prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());

//     std::free(ret_mem);
//     ret_mem = nullptr;
//   }

//   for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
//     float f = prediction[i] > 0 ?
//           static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max():
//           static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
//     prediction_fp.emplace_back(f);
//   }

//   reference_prediction_fp.resize(prediction_fp.size());
//   IsVectorsClose(reference_prediction_fp, prediction_fp, precision_threshold);

//   GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
// }

TEST_CASE("SUCCEED Run inference with model 48kHz on huge single chunk (4000 frames) [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code)
                                 .SetPCMWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code)
                                 .SetPCMWav("test1_48_noised.wav"));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  GigaDenoiserContext ctx = nullptr;

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = chunk_size;
  const std::size_t target_chunks = 4000;

  auto chunks = ChunkWav<int16_t>(test_data.pcm_wav_data, 48000, chunk_size, chunk_stride);

  REQUIRE_FALSE(chunks.empty());

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  const std::size_t base_chunks = chunks.size();
  const std::size_t repeats = (target_chunks + base_chunks - 1) / base_chunks;

  const std::size_t samples_per_chunk = chunks.front().size();

  std::vector<int16_t> huge_input;
  huge_input.reserve(target_chunks * samples_per_chunk);

  std::size_t filled = 0;
  for (std::size_t r = 0; r < repeats && filled < target_chunks; ++r) {
    for (const auto& ch : chunks) {
      huge_input.insert(huge_input.end(), ch.begin(), ch.end());
      if (++filled == target_chunks) break;
    }
  }

  REQUIRE(filled == target_chunks);
  REQUIRE(huge_input.size() == target_chunks * samples_per_chunk);

  unsigned char* ret_mem = nullptr;
  uint64_t ret_size = 0;

  REQUIRE_NOTHROW(
      ret_code = GigaDenoiserDenoiseProcessChunkInt(test_data.denoiser, ctx, (const unsigned char*)huge_input.data(),
                                                    huge_input.size() * sizeof(int16_t), &ret_mem, &ret_size));
  CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
  REQUIRE(ret_mem != nullptr);
  REQUIRE(ret_size > 0);

  std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
  std::memcpy(ret_vec.data(), ret_mem, ret_size);

  std::free(ret_mem);
  ret_mem = nullptr;

  REQUIRE_FALSE(ret_vec.empty());

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Get statistic information with nullptr stat structure [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif

  GigaDenoiserContext ctx = nullptr;

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseGetStreamStat(test_data.denoiser, ctx, nullptr);

  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
}

TEST_CASE("FAIL Get statistic information with nullptr context [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code));
#else
  auto test_data = std::move(TestCAPI().InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code));
#endif
  StreamStatStruct stream_stat;
  ret_code = GigaDenoiserDenoiseGetStreamStat(test_data.denoiser, nullptr, &stream_stat);

  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT);
}

TEST_CASE("SUCCEED Get statistic information [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t chunks_to_compare = 100;
  GigaDenoiserContext ctx = nullptr;
  std::vector<float> predictions1;

  auto chunks = ChunkWav<float>(test_data.wav_data, 48000, chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;
    ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser, ctx, (const unsigned char*)chunk.data(),
                                                 sizeof(float) * chunk.size(), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
    StreamStatStruct stream_stat;
    ret_code = GigaDenoiserDenoiseGetStreamStat(test_data.denoiser, ctx, &stream_stat);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

    free(ret_mem);
    ret_mem = NULL;
  }

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
  CHECK(ctx == nullptr);
}

TEST_CASE("SUCCEED Run inference with 100 tensors for 2 contexts and compare result [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t chunks_to_compare = 100;
  GigaDenoiserContext ctx1 = nullptr;
  GigaDenoiserContext ctx2 = nullptr;
  std::vector<float> predictions1;
  std::vector<float> predictions2;

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx1, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx2, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx1, 1000);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx2, 2000);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
  CHECK(ctx1 != ctx2);

  auto chunks = ChunkWav<float>(test_data.wav_data, 48000, chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  for (auto& chunk : chunks) {
    unsigned char* ret_mem1;
    uint64_t ret_size1 = 0;
    unsigned char* ret_mem2;
    uint64_t ret_size2 = 0;
    ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser, ctx1, (const unsigned char*)chunk.data(),
                                                 sizeof(float) * chunk.size(), &ret_mem1, &ret_size1);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

    ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser, ctx2, (const unsigned char*)chunk.data(),
                                                 sizeof(float) * chunk.size(), &ret_mem2, &ret_size2);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

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

  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx1);
  CHECK(ctx1 == nullptr);
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with the best quality 48 kHz model and 5 contexts concurently [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_tflite_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestCAPI()
                                 .InitDenoiser("default_configuration_best_quality_onnx_48khz.yaml", ret_code)
                                 .SetWav("test1_48_noised.wav"));
#endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t chunks_to_compare = 100;
  const uint16_t context_count = 5;
  std::vector<std::vector<float>> predictions(context_count);
  std::vector<std::thread> pool;
  std::vector<GigaDenoiserContext> contexts;

  auto chunks = ChunkWav<float>(test_data.wav_data, 48000, chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  auto functor = [&test_data, &chunks](GigaDenoiserContext ctx, std::vector<float>& predictions) -> void {
    for (auto& chunk : chunks) {
      GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
      unsigned char* ret_mem;
      uint64_t ret_size = 0;
      std::vector<float> ret;
      ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser, ctx, (const unsigned char*)chunk.data(),
                                                   sizeof(float) * chunk.size(), &ret_mem, &ret_size);
      CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
      std::vector<float> ret_vec(ret_size / sizeof(float));
      std::memcpy(ret_vec.data(), ret_mem, ret_size);
      predictions.insert(predictions.end(), ret_vec.begin(), ret_vec.end());

      std::free(ret_mem);
      ret_mem = nullptr;
    }
  };

  for (uint32_t i = 0; i < context_count; ++i) {
    GigaDenoiserContext ctx = nullptr;
    ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
    CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
    ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, i);
    CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);
    contexts.emplace_back(ctx);
    pool.emplace_back(std::thread(functor, contexts.back(), std::ref(predictions.at(i))));
  }

  for (uint32_t i = 0; i < context_count; ++i) {
    if (pool[i].joinable()) {
      pool[i].join();
    }
  }

  for (uint32_t i = 0; i < context_count - 1; ++i) {
    REQUIRE(predictions[i] == predictions[i + 1]);
  }

  for (uint32_t i = 0; i < context_count; ++i) {
    GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &(contexts[i]));
    CHECK(contexts[i] == nullptr);
  }
}

/*
#ifndef BUILD_WITH_ADDRESS_SANITIZER
TEST_CASE("SUCCEED Run inference for the best quality model 16 khz and compare time with threshold [giga-denoiser c
api]") { GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS; #ifdef BUILD_TFLITE_ENGINE auto test_data =
std::move(TestCAPI(). InitDenoiser("default_configuration_best_quality_tflite_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestCAPI().
                               InitDenoiser("default_configuration_best_quality_onnx_16khz.yaml", ret_code).
                               SetWav("rec_noise_001_16k.wav"));
  #endif
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t chunks_to_compare = 100;
  // average and max inference time in milli seconds for 16ms chunk
  float avg_upperbound = 25.0f;  // ms
  float max_upperbound = 40.0f;  // ms

  GigaDenoiserContext ctx = nullptr;

  auto chunks = ChunkWav<float>(test_data.wav_data, 16000, chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  ret_code = GigaDenoiserDenoiseCreateContext(test_data.denoiser, &ctx, 0, 0);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  ret_code = GigaDenoiserDenoiseSetContextId(test_data.denoiser, ctx, 1000);
  CHECK(ret_code == GigaDenoiserRetCode::SUCCESS);

  std::vector<uint32_t> stat;

  for (auto& chunk : chunks) {
    unsigned char* ret_mem;
    uint64_t ret_size = 0;

    auto start = std::chrono::high_resolution_clock::now();
    ret_code = GigaDenoiserDenoiseProcessChunkFp(test_data.denoiser,
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
  GigaDenoiserDenoiseDestroyContext(test_data.denoiser, &ctx);
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
TEST_CASE("FAIL Run AudioProcessing with incorrect config file [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().InitAudioProcessor("config.yaml", ret_code));

  REQUIRE(ret_code == GigaDenoiserRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading RC parameters from config [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data =
      std::move(TestCAPI().InitAudioProcessor("sound_processing_rc.yaml", ret_code).SetWav("test1_48_noised.wav"));
  GigaDenoiserAudioProcessAddRc(test_data.audio_proc);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    ret_code = GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, (const unsigned char*)chunk.data(),
                                               chunk.size() * sizeof(float), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);
  REQUIRE(greater_than_0_9.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.9
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);

  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}

TEST_CASE("SUCCEED Run AudioProcessing for pcm16 data with reading RC parameters from config [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data =
      std::move(TestCAPI().InitAudioProcessor("sound_processing_rc.yaml", ret_code).SetPCMWav("test1_48_noised.wav"));
  GigaDenoiserAudioProcessAddRc(test_data.audio_proc);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<int16_t> prediction;
  std::vector<float> prediction_fp;
  std::vector<float> reference_fp;
  auto chunks = ChunkWav<int16_t>(test_data.pcm_wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  LoadWavFx("data/wavs/test1_48_noised.wav", reference_fp);
  reference_fp.resize(source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    ret_code = GigaDenoiserAudioProcessChunkInt(test_data.audio_proc, (const unsigned char*)chunk.data(),
                                                chunk.size() * sizeof(int16_t), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
    std::vector<int16_t> ret_vec(ret_size / sizeof(int16_t));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }

  // convert from pcm16 to fp
  for (std::vector<int16_t>::size_type i = 0; i != prediction.size(); i++) {
    float f = prediction[i] > 0 ? static_cast<float>(prediction[i]) / std::numeric_limits<int16_t>::max()
                                : static_cast<float>(prediction[i]) / -std::numeric_limits<int16_t>::min();
    prediction_fp.emplace_back(f);
  }

  REQUIRE(source_data_size == prediction_fp.size());

  std::copy_if(std::begin(reference_fp), std::end(reference_fp), std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);
  REQUIRE(greater_than_0_9.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.9
  std::copy_if(std::begin(prediction_fp), std::end(prediction_fp), std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);
  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}

TEST_CASE("SUCCEED Run AudioProcessing with changed RC parameters [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data =
      std::move(TestCAPI().InitAudioProcessor("sound_processing_rc.yaml", ret_code).SetWav("test1_48_noised.wav"));

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_8;
  chunks.resize(chunks_to_compare);
  GigaDenoiserAudioProcessAddRcWithParam(test_data.audio_proc, 48000, -4, 10, 0.05, 300, 5);
  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    ret_code = GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, (const unsigned char*)chunk.data(),
                                               chunk.size() * sizeof(float), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_0_8),
               [](float& i) { return std::fabs(i) > 0.8; });

  REQUIRE(greater_than_0_8.size() != 0);
  greater_than_0_8.resize(0);
  REQUIRE(greater_than_0_8.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.8
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_0_8),
               [](float& i) { return std::fabs(i) > 0.8; });
  REQUIRE(greater_than_0_8.size() == 0);
  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}

TEST_CASE("SUCCEED Run AudioProcessing with changed LC parameters [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().InitAudioProcessor("sound_processing_rc.yaml", ret_code));

  constexpr uint32_t fs = 16000;
  constexpr uint32_t chunk_ms = 32;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (12 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;
  const int32_t bypass_time = 3;
  const int32_t target_db = -23;
  const int32_t max_gain_db = 40;
  const uint32_t use_vad = false;

  GigaDenoiserAudioProcessAddLcWithParam(test_data.audio_proc, fs, target_db, bypass_time, max_gain_db, use_vad, 0);

  const auto tt = dsplib::arange(num_samples) / fs;
  const auto in = 0.5 * sin(2 * dsplib::pi * 440.0 * tt);

  auto rms_before = dsplib::mag2db(dsplib::rms(in));

  // process frame by frame
  dsplib::arr_real out;

  for (uint32_t i = 0; i < num_frames; ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    const uint32_t t1 = i * frame_len;
    const uint32_t t2 = t1 + frame_len;
    const auto x = *in.slice(t1, t2);
    std::vector<float> chunk = x.to_vec();

    ret_code = GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, (const unsigned char*)chunk.data(),
                                               chunk.size() * sizeof(float), &ret_mem, &ret_size);
    // Memory to vector
    std::vector<float> ret(ret_size / sizeof(float));
    std::memcpy(ret.data(), ret_mem, ret_size);
    // Vector to dsplib::arr_real
    out |= dsplib::arr_real(ret);

    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }

  // rms changed (after bypass time)
  {
    auto rms_gained = dsplib::mag2db(dsplib::rms(out.slice(fs * bypass_time, num_samples)));
    const dsplib::real_t rms_target = -22.31;
    REQUIRE(std::abs(rms_gained - rms_target) < 0.5);
  }

  // rms not changed (before bypass time)
  {
    auto rms_bypassed = dsplib::mag2db(dsplib::rms(out.slice(0, fs * bypass_time)));
    REQUIRE(std::abs(rms_bypassed - rms_before) < 0.5);
  }

  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}

TEST_CASE("SUCCEED VAD LC does not normalize pure noise to target [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data = std::move(TestCAPI().InitAudioProcessor("sound_processing_rc.yaml", ret_code));
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
  REQUIRE(test_data.audio_proc != nullptr);

  constexpr uint32_t fs = 16000;
  constexpr uint32_t chunk_ms = 32;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (8 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;

  const float bypass_time = 0.0f;
  const float target_db = -15.0f;
  const float max_gain_db = 30.0f;
  const uint32_t use_vad = 1;

  ret_code = GigaDenoiserAudioProcessAddLcWithParam(test_data.audio_proc, fs, target_db, bypass_time, max_gain_db,
                                                    use_vad, 100);
  REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);

  // Pure noise for the whole duration.
  const auto in = Noise(num_samples, 0.01f);

  dsplib::arr_real out;
  for (uint32_t i = 0; i < static_cast<uint32_t>(num_frames); ++i) {
    const uint32_t t1 = i * frame_len;
    const uint32_t t2 = t1 + frame_len;
    const auto x = *in.slice(t1, t2);
    std::vector<float> chunk = x.to_vec();

    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    ret_code =
        GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, reinterpret_cast<const unsigned char*>(chunk.data()),
                                        chunk.size() * sizeof(float), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

    std::vector<float> ret_vec(ret_size / sizeof(float));
    if (ret_size != 0) {
      std::memcpy(ret_vec.data(), ret_mem, ret_size);
    }
    out |= dsplib::arr_real(ret_vec);

    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }

  // If VAD works, pure noise should not be driven close to target_db.
  const float rms_out = RmsDb(out);
  REQUIRE(std::abs(rms_out - target_db) > 6.0f);

  // Also, the output should not be excessively amplified vs input.
  const float rms_in = RmsDb(in);
  REQUIRE((rms_out - rms_in) < 10.0f);

  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}

TEST_CASE("SUCCEED VAD LC reaches target for speech under different input gains [giga-denoiser c api]") {
  constexpr uint32_t fs = 16000;
  constexpr uint32_t chunk_ms = 32;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (10 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;

  const float bypass_time = 0.0f;
  const float target_db = -15.0f;
  const float max_gain_db = 30.0f;
  const uint32_t use_vad = 1;

  // "Speech-like" synthetic signal.
  const auto base = Tone(fs, num_samples, 220.0f, 0.05f) + Tone(fs, num_samples, 440.0f, 0.02f);

  const dsplib::arr_real gains_db = {-20, -10, 0, 10};
  for (auto gdb : gains_db) {
    GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
    auto test_data = std::move(TestCAPI().InitAudioProcessor("sound_processing_rc.yaml", ret_code));
    REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);
    REQUIRE(test_data.audio_proc != nullptr);

    ret_code = GigaDenoiserAudioProcessAddLcWithParam(test_data.audio_proc, fs, target_db, bypass_time, max_gain_db,
                                                      use_vad, gdb * 100);
    REQUIRE(ret_code == GigaDenoiserRetCode::SUCCESS);

    const auto in = base * dsplib::db2mag(gdb);

    dsplib::arr_real out;
    for (uint32_t i = 0; i < static_cast<uint32_t>(num_frames); ++i) {
      const uint32_t t1 = i * frame_len;
      const uint32_t t2 = t1 + frame_len;
      const auto x = *in.slice(t1, t2);
      std::vector<float> chunk = x.to_vec();

      unsigned char* ret_mem = nullptr;
      uint64_t ret_size = 0;
      ret_code =
          GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, reinterpret_cast<const unsigned char*>(chunk.data()),
                                          chunk.size() * sizeof(float), &ret_mem, &ret_size);
      CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);

      std::vector<float> ret_vec(ret_size / sizeof(float));
      if (ret_size != 0) {
        std::memcpy(ret_vec.data(), ret_mem, ret_size);
      }
      out |= dsplib::arr_real(ret_vec);

      if (ret_mem != nullptr) {
        std::free(ret_mem);
        ret_mem = nullptr;
      }
    }

    // Use the tail segment to allow the controller to accumulate stats and settle.
    const int32_t t_start = 4 * static_cast<int32_t>(fs);
    const auto out_tail = out.slice(t_start, num_samples);
    const float rms_tail = RmsDb(out_tail);
    REQUIRE(std::abs(rms_tail - target_db) < 4.0f);

    GigaDenoiserAudioProcessFree(test_data.audio_proc);
    test_data.audio_proc = nullptr;
  }
}

TEST_CASE("SUCCEED Run AudioProcessing with reading AGC parameters from config [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data =
      std::move(TestCAPI().InitAudioProcessor("sound_processing_agc.yaml", ret_code).SetWav("test1_48_noised.wav"));
  GigaDenoiserAudioProcessAddAgc(test_data.audio_proc);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, (const unsigned char*)chunk.data(),
                                               chunk.size() * sizeof(float), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_1),
               [](float& i) { return std::fabs(i) > 1.0; });

  REQUIRE(greater_than_1.size() == 0);
  // AGC could increase signal upto 1.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_1),
               [](float& i) { return std::fabs(i) > 1.0; });
  REQUIRE(greater_than_1.size() != 0);
  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}

TEST_CASE("SUCCEED Run AudioProcessing with changed AGC parameters [giga-denoiser c api]") {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  auto test_data =
      std::move(TestCAPI().InitAudioProcessor("sound_processing_agc.yaml", ret_code).SetWav("test1_48_noised.wav"));

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  chunks.resize(chunks_to_compare);
  GigaDenoiserAudioProcessAddAgcWithParam(test_data.audio_proc, 48000, 0.9, 20.0);
  for (uint32_t i = 0; i < chunks.size(); ++i) {
    unsigned char* ret_mem = nullptr;
    uint64_t ret_size = 0;
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = GigaDenoiserAudioProcessChunkFp(test_data.audio_proc, (const unsigned char*)chunk.data(),
                                               chunk.size() * sizeof(float), &ret_mem, &ret_size);
    CHECK(ret_code >= GigaDenoiserRetCode::SUCCESS);
    std::vector<float> ret_vec(ret_size / sizeof(float));
    std::memcpy(ret_vec.data(), ret_mem, ret_size);
    prediction.insert(prediction.end(), ret_vec.begin(), ret_vec.end());
    if (ret_mem != nullptr) {
      std::free(ret_mem);
      ret_mem = nullptr;
    }
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
  GigaDenoiserAudioProcessFree(test_data.audio_proc);
  test_data.audio_proc = nullptr;
}
