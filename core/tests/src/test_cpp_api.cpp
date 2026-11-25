#include <giga-edge-enhance.h>

#include <algorithm>
#include <catch.hpp>
#include <cmath>
#include <filesystem>
#include <future>  // NOLINT
#include <memory>

#include "src/default_configurations.h"
#include "src/task.h"
#include "tests/src/utils.h"

struct TestFixture {
  std::vector<float> wav_data;
  std::unique_ptr<giga_edge::enhance::Denoiser> denoiser;
  std::unique_ptr<giga_edge::enhance::AudioProcessor> dsp;

  const std::string data_root = "data/";

  TestFixture() : denoiser(nullptr), dsp(nullptr) {}
  ~TestFixture() {
    denoiser.reset();
    dsp.reset();
  }
  TestFixture& operator=(const TestFixture&) = delete;
  TestFixture(const TestFixture&) = delete;
  TestFixture(TestFixture&& orig)
      : wav_data(std::move(orig.wav_data)), denoiser(std::move(orig.denoiser)), dsp(std::move(orig.dsp)) {
    orig.wav_data.clear();
    orig.denoiser = nullptr;
    orig.dsp = nullptr;
  }
  TestFixture& SetWav(const std::string& wav_path) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_data, false);
    return *this;
  }
  TestFixture& InitDenoiser(const std::filesystem::path& config_name) {
    std::filesystem::path config_path = data_root + "config/";
    config_path += config_name;
    const std::string cfg = config_path.string();  // narrow on Windows
    denoiser = std::make_unique<giga_edge::enhance::Denoiser>(cfg.c_str());
    return *this;
  }

  TestFixture& InitDenoiser(const std::string& yaml) {
    denoiser = std::make_unique<giga_edge::enhance::Denoiser>(yaml);
    return *this;
  }

  TestFixture& InitAudioProcessor(const std::filesystem::path& config_name) {
    std::filesystem::path config_path = data_root + "config/";
    config_path += config_name;
    const std::string cfg = config_path.string();  // narrow on Windows

    dsp = std::make_unique<giga_edge::enhance::AudioProcessor>(cfg.c_str());
    return *this;
  }

  TestFixture& InitAudioProcessor(const std::string& yaml) {
    dsp = std::make_unique<giga_edge::enhance::AudioProcessor>(yaml);
    return *this;
  }

  TestFixture& InitAudioProcessor() {
    dsp = std::make_unique<giga_edge::enhance::AudioProcessor>();
    return *this;
  }
};

TEST_CASE("SUCCEED Convert pcm to float point [gigaedge-enhance c++ api]") {
  std::vector<int16_t> pcm16 = {0, 32767, -32768, 2000, -5000};
  std::vector<float> ref_float = {0.0, 1.0, -1.0, 0.06103, -0.1526};
  std::vector<float> converted_float;

  giga_edge::enhance::tools::convert_pcm16_to_float32(pcm16, converted_float);

  IsVectorsClose(ref_float, converted_float, 0.001);
}

TEST_CASE("SUCCEED Convert float point to pcm [gigaedge-enhance c++ api]") {
  std::vector<float> float_point32 = {0, 1, -1, 0.06103, -0.1526};
  std::vector<int16_t> ref_pcm = {0, 32767, -32767, 2000, -5000};
  std::vector<int16_t> converted_pcm;

  giga_edge::enhance::tools::convert_float32_to_pcm16(float_point32, converted_pcm);

  REQUIRE(ref_pcm == converted_pcm);
}

TEST_CASE("SUCCEED Create context [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml")));
#else
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml")));
#endif
  giga_edge::enhance::Context<float>* ctx = nullptr;
  ctx = test_data.denoiser->CreateContext();
  REQUIRE(ctx != nullptr);

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Retrieve model sample rate [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml")));
#else
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml")));
#endif
  REQUIRE(test_data.denoiser->GetModelSampleRate() == 48000);
}

TEST_CASE("SUCCEED Retrieve model input frame size [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml")));
#else
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml")));
#endif
  REQUIRE(test_data.denoiser->GetModelInputFrameSize() == (48000 * 16) / 1000);
}

TEST_CASE("SUCCEED Set and retrieve a context id [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml")));
#else
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml")));
#endif

  giga_edge::enhance::Context<float>* ctx = nullptr;
  ctx = test_data.denoiser->CreateContext();

  test_data.denoiser->SetContextId(ctx, 1000);
  REQUIRE(test_data.denoiser->GetContextId(ctx) == 1000);
  test_data.denoiser->SetContextId(ctx, 5000);
  REQUIRE(test_data.denoiser->GetContextId(ctx) == 5000);

  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with 100 tensors [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext();
  const uint32_t chunks_to_compare = 100;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(ret_code <= GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Run hybrid inference with 100 tensors [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_hybrid_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_hybrid_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext();
  const uint32_t chunks_to_compare = 100;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(ret_code <= GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Get statistic information [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext();
  const uint32_t chunks_to_compare = 400;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  REQUIRE_FALSE(chunks.empty());

  for (uint32_t i = 0; i < chunks_to_compare; ++i) {
    const auto& chunk = chunks[i % chunks.size()];
    std::vector<float> ret;
    auto ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    test_data.denoiser->GetStreamStat(ctx);

    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(ret_code <= GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION);
  }

  auto stat = test_data.denoiser->GetStreamStat(nullptr);
  CHECK(stat.snr == 0.0);

  stat = test_data.denoiser->GetStreamStat(ctx);
  REQUIRE(std::isfinite(stat.snr));
  REQUIRE(stat.snr != 0.0);

  auto* fresh_context = test_data.denoiser->CreateContext();
  for (uint32_t i = 0; i < 10; ++i) {
    std::vector<float> output;
    test_data.denoiser->ProcessChunk(chunks[i % chunks.size()], output, fresh_context);
  }
  CHECK(test_data.denoiser->GetStreamStat(fresh_context).snr == 0.0f);
  test_data.denoiser->DestroyContext(&fresh_context);


  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}

TEST_CASE("SUCCEED Run inference with 100 tensors and default context arguments [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext(0, 0);
  const uint32_t chunks_to_compare = 100;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  for (auto& chunk : chunks) {
    std::vector<float> ret;
    REQUIRE(chunk.size() == test_data.denoiser->GetModelInputFrameSize());
    auto ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(ret_code <= GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION);
  }

  test_data.denoiser->DestroyContext(&ctx);
  REQUIRE(ctx == nullptr);
}


TEST_CASE("SUCCEED Reload configuration from disk [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml")));
#else
  auto test_data =
      std::move(TestFixture().InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml")));
#endif
  REQUIRE_NOTHROW(test_data.denoiser->ReloadConfig());
}


TEST_CASE("SUCCEED Run inference with the best quality 48 kHz model and 2 contexts [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  std::vector<float> predictions1;
  std::vector<float> predictions2;
  giga_edge::enhance::Context<float>* ctx1 = test_data.denoiser->CreateContext();
  giga_edge::enhance::Context<float>* ctx2 = test_data.denoiser->CreateContext();
  CHECK(ctx1 != ctx2);
  const uint32_t chunks_to_compare = 100;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret1, ctx1);
    CHECK(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret2, ctx2);
    CHECK(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    predictions1.insert(predictions1.end(), ret1.begin(), ret1.end());
    predictions2.insert(predictions2.end(), ret2.begin(), ret2.end());
  }
  IsVectorsClose(predictions1, predictions2, 0.000001);

  test_data.denoiser->DestroyContext(&ctx1);
  CHECK(ctx1 == nullptr);
  test_data.denoiser->DestroyContext(&ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE("SUCCEED Run inference with 48 kHz model and 2 context with default parameters [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  std::vector<float> predictions1;
  std::vector<float> predictions2;
  giga_edge::enhance::Context<float>* ctx1 = test_data.denoiser->CreateContext();
  giga_edge::enhance::Context<float>* ctx2 = test_data.denoiser->CreateContext(0, 0);
  CHECK(ctx1 != ctx2);
  const uint32_t chunks_to_compare = 100;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);
  for (auto& chunk : chunks) {
    std::vector<float> ret1;
    std::vector<float> ret2;
    GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret1, ctx1);
    CHECK(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    ret_code = test_data.denoiser->ProcessChunk(chunk, ret2, ctx2);
    CHECK(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    predictions1.insert(predictions1.end(), ret1.begin(), ret1.end());
    predictions2.insert(predictions2.end(), ret2.begin(), ret2.end());
  }
  IsVectorsClose(predictions1, predictions2, 0.000001);

  test_data.denoiser->DestroyContext(&ctx1);
  CHECK(ctx1 == nullptr);
  test_data.denoiser->DestroyContext(&ctx2);
  CHECK(ctx2 == nullptr);
}

TEST_CASE(
    "SUCCEED Run inference with the best quality 48 kHz model and 2 contexts concurently [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  const uint16_t context_count = 2;
  const uint32_t chunks_to_compare = 100;
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  std::vector<std::vector<float>> predictions(context_count);
  std::vector<std::thread> pool;
  std::vector<giga_edge::enhance::Context<float>*> contexts;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);

  auto functor = [&test_data, &chunks](giga_edge::enhance::Context<float>* ctx,
                                       std::vector<float>& predictions) -> void {
    for (auto& chunk : chunks) {
      std::vector<float> ret;
      GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, ret, ctx);
      CHECK(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
      predictions.insert(predictions.end(), ret.begin(), ret.end());
    }
  };

  for (uint32_t i = 0; i < context_count; ++i) {
    giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext();
    test_data.denoiser->SetContextId(ctx, i);
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
    test_data.denoiser->DestroyContext(&contexts[i]);
  }
}

TEST_CASE("FAIL Run inference with invalid chunk size [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  std::vector<float> predictions;
  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext(48000, 48000);
  const uint32_t chunk_size = 20;
  const uint32_t chunk_stride = 10;
  const uint32_t chunks_to_compare = 5;
  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);
  chunks.resize(chunks_to_compare);
  std::vector<float> ret;
  GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(chunks[0], ret, ctx);
  CHECK(ret_code == GigaEdgeEnhanceRetCode::ERR_INVALID_FRAME_SIZE);
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with model 48kHz on huge single chunk (4000 frames) [gigaedge-enhance c++ api]") {
#ifdef BUILD_TFLITE_ENGINE
  auto test_data =
      std::move(TestFixture()
                    .InitDenoiser(std::filesystem::path("default_configuration_discr_web_tflite_48khz.yaml"))
                    .SetWav("test1_48_noised.wav"));
#else
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
#endif

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext();

  const uint32_t chunk_size = 16;  // ms
  const uint32_t chunk_stride = chunk_size;
  const std::size_t target_chunks = 4000;

  auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), chunk_size, chunk_stride);

  REQUIRE_FALSE(chunks.empty());

  const std::size_t base_chunks = chunks.size();
  const std::size_t repeats = (target_chunks + base_chunks - 1) / base_chunks;

  const std::size_t samples_per_chunk = chunks.front().size();

  std::vector<float> huge_input;
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

  std::vector<float> ret;
  REQUIRE_NOTHROW(test_data.denoiser->ProcessChunk(huge_input, ret, ctx));

  REQUIRE_FALSE(ret.empty());

  test_data.denoiser->DestroyContext(&ctx);
}

#ifdef BUILD_ONNX_ENGINE
TEST_CASE("ONNX dynamic buffers shrink after a larger chunk [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture()
                                 .InitDenoiser(std::filesystem::path("default_configuration_discr_web_onnx_48khz.yaml"))
                                 .SetWav("test1_48_noised.wav"));
  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext();
  const auto chunks = ChunkWav<float>(test_data.wav_data, test_data.denoiser->GetModelSampleRate(), 16, 16);
  REQUIRE(chunks.size() >= 7);

  for (const size_t frame_count : {size_t{1}, size_t{4}, size_t{2}}) {
    std::vector<float> input;
    for (size_t i = 0; i < frame_count; ++i) {
      input.insert(input.end(), chunks[i].begin(), chunks[i].end());
    }

    std::vector<float> output;
    const GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(input, output, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(output.size() == input.size());
  }

  test_data.denoiser->DestroyContext(&ctx);
}
#endif


#ifdef BUILD_ONNX_ENGINE
namespace {

constexpr char hq_8khz_yaml[] = R"(
model:
  model_path: data/models/fast_quant_model_8khz.onnx
  model_inputs: ["noisy_mag_in", "prev_fb_h_in", "prev_fb_c_in", "prev_fb_conv1_buf_in", "prev_fb_conv3_buf_in"]
  model_outputs: ["cur_fb_h_out", "cur_fb_c_out", "cur_fb_conv1_buf_out", "cur_fb_conv3_buf_out", "predicted_mask_out"]

feature_params:
  sampling_freq: 8000

inference:
  gpu: false
  engine_num_threads: 1
  model_type: denoiser_discr_hq_model)";

void require_finite(const std::vector<float>& samples) {
  for (float sample : samples) {
    REQUIRE(std::isfinite(sample));
  }
}

}  // namespace

TEST_CASE("SUCCEED Run inference with the 8 kHz HQ model [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitDenoiser(std::string(hq_8khz_yaml)).SetWav("test1_24_noised.wav"));
  REQUIRE(test_data.denoiser->GetModelSampleRate() == 8000);

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext(8000, 8000);
  auto chunks = ChunkWav<float>(test_data.wav_data, 8000, 16, 16);
  REQUIRE(chunks.size() >= 20);
  chunks.resize(20);

  std::vector<float> prediction;
  for (const auto& chunk : chunks) {
    std::vector<float> output;
    const GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, output, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(output.size() == chunk.size());
    prediction.insert(prediction.end(), output.begin(), output.end());
  }
  require_finite(prediction);
  REQUIRE(std::find_if(prediction.begin(), prediction.end(), [](float sample) { return sample != 0.0f; }) !=
          prediction.end());

  std::vector<float> final_chunk;
  const GigaEdgeEnhanceRetCode final_code = test_data.denoiser->GetFinalChunk(final_chunk, ctx);
  REQUIRE(final_code == GigaEdgeEnhanceRetCode::SUCCESS);
  REQUIRE_FALSE(final_chunk.empty());
  require_finite(final_chunk);
  REQUIRE(std::vector<float>(final_chunk.size(), 0.0f) != final_chunk);
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the 8 kHz HQ model and 32 ms chunks [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitDenoiser(std::string(hq_8khz_yaml)).SetWav("test1_24_noised.wav"));
  const uint64_t hop = test_data.denoiser->GetModelInputFrameSize();
  REQUIRE(hop == 128);
  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext(8000, 8000);
  auto chunks = ChunkWav<float>(test_data.wav_data, 8000, 32, 32);
  REQUIRE(chunks.size() >= 10);
  chunks.resize(10);

  bool first_chunk = true;
  for (const auto& chunk : chunks) {
    std::vector<float> output;
    const GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(chunk, output, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    require_finite(output);
    if (first_chunk && chunk.size() > hop) {
      REQUIRE(output.size() + hop == chunk.size());
    } else {
      REQUIRE(output.size() == chunk.size());
    }
    first_chunk = false;
  }
  test_data.denoiser->DestroyContext(&ctx);
}

TEST_CASE("SUCCEED Run inference with the 8 kHz HQ model and variable chunk sizes [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitDenoiser(std::string(hq_8khz_yaml)).SetWav("test1_24_noised.wav"));
  const uint64_t hop = test_data.denoiser->GetModelInputFrameSize();
  REQUIRE(hop == 128);
  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext(8000, 8000);
  auto chunks = ChunkWav<float>(test_data.wav_data, 8000, 16, 16);
  const uint32_t factors[] = {2, 4, 3, 8, 2};
  std::size_t cursor = 0;
  bool first_chunk = true;
  std::vector<float> prediction;
  for (uint32_t factor : factors) {
    REQUIRE(cursor + factor <= chunks.size());
    const std::vector<float> merged = MergeChunks(chunks, cursor, factor);
    cursor += factor;
    std::vector<float> output;
    const GigaEdgeEnhanceRetCode ret_code = test_data.denoiser->ProcessChunk(merged, output, ctx);
    REQUIRE(ret_code >= GigaEdgeEnhanceRetCode::SUCCESS);
    require_finite(output);
    if (first_chunk && merged.size() > hop) {
      REQUIRE(output.size() + hop == merged.size());
    } else {
      REQUIRE(output.size() == merged.size());
    }
    first_chunk = false;
    prediction.insert(prediction.end(), output.begin(), output.end());
  }
  REQUIRE(std::find_if(prediction.begin(), prediction.end(), [](float sample) { return sample != 0.0f; }) !=
          prediction.end());
  test_data.denoiser->DestroyContext(&ctx);
}

#ifdef BUILD_RESAMPLER
TEST_CASE(
    "SUCCEED Run inference with the super fast 8 kHz on huge single chunk (16000 frames) [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitDenoiser(std::string(hq_8khz_yaml)).SetWav("test1_24_noised.wav"));

  giga_edge::enhance::Context<float>* ctx = test_data.denoiser->CreateContext(24000, 8000);

  const uint32_t chunk_size = 16;  // ms
  const uint32_t chunk_stride = chunk_size;
  const std::size_t target_chunks = 16000;

  auto chunks = ChunkWav<float>(test_data.wav_data, 24000, chunk_size, chunk_stride);

  REQUIRE_FALSE(chunks.empty());

  const std::size_t base_chunks = chunks.size();
  const std::size_t repeats = (target_chunks + base_chunks - 1) / base_chunks;

  const std::size_t samples_per_chunk = chunks.front().size();

  std::vector<float> huge_input;
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

  std::vector<float> ret;
  REQUIRE_NOTHROW(test_data.denoiser->ProcessChunk(huge_input, ret, ctx));

  REQUIRE_FALSE(ret.empty());

  test_data.denoiser->DestroyContext(&ctx);
}
#endif

#endif

TEST_CASE("SUCCEED Run AudioProcessing with RC default parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));

  test_data.dsp->AddRC();
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
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
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
}

TEST_CASE("SUCCEED Run AudioProcessing LC with parameters passed via constructor [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor());

  constexpr uint32_t fs = 16000;
  constexpr uint32_t chunk_ms = 32;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (12 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;
  const int32_t bypass_time = 3;
  const int32_t target_db = -23;
  const int32_t max_gain_db = 40;

  test_data.dsp->AddLC(fs, target_db, bypass_time, max_gain_db, false, false);

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

TEST_CASE("SUCCEED LC with VAD prevents clipping on loud input [gigaedge-enhance c++ api]") {
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
  const bool use_postprocessing = true;

  test_data.dsp->AddLC(fs, target_db, bypass_time, max_gain_db, use_vad, use_postprocessing);

  // Very loud input intended to trigger limiter (if enabled in this high-level path).
  const auto in = Tone(fs, num_samples, 220.0f, 2.0f);

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
  const float limiter_mag = dsplib::db2mag(-1.0f);
  REQUIRE(MaxAbs(out) <= limiter_mag + 5e-3f);
}

TEST_CASE("SUCCEED Run AudioProcessing with AGC default parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));

  test_data.dsp->AddAGC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(chunks_to_compare);

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

TEST_CASE("SUCCEED Run AudioProcessing with cahnged threshold for NG [gigaedge-enhance c++ api]") {
  const char change[] = R"(
sound_processing:
  noise_gate_ctrl:
    threshold: -15.0
)";
  auto test_data = std::move(TestFixture().InitAudioProcessor(std::string(change)).SetWav("test1_48_noised.wav"));

  test_data.dsp->AddNG();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() > orig_equal_0);
}

TEST_CASE("SUCCEED Run AudioProcessing with DRC default parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));

  test_data.dsp->AddDRC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with changed AGC parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));

  test_data.dsp->AddAGC(48000, 0.9, 20.0);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  chunks.resize(chunks_to_compare);

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

TEST_CASE("SUCCEED Run AudioProcessing with changed RC parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));

  test_data.dsp->AddRC(48000, -4);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_8;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
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
}

TEST_CASE("SUCCEED Run AudioProcessing with changed NG parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));
  test_data.dsp->AddNG(48000, -40, 0.05, 0.02, 0.05);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  std::vector<float> equal_0;
  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() == orig_equal_0);
}

TEST_CASE("SUCCEED Run AudioProcessing with changed DRC parameters [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture().InitAudioProcessor().SetWav("test1_48_noised.wav"));

  test_data.dsp->AddDRC(48000, 0, 5, 0.0, 0.01, 0.2);
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() != 0);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading RC parameters from config [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture()
                                 .InitAudioProcessor(std::filesystem::path("sound_processing_rc.yaml"))
                                 .SetWav("test1_48_noised.wav"));

  test_data.dsp->AddRC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
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
}

TEST_CASE("SUCCEED Run AudioProcessing with reading AGC parameters from config [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture()
                                 .InitAudioProcessor(std::filesystem::path("sound_processing_agc.yaml"))
                                 .SetWav("test1_48_noised.wav"));

  test_data.dsp->AddAGC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(chunks_to_compare);

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

TEST_CASE("SUCCEED Run AudioProcessing with reading NG parameters from config [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture()
                                 .InitAudioProcessor(std::filesystem::path("sound_processing_rc.yaml"))
                                 .SetWav("test1_48_noised.wav"));

  test_data.dsp->AddNG();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() > orig_equal_0);
}

TEST_CASE("SUCCEED Run AudioProcessing with reading DRC parameters from config [gigaedge-enhance c++ api]") {
  auto test_data = std::move(TestFixture()
                                 .InitAudioProcessor(std::filesystem::path("sound_processing_agc.yaml"))
                                 .SetWav("test1_48_noised.wav"));

  test_data.dsp->AddDRC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * chunk_size;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(test_data.wav_data, audio_src_sample_rate, chunk_size, chunk_stride);
  std::vector<float> reference(test_data.wav_data.begin(), test_data.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    test_data.dsp->ProcessChunk(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}
TEST_CASE("SUCCESS AudioProcessing accepts YAML string and can process a small frame [gigaedge-enhance c++ api]") {
  const std::string yaml = giga_edge::enhance::default_audio_processing_config_yaml;

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
TEST_CASE("FAIL AudioProcessing YAML string non-scalar where scalar required throws [gigaedge-enhance c++ api]") {
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
TEST_CASE("FAIL AudioProcessing YAML string: range_ctrl.sample_rate too large [gigaedge-enhance c++ api]") {
  const std::string bad = R"(
sound_processing:
  range_ctrl:
    sample_rate: 200000
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: auto_gain_ctrl.target_level <= 0 [gigaedge-enhance c++ api]") {
  const std::string bad = R"(
sound_processing:
  auto_gain_ctrl:
    target_level: -0.02
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: noise_gate_ctrl.attack_time <= 0 [gigaedge-enhance c++ api]") {
  const std::string bad = R"(
sound_processing:
  noise_gate_ctrl:
    sample_rate: 48000
    attack_time: -0.01
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: drc.release_time <= 0 [gigaedge-enhance c++ api]") {
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

TEST_CASE("FAIL AudioProcessing YAML string: loudness_ctrl.bypass_time <= 0 [gigaedge-enhance c++ api]") {
  const std::string bad = R"(
sound_processing:
  loudness_ctrl:
    bypass_time: -0.01
    max_gain_db: 40.0
)";
  REQUIRE_THROWS_AS(TestFixture().InitAudioProcessor(bad), std::runtime_error);
}

TEST_CASE("FAIL AudioProcessing YAML string: limiter_ctrl.threshold non-numeric [gigaedge-enhance c++ api]") {
  const std::string bad = R"(
sound_processing:
  limiter_ctrl:
    sample_rate: 48000
    threshold: abc
)";
  REQUIRE_THROWS(TestFixture().InitAudioProcessor(bad));
}
