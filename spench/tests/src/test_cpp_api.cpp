#include <memory>
#include <catch.hpp>
#include <spench.h>
#include "spench/src/spench_task.h"
#include "spench/src/spench_default_configurations.h"
#include "tests/src/utils.h"
#include <filesystem>
#include <future> // NOLINT

struct TestFixture {
  std::vector<float> wav_data;
  std::vector<float> wav_lbk_data;
  std::unique_ptr<spench::SpenchDenoise> denoiser;
  std::unique_ptr<spench::SpenchAudioDereverb> dereverb;
  std::unique_ptr<spench::SpenchAudioProcess> dsp;
  std::unique_ptr<spench::SpenchAudioEchoCancelation> aec;

  const std::string kBase = "data/";

  TestFixture() :
          denoiser(nullptr),
          dereverb(nullptr),
          dsp(nullptr),
          aec(nullptr) {}
  ~TestFixture() {
    denoiser.reset();
    dereverb.reset();
    dsp.reset();
    aec.reset();
  }
  TestFixture& operator=(const TestFixture&) = delete;
  TestFixture(const TestFixture&) = delete;
  TestFixture(TestFixture&& orig) :
          wav_data(std::move(orig.wav_data)),
          wav_lbk_data(std::move(orig.wav_lbk_data)),
          denoiser(std::move(orig.denoiser)),
          dereverb(std::move(orig.dereverb)),
          dsp(std::move(orig.dsp)),
          aec(std::move(orig.aec)) {
    orig.wav_data.clear();
    orig.wav_lbk_data.clear();
    orig.denoiser = nullptr;
    orig.dereverb = nullptr;
    orig.dsp = nullptr;
    orig.aec = nullptr;
  }
  TestFixture& SetWav(const std::string& wav_path) {
    std::string full_wav_path = kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), wav_data, false);
    return *this;
  }
  TestFixture& SetLbk(const std::string& wav_lbk_path) {
    std::string full_wav_path = kBase + "wavs/" + wav_lbk_path;
    loadWavFx(full_wav_path.c_str(), wav_lbk_data, false);
    return *this;
  }

  TestFixture& InitDenoiser(const std::filesystem::path& config_name,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    std::filesystem::path config_path = kBase + "config/";
    config_path += config_name;
    const std::string cfg = config_path.string();  // narrow on Windows
    denoiser = std::make_unique<spench::SpenchDenoise>(cfg.c_str(),
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitDenoiser(const std::string& yaml,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    denoiser = std::make_unique<spench::SpenchDenoise>(yaml,
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitDenoiser(std::shared_ptr<spench::PackedModelReader> bundle,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    denoiser = std::make_unique<spench::SpenchDenoise>(bundle,
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitDereverberator(const std::filesystem::path& config_name,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    std::filesystem::path config_path = kBase + "config/";
    config_path += config_name;
    const std::string cfg = config_path.string();  // narrow on Windows
    dereverb = std::make_unique<spench::SpenchAudioDereverb>(cfg.c_str(),
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitDereverberator(const std::string& yaml,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    dereverb = std::make_unique<spench::SpenchAudioDereverb>(yaml,
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitDereverberator(std::shared_ptr<spench::PackedModelReader> bundle,
                            const std::string& password = "",
                            const std::string& checksum = "") {
    dereverb = std::make_unique<spench::SpenchAudioDereverb>(bundle,
                                                        password.c_str(),
                                                        checksum.c_str());
    return *this;
  }

  TestFixture& InitAudioProcessor(const std::filesystem::path& config_name) {
    std::filesystem::path config_path = kBase + "config/";
    config_path += config_name;
    const std::string cfg = config_path.string();  // narrow on Windows

    dsp = std::make_unique<spench::SpenchAudioProcess>(cfg.c_str());
    return *this;
  }

  TestFixture& InitAudioProcessor(const std::string& yaml) {
    dsp = std::make_unique<spench::SpenchAudioProcess>(yaml);
    return *this;
  }

  TestFixture& InitAudioProcessor() {
    dsp = std::make_unique<spench::SpenchAudioProcess>();
    return *this;
  }

  TestFixture& InitAudioEchoCancelation(const std::filesystem::path& config_name) {
    std::filesystem::path config_path = kBase + "config/";
    config_path += config_name;
    const std::string cfg = config_path.string();  // narrow on Windows

    aec = std::make_unique<spench::SpenchAudioEchoCancelation>(cfg.c_str());
    return *this;
  }

  TestFixture& InitAudioEchoCancelation(const std::string& yaml) {
    aec = std::make_unique<spench::SpenchAudioEchoCancelation>(yaml);
    return *this;
  }

  TestFixture& InitAudioEchoCancelation() {
    aec = std::make_unique<spench::SpenchAudioEchoCancelation>("");
    return *this;
  }
};

#ifdef BUILD_ONNX_ENGINE
static std::vector<uint8_t> read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    throw std::runtime_error("cannot open file: " + p.string());
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
}
#endif

TEST_CASE("SUCCEED Convert pcm to float point [spench c++ api]") {
  std::vector<int16_t> pcm16 = {0, 32767, -32768, 2000, -5000};
  std::vector<float> ref_float = {0.0, 1.0, -1.0, 0.06103, -0.1526};
  std::vector<float> converted_float;

  spench::tools::convertPCM16ToFloat32(pcm16, converted_float);

  IsVectorsClose(ref_float, converted_float, 0.001);
}

TEST_CASE("SUCCEED Convert float point to pcm [spench c++ api]") {
  std::vector<float> float_point32 = {0, 1, -1, 0.06103, -0.1526};
  std::vector<int16_t> ref_pcm = {0, 32767, -32767, 2000, -5000};
  std::vector<int16_t> converted_pcm;

  spench::tools::convertFloat32ToPCM16(float_point32, converted_pcm);

  REQUIRE(ref_pcm == converted_pcm);
}

TEST_CASE("SUCCEED Create context [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")));
#endif
  spench::SpenchContext<float>* ctx = nullptr;
  ctx = test_data.denoiser->CreateContext();
  REQUIRE(ctx != nullptr);

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Retrieve model sample rate [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")));
#endif
  REQUIRE(test_data.denoiser->GetModelSampleRate() == 48000);
}

TEST_CASE("SUCCEED Retrieve model input frame size [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")));
#endif
  REQUIRE(test_data.denoiser->GetModelInputFrameSize() == (48000 * 16) / 1000);
}

TEST_CASE("SUCCEED Set and retrieve a context id [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")));
#endif

  spench::SpenchContext<float>* ctx = nullptr;
  ctx = test_data.denoiser->CreateContext();

  test_data.denoiser->SetContextId(ctx, 1000);
  REQUIRE(test_data.denoiser->GetContextId(ctx) == 1000);
  test_data.denoiser->SetContextId(ctx, 5000);
  REQUIRE(test_data.denoiser->GetContextId(ctx) == 5000);

  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Get empty statistic information with nullptr instead of context [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  auto stat = test_data.denoiser->GetStreamStat(nullptr);
  REQUIRE(stat.m_snr == 0.0);
}

TEST_CASE("SUCCEED Run inference with 100 tensors [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
                std::move(TestFixture().
                          InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                          SetWav("rec_noise_001_48k.wav"));
  #else
    auto test_data =
                std::move(TestFixture().
                          InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                          SetWav("rec_noise_001_48k.wav"));
  #endif

  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    REQUIRE(ret_code >= SpenchRetCode::SUCCESS);
    REQUIRE(ret_code <= SpenchRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

#if 0
TEST_CASE("SUCCEED Get statistic information [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
              std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_16khz.yaml")).
                        SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data =
              std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_16khz.yaml")).
                        SetWav("rec_noise_001_16k.wav"));
  #endif

  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    auto ret_code = test_data.denoiser->ProcessChunk(ctx, chunk, ret);
    test_data.denoiser->GetStreamStat(ctx);

    REQUIRE(ret_code >= SpenchRetCode::SUCCESS);
    REQUIRE(ret_code <= SpenchRetCode::CONTEXT_ACCUMULATION);
  }

  auto stat = test_data.denoiser->GetStreamStat(nullptr);
  CHECK(stat.m_snr == 0.0);

  stat = test_data.denoiser->GetStreamStat(ctx);
  REQUIRE(stat.m_snr != 0.0);

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}
#endif

TEST_CASE("SUCCEED Run inference with 100 tensors and default context arguments [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
                std::move(TestFixture().
                          InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                          SetWav("rec_noise_001_48k.wav"));
  #else
    auto test_data =
                std::move(TestFixture().
                          InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                          SetWav("rec_noise_001_48k.wav"));
  #endif

  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext(0, 0);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    REQUIRE(ret_code >= SpenchRetCode::SUCCESS);
    REQUIRE(ret_code <= SpenchRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}


TEST_CASE("SUCCEED Reload configuration from disk [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")));
#else
  auto test_data = std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")));
#endif
  REQUIRE_NOTHROW(test_data.denoiser->ReloadConfig());
}


TEST_CASE("SUCCEED Run inference with the best quality 48 kHz model and 2 contexts [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
                std::move(TestFixture().
                          InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                          SetWav("rec_noise_001_48k.wav"));
  #else
    auto test_data =
                std::move(TestFixture().
                          InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                          SetWav("rec_noise_001_48k.wav"));
  #endif

  std::vector<float> predictions1;
  std::vector<float> predictions2;
  spench::SpenchContext<float>* ctx1 = test_data.denoiser->CreateContext();
  spench::SpenchContext<float>* ctx2 = test_data.denoiser->CreateContext();
  CHECK(ctx1 != ctx2);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret1, ctx1);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret2, ctx2);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    predictions1.insert(predictions1.end(), ret1.begin(), ret1.end());
    predictions2.insert(predictions2.end(), ret2.begin(), ret2.end());
  }
  IsVectorsClose(predictions1, predictions2, 0.000001);

  test_data.denoiser->DestroyContext(&ctx1);
  CHECK(ctx1 == nullptr);
  test_data.denoiser->DestroyContext(&ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with 48 kHz model and 2 context with default parameters [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
              std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                        SetWav("rec_noise_001_48k.wav"));
  #else
    auto test_data =
              std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                        SetWav("rec_noise_001_48k.wav"));
  #endif

  std::vector<float> predictions1;
  std::vector<float> predictions2;
  spench::SpenchContext<float>* ctx1 = test_data.denoiser->CreateContext();
  spench::SpenchContext<float>* ctx2 = test_data.denoiser->CreateContext(0, 0);
  CHECK(ctx1 != ctx2);
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret1, ctx1);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret2, ctx2);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    predictions1.insert(predictions1.end(), ret1.begin(), ret1.end());
    predictions2.insert(predictions2.end(), ret2.begin(), ret2.end());
  }
  IsVectorsClose(predictions1, predictions2, 0.000001);

  test_data.denoiser->DestroyContext(&ctx1);
  CHECK(ctx1 == nullptr);
  test_data.denoiser->DestroyContext(&ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with the best quality 48 kHz model and 2 contexts concurently [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                      SetWav("rec_noise_001_48k.wav"));
  #else
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                      SetWav("rec_noise_001_48k.wav"));
  #endif

  const uint16_t kNumber = 2;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<spench::SpenchContext<float>*> contexts;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          spench::SpenchContext<float>* ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
      CHECK(ret_code >= SpenchRetCode::SUCCESS);
      predictions.insert(predictions.end(), ret.begin(), ret.end());
    }
  };

  for (uint32_t i = 0; i < kNumber; ++i) {
    spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();
    test_data.denoiser->SetContextId(ctx, i);
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
    test_data.denoiser->DestroyContext(&contexts[i]);
  }
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model and 2 contexts concurently [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("rec_noise_001_16k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                               SetWav("rec_noise_001_16k.wav"));
  #endif

  const uint16_t kNumber = 2;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  std::vector<std::vector<float>> predictions(kNumber);
  std::vector<std::thread> pool;
  std::vector<spench::SpenchContext<float>*> contexts;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  auto functor =
   [&test_data, &chunks](
          spench::SpenchContext<float>* ctx,
          std::vector<float>& predictions) ->void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
      CHECK(ret_code >= SpenchRetCode::SUCCESS);
      predictions.insert(predictions.end(), ret.begin(), ret.end());
    }
  };

  for (uint32_t i = 0; i < kNumber; ++i) {
    spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();
    test_data.denoiser->SetContextId(ctx, i);
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
    test_data.denoiser->DestroyContext(&contexts[i]);
  }
}

TEST_CASE("FAIL Run inference with invalid chunk size [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
              std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                        SetWav("rec_noise_001_48k.wav"));
  #else
    auto test_data =
              std::move(TestFixture().
                        InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                        SetWav("rec_noise_001_48k.wav"));
  #endif

  std::vector<float> predictions;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext(48000, 48000);
  const uint32_t kChunk_size = 20;
  const uint32_t kChunk_stride = 10;
  const uint32_t kChunks_to_compare = 5;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);
  std::vector<float> ret;
  SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunks[0], ret, ctx);
  CHECK(ret_code == SpenchRetCode::ERR_INVALID_FRAME_SIZE);
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the best quality model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                      SetWav("rec_noise_001_48k.wav"));
    const float k_precision_threshold = 0.015;
  #else
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                      SetWav("rec_noise_001_48k.wav"));
    const float k_precision_threshold = 0.005;
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction);

  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.denoiser->DestroyContext(&ctx);

  // erase first zero frame
  const uint32_t warmup_samples = 768;
  prediction.erase(prediction.begin(), prediction.begin() + warmup_samples);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, k_precision_threshold);
}

TEST_CASE("SUCCEED Run inference with model 48kHz on a part of .wav with double chunks [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_best_quality_tflite_48khz.yaml")).
                      SetWav("rec_noise_001_48k.wav"));
    const float k_precision_threshold = 0.015;
  #else
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_best_quality_onnx_48khz.yaml")).
                      SetWav("rec_noise_001_48k.wav"));
    const float k_precision_threshold = 0.005;
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 32;
  const uint32_t kChunk_stride = kChunk_size;
  const uint32_t kChunks_to_compare = 150;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction);

  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, k_precision_threshold);
}

TEST_CASE("SUCCEED Run inference with model 48kHz on a part of .wav with chunks of various size [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path(
                        "default_configuration_best_quality_tflite_48khz.yaml"))
                    .SetWav("rec_noise_001_48k.wav"));
  const float k_precision_threshold = 0.015;
#else
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path(
                        "default_configuration_best_quality_onnx_48khz.yaml"))
                    .SetWav("rec_noise_001_48k.wav"));
  const float k_precision_threshold = 0.005;
#endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;

  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;          // ms
  const uint32_t kChunk_stride = kChunk_size;
  const uint32_t kChunks_to_compare = 300;

  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  chunks.resize(kChunks_to_compare);

  loadWavFx("data/results/wavs/rec_denoised_best_quality_001_48k.wav", reference_prediction);

  // 20*8 + 20*4 + 10*3 + 15*2 = 160 + 80 + 30 + 30 = 300
  constexpr std::size_t kNumGroups = 65;
  const std::array<uint32_t, 65> kMergeFactors = [] {
    std::array<uint32_t, 65> a{};
    std::size_t idx = 0;

    for (uint32_t i = 0; i < 20; ++i) a[idx++] = 8;
    for (uint32_t i = 0; i < 20; ++i) a[idx++] = 4;
    for (uint32_t i = 0; i < 10; ++i) a[idx++] = 3;
    for (uint32_t i = 0; i < 15; ++i) a[idx++] = 2;

    return a;
  }();

  std::size_t chunk_pos = 0;
  for (uint32_t factor : kMergeFactors) {
    auto merged_chunk = mergeChunks(chunks, chunk_pos, factor);
    chunk_pos += factor;

    std::vector<float> ret;
    test_data.denoiser->ProcessChunk(merged_chunk, ret, ctx);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  test_data.denoiser->DestroyContext(&ctx);

  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, k_precision_threshold);
}

TEST_CASE("SUCCEED Run inference with model 48kHz on huge single chunk (4000 frames) [spench c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path(
                        "default_configuration_best_quality_tflite_48khz.yaml"))
                    .SetWav("rec_noise_001_48k.wav"));
#else
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path(
                        "default_configuration_best_quality_onnx_48khz.yaml"))
                    .SetWav("rec_noise_001_48k.wav"));
#endif

  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;                 // ms
  const uint32_t kChunk_stride = kChunk_size;
  const std::size_t kTargetChunks = 4000;

  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);

  REQUIRE_FALSE(chunks.empty());

  const std::size_t base_chunks = chunks.size();
  const std::size_t repeats = (kTargetChunks + base_chunks - 1) / base_chunks;

  const std::size_t samples_per_chunk = chunks.front().size();

  std::vector<float> huge_input;
  huge_input.reserve(kTargetChunks * samples_per_chunk);

  std::size_t filled = 0;
  for (std::size_t r = 0; r < repeats && filled < kTargetChunks; ++r) {
    for (const auto& ch : chunks) {
      huge_input.insert(huge_input.end(), ch.begin(), ch.end());
      if (++filled == kTargetChunks) break;
    }
  }

  REQUIRE(filled == kTargetChunks);
  REQUIRE(huge_input.size() == kTargetChunks * samples_per_chunk);

  std::vector<float> ret;
  REQUIRE_NOTHROW(test_data.denoiser->ProcessChunk(huge_input, ret, ctx));

  REQUIRE_FALSE(ret.empty());

  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                      SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data =
            std::move(TestFixture().
                      InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                      SetWav("rec_noise_001_24k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.01);
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("SUCCEED Run inference with encrypted fast 24 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
      std::move(TestFixture().
                InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_encrypted_24khz.yaml"), "test").
                SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data =
      std::move(TestFixture().
                InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_encrypted_24khz.yaml"), "test").
                SetWav("rec_noise_001_24k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.01);
}

TEST_CASE("SUCCEED Run inference with licenced encrypted fast 24 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data =
          std::move(TestFixture().
                    InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_lic_encrypted_24khz.yaml"),
                                "qwerty",
                                 "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I=").
                    SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data =
          std::move(TestFixture().
                    InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_lic_encrypted_24khz.yaml"),
                                 "qwerty",
                                 "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU=").
                    SetWav("rec_noise_001_24k.wav"));
  #endif

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  test_data.denoiser->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.01);
}
#else
TEST_CASE("FAIL Run with password when the framework built without encryption [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_encrypted_24khz.yaml"), "test"),
      "Due to errors a neural network handler module could not be created! ret_code: -8");
  #else
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_encrypted_24khz.yaml"), "test"),
      "Due to errors a neural network handler module could not be created! ret_code: -8");
  #endif
}

TEST_CASE("FAIL Run with password and checksum when the framework built without encryption [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_lic_encrypted_24khz.yaml"),
                "qwerty",
                "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I="),
      "Due to errors a neural network handler module could not be created! ret_code: -8");
  #else
    REQUIRE_THROWS_WITH(TestFixture().
      InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_lic_encrypted_24khz.yaml"),
                "qwerty",
                "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU="),
      "Due to errors a neural network handler module could not be created! ret_code: -8");
  #endif
}
#endif

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with the final inference chunk [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("rec_noise_001_24k.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                               SetWav("rec_noise_001_24k.wav"));
  #endif

  SpenchRetCode ret_code;
  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 10;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.denoiser->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/rec_denoised_fast_001_24k.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  reference_prediction.resize(prediction.size());
  IsVectorsClose(reference_prediction, prediction, 0.0001);
  std::vector<float> ret;
  ret_code = test_data.denoiser->GetFinalChunk(ret, ctx);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  test_data.denoiser->DestroyContext(&ctx);
}

#ifndef BUILD_WITHOUT_SOXR
TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with input resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
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
  spench::SpenchContext<float>* ctx =
                          test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_24_enhanced.wav", reference);
  chunks.resize(kChunks_to_compare);
  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  std::vector<float> ret;
  ret_code = test_data.denoiser->GetFinalChunk(ret, ctx);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);
  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.03);
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with output resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
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
  spench::SpenchContext<float>* ctx =
                          test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);
  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);

  std::vector<float> ret;
  ret_code = test_data.denoiser->GetFinalChunk(ret, ctx);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.03);
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with input and output resampling 48 kHz [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
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
  spench::SpenchContext<float>* ctx =
                          test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  std::vector<float> ret;
  ret_code = test_data.denoiser->GetFinalChunk(ret, ctx);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.03);
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the fast 24 kHz model with yaml string config [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    static constexpr char yaml[] = R"(
model:
  model_path: data/models/fast_model_24khz.tflite)";
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::string(yaml)).
                               SetWav("test1_48_noised.wav"));
  #else
    static constexpr char yaml[] = R"(
model:
  model_path: data/models/fast_model_24khz.onnx)";
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::string(yaml)).
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
  spench::SpenchContext<float>* ctx =
                          test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  std::vector<float> ret;
  ret_code = test_data.denoiser->GetFinalChunk(ret, ctx);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.03);
  test_data.denoiser->DestroyContext(&ctx);
}

#ifdef BUILD_ONNX_ENGINE
TEST_CASE("SUCCEED Run inference with the packed fast 24 kHz denoiser model [spench c++ api]") {
  auto bundle = std::make_shared<spench::PackedModelReader>(read_file("data/models/packed_fast_model.spench"));
  auto test_data = std::move(TestFixture().
                              InitDenoiser(bundle).
                              SetWav("test1_48_noised.wav"));

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
  spench::SpenchContext<float>* ctx =
                          test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                              kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  loadWavFx("data/results/wavs/test1_48_enhanced_upsampled.wav", reference);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    if (ret_code >= SpenchRetCode::SUCCESS) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  std::vector<float> ret;
  ret_code = test_data.denoiser->GetFinalChunk(ret, ctx);

  CHECK(ret_code == SpenchRetCode::SUCCESS);
  REQUIRE(std::vector<float>(ret.size(), 0.0) != ret);

  prediction.insert(prediction.end(), ret.begin(), ret.end());

  reference.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));
  prediction.resize(std::min(kTotal_size, static_cast<uint32_t>(prediction.size())));

  IsVectorsClose(reference, prediction, 0.03);
  test_data.denoiser->DestroyContext(&ctx);
}
#endif

TEST_CASE("SUCCEED Run inference with the dereverb 48 kHz model with resampling from 44.1 kHz [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    // auto test_data =
    //.     std::move(TestFixture().
    //           InitDereverberator(std::filesystem::path("default_configuration_dereverberation_tflite_48khz.yaml")).
    //           SetWav("reverb_with_noise_48khz.wav"));
  #else
  auto test_data =
            std::move(TestFixture().
                 InitDereverberator(std::filesystem::path("default_configuration_dereverberation_onnx_48khz.yaml")).
                 SetWav("reverb_with_noise_44.1khz.wav"));

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  SpenchRetCode ret_code;

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_dst_sample_rate = 44100;
  const uint32_t kAudio_src_sample_rate = 44100;

  spench::SpenchContext<float>* ctx = test_data.dereverb->CreateContext(kAudio_src_sample_rate,
                                                                          kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);

  loadWavFx("data/results/wavs/dereverb_with_noise_44.1khz.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;

    ret_code = test_data.dereverb->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  test_data.dereverb->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  // ToDo (artsokol) make a threshold smaller for new dereverb model
  IsVectorsClose(reference_prediction, prediction, 0.4);
  #endif
}

TEST_CASE("SUCCEED Run inference with the dereverb 48 kHz model with yaml string config [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE

  #else
    static constexpr char yaml[] = R"(
model:
  model_path: data/models/dereverb_model_48khz.onnx)";
  auto test_data = std::move(TestFixture().
                               InitDereverberator(std::string(yaml)).
                               SetWav("reverb_with_noise_44.1khz.wav"));

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  SpenchRetCode ret_code;

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_dst_sample_rate = 44100;
  const uint32_t kAudio_src_sample_rate = 44100;

  spench::SpenchContext<float>* ctx = test_data.dereverb->CreateContext(kAudio_src_sample_rate,
                                                                          kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);

  loadWavFx("data/results/wavs/dereverb_with_noise_44.1khz.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;

    ret_code = test_data.dereverb->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  test_data.dereverb->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  // ToDo (artsokol) make a threshold smaller for new dereverb model
  IsVectorsClose(reference_prediction, prediction, 0.4);
  #endif
}

#ifdef BUILD_ONNX_ENGINE
TEST_CASE("SUCCEED Run inference with packed 48 kHz dereverb model [spench c++ api]") {
  auto bundle = std::make_shared<spench::PackedModelReader>(read_file("data/models/packed_dereverb_model.spench"));
  auto test_data = std::move(TestFixture().
                  InitDereverberator(bundle).
                  SetWav("reverb_with_noise_44.1khz.wav"));

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  SpenchRetCode ret_code;

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 100;
  const uint32_t kAudio_dst_sample_rate = 44100;
  const uint32_t kAudio_src_sample_rate = 44100;

  spench::SpenchContext<float>* ctx = test_data.dereverb->CreateContext(kAudio_src_sample_rate,
                                                                          kAudio_dst_sample_rate);

  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);

  loadWavFx("data/results/wavs/dereverb_with_noise_44.1khz.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;

    ret_code = test_data.dereverb->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  test_data.dereverb->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());
  // ToDo (artsokol) make a threshold smaller for new dereverb model
  IsVectorsClose(reference_prediction, prediction, 0.4);
}
#endif

#else
TEST_CASE("FAIL Run inference with the fast 24 kHz model with input resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                               SetWav("test1_48_noised.wav"));
  #endif

  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;

  REQUIRE_THROWS_WITH(test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
}

TEST_CASE("FAIL Run inference with the fast 24 kHz model with output resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("test1_24_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                               SetWav("test1_24_noised.wav"));
  #endif

  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;

  REQUIRE_THROWS_WITH(test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
}

TEST_CASE("FAIL Run inference with the fast 24 kHz model with input and output resampling 48 kHz [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_tflite_24khz.yaml")).
                               SetWav("test1_48_noised.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDenoiser(std::filesystem::path("default_configuration_fast_onnx_24khz.yaml")).
                               SetWav("reverb_with_noise_44.1khz.wav"));
  #endif

  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;

  REQUIRE_THROWS_WITH(test_data.denoiser->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
}

TEST_CASE("FAIL Run inference with dereverb model 48kHz with output resampling [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    // auto test_data = std::move(TestFixture().
    //                            InitDereverberator("default_configuration_dereverberation_tflite_48khz.yaml").
    //                            SetWav("reverb_with_noise_48khz.wav"));
  #else
    auto test_data = std::move(TestFixture().
                               InitDereverberator("default_configuration_dereverberation_onnx_48khz.yaml").
                               SetWav("test1_24_noised.wav"));

  const uint32_t kAudio_src_sample_rate = 44100;
  const uint32_t kAudio_dst_sample_rate = 48000;

  REQUIRE_THROWS_WITH(test_data.dereverb->CreateContext(kAudio_src_sample_rate,
                                                          kAudio_dst_sample_rate),
                      "Due to errors a context could not be created!");
  #endif
}
#endif

TEST_CASE("SUCCEED Run AudioProcessing with RC default parameters [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);
  REQUIRE(greater_than_0_9.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.9
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);
}

TEST_CASE("SUCCEED Run AudioProcessing LC with parameters passed via constructor [spench c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor());

  constexpr uint32_t fs = 16000;
  constexpr uint32_t chunk_ms = 32;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (12 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;
  const     int32_t bypass_time = 3;
  const     int32_t target_db = -23;
  const     int32_t max_gain_db = 40;

  test_data.dsp->AddLC(fs, target_db, bypass_time, max_gain_db);

  const auto tt = dsplib::arange(num_samples) / fs;
  const auto in = 0.5 * sin(2 * dsplib::pi * 440.0 * tt);

  auto rms_before = dsplib::mag2db(dsplib::rms(in));

  // process frame by frame
  dsplib::arr_real out;

  for (uint32_t i = 0; i < num_frames; ++i) {
    const uint32_t t1 = i * frame_len;
    const uint32_t t2 = t1 + frame_len;
    const auto x = *in.slice(t1, t2);
    std::vector<float> chunk = x.to_vec();
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    out |= dsplib::arr_real(ret);
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
}

TEST_CASE("SUCCEED LC with VAD prevents clipping on loud input [spench c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor());

  constexpr uint32_t fs = 16000;
  constexpr uint32_t chunk_ms = 32;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (6 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;

  const int32_t bypass_time = 0;
  const int32_t target_db = -15;
  const int32_t max_gain_db = 30;
  const bool use_vad = true;

  test_data.dsp->AddLC(fs, target_db, bypass_time, max_gain_db, use_vad);

  // Very loud input intended to trigger limiter (if enabled in this high-level path).
  const auto in = tone(fs, num_samples, 220.0f, 2.0f);

  dsplib::arr_real out;
  for (uint32_t i = 0; i < num_frames; ++i) {
    const uint32_t t1 = i * frame_len;
    const uint32_t t2 = t1 + frame_len;
    const auto x = *in.slice(t1, t2);
    std::vector<float> chunk = x.to_vec();
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    out |= dsplib::arr_real(ret);
  }

  // If limiter is enabled internally, peak should be bounded near -1 dBFS.
  const float kLimiterMag = dsplib::db2mag(-1.0f);
  REQUIRE(maxAbs(out) <= kLimiterMag + 5e-3f);
}

TEST_CASE("SUCCEED Run AudioProcessing with AGC default parameters [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddAGC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
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

TEST_CASE("SUCCEED Run AudioProcessing with cahnged threshold for NG [spench c++ api]") {
  const char change[] = R"(
sound_processing:
  noise_gate_ctrl:
    threshold: -15.0
)";
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor(std::string(change)).
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddNG();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() > orig_equal_0);
}

TEST_CASE("SUCCEED Run AudioProcessing with DRC default parameters [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddDRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
    std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with changed AGC parameters [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddAGC(48000, 0.9, 20.0);
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
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
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddRC(48000, -4);
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_8;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
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

TEST_CASE("SUCCEED Run AudioProcessing with changed NG parameters [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));
  test_data.dsp->AddNG(48000, -40, 0.05, 0.02, 0.05);
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  std::vector<float> equal_0;
  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() == orig_equal_0);
}

TEST_CASE("SUCCEED Run AudioProcessing with changed DRC parameters [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor().
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddDRC(48000, 0, 5, 0.0, 0.01, 0.2);
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() != 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading RC parameters from config [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor(
                                std::filesystem::path("default_configuration_fast_tflite_rc_enabled_24khz.yaml")).
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);
  REQUIRE(greater_than_0_9.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.9
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading AGC parameters from config [spench c++ api]") {
  auto test_data =
      std::move(TestFixture().
                InitAudioProcessor(std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml")).
                SetWav("test1_48_noised.wav"));

  test_data.dsp->AddAGC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
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

TEST_CASE("SUCCEED Run AudioProcessing with reading NG parameters from config [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                             InitAudioProcessor(
                                std::filesystem::path("default_configuration_fast_tflite_rc_enabled_24khz.yaml")).
                             SetWav("test1_48_noised.wav"));

  test_data.dsp->AddNG();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() > orig_equal_0);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading DRC parameters from config [spench c++ api]") {
  auto test_data =
      std::move(TestFixture().
                InitAudioProcessor(std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml")).
                SetWav("test1_48_noised.wav"));

  test_data.dsp->AddDRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * kChunk_size;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(),
                               test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

// -----------------------------------------------------------------------------
// POSITIVE — YAML string accepted, chain can be built and used
// -----------------------------------------------------------------------------
TEST_CASE("SUCCESS AudioProcessing accepts YAML string and can process a small frame [spench c++ api]") {
  const std::string yaml = spench::DefaultAudioProcessingConfigYAML;

  // Construct from YAML string
  auto fx = std::move(TestFixture().InitAudioProcessor(yaml));

  // Build a small chain reading params from YAML
  REQUIRE_NOTHROW(fx.dsp->AddRC());
  REQUIRE_NOTHROW(fx.dsp->AddAGC());
  REQUIRE_NOTHROW(fx.dsp->AddNG());
  REQUIRE_NOTHROW(fx.dsp->AddDRC());
  REQUIRE_NOTHROW(fx.dsp->AddLim());
  REQUIRE_NOTHROW(fx.dsp->AddLC());

  // One 16-ms frame @ 48 kHz
  const uint32_t fs = 48000;
  const uint32_t frame_len = fs * 16 / 1000;
  std::vector<float> in(frame_len, 0.0f);
  // add a tiny tone so it’s not all zeros
  for (size_t i = 0; i < in.size(); ++i) {
    in[i] = 0.1f * std::sin(2 * M_PI * 440.0 * static_cast<double>(i) / static_cast<double>(fs));
  }

  std::vector<float> out;
  REQUIRE_NOTHROW(fx.dsp->ProcessChunk(in, out));
  REQUIRE(out.size() == in.size());
}

// -----------------------------------------------------------------------------
// NEGATIVE — structure/type validation (YAML string)
// -----------------------------------------------------------------------------
TEST_CASE("FAIL AudioProcessing YAML string non-scalar where scalar required throws [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  range_ctrl:
    sample_rate: [48000]        # must be scalar
    target_level_db: -3.0
    max_gain_db: 10
    attack_time: 0.0
    release_time: 300
    hold_time: 5
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

// -----------------------------------------------------------------------------
// NEGATIVE — numeric/range validation (YAML string)
// -----------------------------------------------------------------------------
TEST_CASE("FAIL AudioProcessing YAML string: range_ctrl.sample_rate too large [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  range_ctrl:
    sample_rate: 200000
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: auto_gain_ctrl.target_level <= 0 [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  auto_gain_ctrl:
    target_level: -0.02
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: noise_gate_ctrl.attack_time <= 0 [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  noise_gate_ctrl:
    sample_rate: 48000
    attack_time: -0.01
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: drc.release_time <= 0 [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  dynamic_range_compressor_ctrl:
    sample_rate: 48000
    threshold: -15.0
    ratio: 5
    knee_width: 0.0
    attack_time: 0.01
    release_time: -0.01
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: loudness_ctrl.bypass_time <= 0 [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  loudness_ctrl:
    bypass_time: -0.01
    max_gain_db: 40.0
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: limiter_ctrl.threshold non-numeric [spench c++ api]") {
  const std::string bad = R"(
sound_processing:
  limiter_ctrl:
    sample_rate: 48000
    threshold: abc
)";
  REQUIRE_THROWS(TestFixture().InitAudioProcessor(bad));
}


TEST_CASE("SUCCEED Run inference with the dereverb 48 kHz model on a part of .wav file [spench c++ api]") {
  #ifdef BUILD_TFLITE_ENGINE
    // auto test_data =
    //       std::move(TestFixture().
    //            InitDereverberator(std::filesystem::path("default_configuration_dereverberation_tflite_48khz.yaml")).
    //            SetWav("reverb_with_noise_48khz.wav"));
  #else
    auto test_data =
            std::move(TestFixture().
                InitDereverberator(std::filesystem::path("default_configuration_dereverberation_onnx_48khz.yaml")).
                SetWav("reverb_with_noise_48khz.wav"));

  std::vector<float> prediction;
  std::vector<float> reference_prediction;
  spench::SpenchContext<float>* ctx = test_data.dereverb->CreateContext();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kChunks_to_compare = 300;
  const uint32_t kAudio_dst_sample_rate = 48000;

  auto chunks = chunkWav<float>(test_data.wav_data,
                                test_data.dereverb->GetModelSampleRate(),
                                kChunk_size, kChunk_stride);

  loadWavFx("data/results/wavs/dereverb_with_noise_48khz.wav", reference_prediction);
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    SpenchRetCode ret_code = test_data.dereverb->ProcessChunk(chunk, ret, ctx);
    CHECK(ret_code >= SpenchRetCode::SUCCESS);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  // remove context zeroes as reference does not include that
  prediction.erase(prediction.begin(),
                   prediction.begin() + kAudio_dst_sample_rate * kChunk_size / 1000);
  //
  test_data.dereverb->DestroyContext(&ctx);
  reference_prediction.resize(prediction.size());

  IsVectorsClose(reference_prediction, prediction, 0.3);
  #endif
}

TEST_CASE("SUCCEED Run AudioEchoCancelation with simulated audio [spench c++ api]") {
  auto test_data =
          std::move(TestFixture().
                InitAudioEchoCancelation(
                  std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml")));
  constexpr int kAudio_src_sample_rate = 48000;
  constexpr int kChunk_size_ms = 16;
  constexpr int frame_len = kAudio_src_sample_rate * kChunk_size_ms / 1000;
  constexpr int num_frames = (10 * kAudio_src_sample_rate) / frame_len;
  constexpr int num_samples = num_frames * frame_len;
  constexpr int filter_len = 4096;

  // loopback - noise, speech - sinus
  auto rir_flt = dsplib::FirFilterR(DumbRIR(filter_len * 0.7));
  auto tt = dsplib::arange(num_samples) / kAudio_src_sample_rate;
  auto sig = 0.1 * sin(2 * dsplib::pi * 440.0 * tt);
  auto ref = 0.5 * dsplib::rand({-1, 1}, num_samples);
  auto noise = rir_flt(ref);
  auto mic = 0.1 * (sig + noise);

  // add noise (nonlinear effects)
  mic += 1e-5 * dsplib::randn(num_samples);
  ref += 1e-5 * dsplib::randn(num_samples);

  const auto snr_before = dsplib::mag2db(rms(sig) / rms(noise));

  // process frame by frame
  dsplib::arr_real out;
  for (int i = 0; i < num_frames; ++i) {
    const int t1 = i * frame_len;
    const int t2 = t1 + frame_len;
    const auto d = *mic.slice(t1, t2);
    const auto x = *ref.slice(t1, t2);
    std::vector<float> y;

    test_data.aec->ProcessChunk(d.to_vec(), x.to_vec(), y);
    out |= dsplib::arr_real(y);
  }

  // skip first second
  out = out.slice(kAudio_src_sample_rate, num_samples);
  auto spec = dsplib::welch(out, 1024).pxx;
  const auto snr_after = dsplib::sinad(spec, dsplib::SinadType::Psd);

  REQUIRE(snr_after - snr_before >= 20);
}

TEST_CASE("SUCCEED Run AudioEchoCancelation with empty config with simulated audio [spench c++ api]") {
    const std::string yaml = R"(
echo_cancellation:
  sample_rate: 48000
)";

  auto test_data = std::move(TestFixture().InitAudioEchoCancelation(yaml));

  constexpr int kAudio_src_sample_rate = 48000;
  constexpr int kChunk_size_ms = 16;
  constexpr int frame_len = kAudio_src_sample_rate * kChunk_size_ms / 1000;
  constexpr int num_frames = (10 * kAudio_src_sample_rate) / frame_len;
  constexpr int num_samples = num_frames * frame_len;
  constexpr int filter_len = 4096;

  // loopback - noise, speech - sinus
  auto rir_flt = dsplib::FirFilterR(DumbRIR(filter_len * 0.7));
  auto tt = dsplib::arange(num_samples) / kAudio_src_sample_rate;
  auto sig = 0.1 * sin(2 * dsplib::pi * 440.0 * tt);
  auto ref = 0.5 * dsplib::rand({-1, 1}, num_samples);
  auto noise = rir_flt(ref);
  auto mic = 0.1 * (sig + noise);

  // add noise (nonlinear effects)
  mic += 1e-5 * dsplib::randn(num_samples);
  ref += 1e-5 * dsplib::randn(num_samples);

  const auto snr_before = dsplib::mag2db(rms(sig) / rms(noise));

  // process frame by frame
  dsplib::arr_real out;
  for (int i = 0; i < num_frames; ++i) {
    const int t1 = i * frame_len;
    const int t2 = t1 + frame_len;
    const auto d = *mic.slice(t1, t2);
    const auto x = *ref.slice(t1, t2);
    std::vector<float> y;

    test_data.aec->ProcessChunk(d.to_vec(), x.to_vec(), y);
    out |= dsplib::arr_real(y);
  }

  // skip first second
  out = out.slice(kAudio_src_sample_rate, num_samples);
  auto spec = dsplib::welch(out, 1024).pxx;
  const auto snr_after = dsplib::sinad(spec, dsplib::SinadType::Psd);

  REQUIRE(snr_after - snr_before >= 20);
}

TEST_CASE("SUCCEED Run AudioEchoCancelation passing yaml string to constructor [spench c++ api]") {
  auto test_data =
          std::move(TestFixture().
                InitAudioEchoCancelation(
                    std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml")));
  constexpr int kAudio_src_sample_rate = 48000;
  constexpr int kChunk_size_ms = 16;
  constexpr int frame_len = kAudio_src_sample_rate * kChunk_size_ms / 1000;
  constexpr int num_frames = (10 * kAudio_src_sample_rate) / frame_len;
  constexpr int num_samples = num_frames * frame_len;
  constexpr int filter_len = 4096;

  // loopback - noise, speech - sinus
  auto rir_flt = dsplib::FirFilterR(DumbRIR(filter_len * 0.7));
  auto tt = dsplib::arange(num_samples) / kAudio_src_sample_rate;
  auto sig = 0.1 * sin(2 * dsplib::pi * 440.0 * tt);
  auto ref = 0.5 * dsplib::rand({-1, 1}, num_samples);
  auto noise = rir_flt(ref);
  auto mic = 0.1 * (sig + noise);

  // add noise (nonlinear effects)
  mic += 1e-5 * dsplib::randn(num_samples);
  ref += 1e-5 * dsplib::randn(num_samples);

  const auto snr_before = dsplib::mag2db(rms(sig) / rms(noise));

  // process frame by frame
  dsplib::arr_real out;
  for (int i = 0; i < num_frames; ++i) {
    const int t1 = i * frame_len;
    const int t2 = t1 + frame_len;
    const auto d = *mic.slice(t1, t2);
    const auto x = *ref.slice(t1, t2);
    std::vector<float> y;

    test_data.aec->ProcessChunk(d.to_vec(), x.to_vec(), y);
    out |= dsplib::arr_real(y);
  }

  // skip first second
  out = out.slice(kAudio_src_sample_rate, num_samples);
  auto spec = dsplib::welch(out, 1024).pxx;
  const auto snr_after = dsplib::sinad(spec, dsplib::SinadType::Psd);

  REQUIRE(snr_after - snr_before >= 20);
}

TEST_CASE("SUCCEED Run AudioEchoCancelation successfully with real 16kHz .wav files [spench c++ api]") {
  auto test_data = std::move(TestFixture().
                    InitAudioEchoCancelation(
                              std::filesystem::path("default_configuration_fast_tflite_dsp_enabled_24khz.yaml")).
                    SetWav("test4_16_echo_mic.wav").
                    SetLbk("test4_16_echo_lbk.wav"));

  constexpr int kAudio_src_sample_rate = 16000;
  constexpr int kChunk_size_ms = 16;
  const uint32_t kChunks_to_compare = 500;
  std::vector<float> prediction;

  auto chunks_mic = chunkWav<float>(test_data.wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size_ms, kChunk_size_ms);

  auto chunks_lbk = chunkWav<float>(test_data.wav_lbk_data,
                                kAudio_src_sample_rate,
                                kChunk_size_ms, kChunk_size_ms);

  chunks_mic.resize(kChunks_to_compare);
  chunks_lbk.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks_mic.size(); ++i) {
    auto& chunk_mic = chunks_mic[i];
    auto& chunk_lbk = chunks_lbk[i];
    std::vector<float> ret;

    test_data.aec->ProcessChunk(chunk_mic, chunk_lbk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
}
