#include <yaml-cpp/yaml.h>

#include <catch.hpp>
#include <cmath>
#include <future>  // NOLINT
#include <memory>
#include <string>

#include "enhance/denoiser.h"
#include "src/config_reader.h"
#include "src/feature_extractor_runner.h"
#include "src/inference_runner.h"

namespace {
std::shared_ptr<giga_edge::enhance::DenoiserConfig> hybrid_config() {
#ifdef BUILD_TFLITE_ENGINE
  const std::string extension = "tflite";
#else
  const std::string extension = "onnx";
#endif
  return std::make_shared<giga_edge::enhance::DenoiserConfig>(giga_edge::enhance::StringConfigReader(
      "model:\n  model_path: data/models/hybrid_identity." + extension +
      "\n  model_input_ctx_len: 0\ninference:\n  model_type: denoiser_hybrid_web_model\n"
      "feature_params:\n  sampling_freq: 48000\n  frame_size_ms: 32\n  frame_stride_ms: 16\n"));
}

giga_edge::enhance::Task spectrum_task(giga_edge::enhance::Context<float>& context, uint32_t frames) {
  giga_edge::enhance::Task task;
  task.data.context = &context;
  for (uint32_t t = 0; t < frames; ++t) {
    giga_edge::enhance::ComplexNumber<float> spectrum(769, 769);
    for (uint32_t f = 0; f < 769; ++f) {
      spectrum.real[f] = 0.01f * static_cast<float>(f + t);
      spectrum.imag[f] = -0.02f * static_cast<float>(f + 2 * t);
    }
    task.data.input_spectrogram.push_back(spectrum);
  }
  task.state.advance_to(giga_edge::enhance::PipelineStage::input_spectrum_ready);
  return task;
}
}  // namespace

TEST_CASE("Hybrid runners preserve complex layout and independent recurrent states", "[hybrid]") {
  using namespace giga_edge::enhance;
  InferenceRunner runner(hybrid_config(), 1, false);
  DenoiseContextHybrid<float> first;
  DenoiseContextHybrid<float> second;
  for (const uint32_t frames : {1u, 3u, 2u}) {
    auto task = spectrum_task(first, frames);
    runner.infer(&task);
    REQUIRE(task.state.is(PipelineStage::model_output_ready));
    REQUIRE(first.stage2_hidden.size() == 8);
    REQUIRE(task.data.new_spectrogram.size() == 769 * frames);
    for (uint32_t t = 0; t < frames; ++t) {
      for (uint32_t f = 0; f < 769; ++f) {
        CHECK(first.noisy_ri[(f * frames + t) * 2] == task.data.input_spectrogram[t].real[f]);
        CHECK(first.noisy_ri[(f * frames + t) * 2 + 1] == task.data.input_spectrogram[t].imag[f]);
        CHECK(task.data.new_spectrogram.real[t * 769 + f] == task.data.input_spectrogram[t].real[f]);
        CHECK(task.data.new_spectrogram.imag[t * 769 + f] == task.data.input_spectrogram[t].imag[f]);
      }
    }
    CHECK(first.stage2_hidden[0] == (frames == 1 ? 0.0f : 7.0f));
    first.stage2_hidden[0] = 7.0f;
    auto other = spectrum_task(second, 1);
    runner.infer(&other);
    CHECK(second.stage2_hidden[0] == 0.0f);
  }
  CHECK(first.stage2_hidden[0] == 7.0f);
}

TEST_CASE("Hybrid runners reject invalid contexts and spectra", "[hybrid]") {
  using namespace giga_edge::enhance;
  InferenceRunner runner(hybrid_config(), 1, false);
  Context<float> wrong;
  auto task = spectrum_task(wrong, 1);
  REQUIRE_THROWS(runner.infer(&task));
  DenoiseContextHybrid<float> context;
  task = spectrum_task(context, 2);
  task.data.input_spectrogram[1].imag.resize(3);
  REQUIRE_THROWS(runner.infer(&task));
  task = spectrum_task(context, 1);
  task.data.input_spectrogram[0].resize(3);
  REQUIRE_THROWS(runner.infer(&task));
  REQUIRE_THROWS(runner.infer(nullptr));
}

TEST_CASE("Hybrid feature extraction reconstructs spectra without applying a mask", "[hybrid]") {
  using namespace giga_edge::enhance;
  auto config = hybrid_config();
  FeatureExtractorRunner runner(config);
  Context<float> context;
  for (const uint32_t frames : {1u, 3u, 2u}) {
    auto task = spectrum_task(context, frames);
    auto expected = ComplexNumber<float>::concat_parts(task.data.input_spectrogram);
    task.data.new_spectrogram = expected;
    task.state.advance_to(PipelineStage::model_output_ready);
    runner.run(&task);
    REQUIRE(task.state.is(PipelineStage::enhanced_ready));
    REQUIRE_FALSE(task.data.enhanced_wav.empty());
    for (uint32_t f = 0; f < 769; ++f) {
      CHECK(context.prev_spectrogram.real[f] == expected.real[(frames - 1) * 769 + f]);
      CHECK(context.prev_spectrogram.imag[f] == expected.imag[(frames - 1) * 769 + f]);
    }
  }
}


TEST_CASE("Hybrid spectrum reconstruction agrees for batched and sequential frames", "[hybrid]") {
  using namespace giga_edge::enhance;
  FeatureExtractorRunner runner(hybrid_config());
  Context<float> batch_context;
  Context<float> stream_context;
  auto batch = spectrum_task(batch_context, 7);
  batch.data.new_spectrogram = ComplexNumber<float>::concat_parts(batch.data.input_spectrogram);
  batch.state.advance_to(PipelineStage::model_output_ready);
  runner.run(&batch);
  std::vector<float> streamed;
  for (const auto& spectrum : batch.data.input_spectrogram) {
    Task frame;
    frame.data.context = &stream_context;
    frame.data.input_spectrogram = {spectrum};
    frame.data.new_spectrogram = spectrum;
    frame.state.advance_to(PipelineStage::input_spectrum_ready);
    frame.state.advance_to(PipelineStage::model_output_ready);
    runner.run(&frame);
    if (!frame.state.is(ChunkOutcome::warming_up)) {
      streamed.insert(streamed.end(), frame.data.enhanced_wav.begin(), frame.data.enhanced_wav.end());
    }
  }
  REQUIRE(streamed.size() == batch.data.enhanced_wav.size());
  for (size_t i = 0; i < streamed.size(); ++i) {
    CHECK(streamed[i] == Approx(batch.data.enhanced_wav[i]).margin(1e-6f));
  }
}


TEST_CASE("Public denoiser API creates and processes hybrid contexts", "[hybrid]") {
  using namespace giga_edge::enhance;
  Denoiser denoiser(YAML::Dump(hybrid_config()->params()));
  auto* context = denoiser.CreateContext();
  REQUIRE(dynamic_cast<DenoiseContextHybrid<float>*>(context) != nullptr);
  std::vector<float> input(768);
  for (size_t i = 0; i < input.size(); ++i) {
    input[i] = 0.1f * std::sin(static_cast<float>(i) * 0.01f);
  }
  for (uint32_t i = 0; i < 4; ++i) {
    std::vector<float> output;
    const auto result = denoiser.ProcessChunk(input, output, context);
    REQUIRE((result == GigaEdgeEnhanceRetCode::SUCCESS || result == GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION));
    if (i > 0) {
      REQUIRE(result == GigaEdgeEnhanceRetCode::SUCCESS);
      REQUIRE_FALSE(output.empty());
      for (float value : output) {
        REQUIRE(std::isfinite(value));
      }
    }
  }
  denoiser.DestroyContext(&context);
  REQUIRE(context == nullptr);
}


TEST_CASE("Hybrid runner handles concurrent streams with different chunk sizes", "[hybrid]") {
  using namespace giga_edge::enhance;
  InferenceRunner runner(hybrid_config(), 1, false);
  auto process = [&runner](uint32_t frames) {
    DenoiseContextHybrid<float> context;
    for (uint32_t i = 0; i < 12; ++i) {
      auto task = spectrum_task(context, frames);
      runner.infer(&task);
      if (task.data.new_spectrogram.size() != 769 * frames ||
          task.data.new_spectrogram.real[769 * frames - 1] != task.data.input_spectrogram.back().real[768]) {
        return false;
      }
    }
    return true;
  };
  auto first = std::async(std::launch::async, process, 1);
  auto second = std::async(std::launch::async, process, 3);
  REQUIRE(first.get());
  REQUIRE(second.get());
}

TEST_CASE("Hybrid model selection rejects discriminative weights", "[hybrid]") {
  using namespace giga_edge::enhance;
  auto params = YAML::Clone(hybrid_config()->params());
#ifdef BUILD_TFLITE_ENGINE
  params["model"]["model_path"] = "data/models/quality_model_48khz.tflite";
#else
  params["model"]["model_path"] = "data/models/quality_model_48khz.onnx";
#endif
  auto config = std::make_shared<DenoiserConfig>(StringConfigReader(YAML::Dump(params)));
  REQUIRE_THROWS(InferenceRunner(config, 1, false));
}
