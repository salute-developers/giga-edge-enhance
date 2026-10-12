#include "spench/denoiser.h"

#include <spdlog/spdlog.h>

#include <memory>

#include "src/handler/denoiser_audio_handler.h"
#include "src/logger.h"
#include "src/task.h"

namespace gigaedge::spench {

Denoiser::Denoiser(const char* config_file)
    : Sequencer(),
      handler_(std::make_unique<DenoiserAudioHandler>(
          std::make_shared<gigaedge::spench::DenoiserConfig>(FileConfigReader(std::string(config_file))))) {
  spdlog::info("Create GigaEdgeSpench denoiser from config file {}", config_file);
}

Denoiser::Denoiser(const std::string& yaml_str)
    : Sequencer(),
      handler_(std::make_unique<DenoiserAudioHandler>(
          std::make_shared<gigaedge::spench::DenoiserConfig>(StringConfigReader(yaml_str)))) {
  spdlog::info("Create GigaEdgeSpench denoiser from YAML string config");
}

Denoiser::Denoiser(std::shared_ptr<gigaedge::spench::PackedModelReader> bundle)
    : Sequencer(),
      handler_(std::make_unique<DenoiserAudioHandler>(std::make_shared<gigaedge::spench::DenoiserConfig>(bundle))) {
  spdlog::info("Create GigaEdgeSpench denoiser from packed model");
}

Denoiser::Denoiser(Denoiser&& orig) noexcept : Sequencer(), handler_(std::move(orig.handler_)) {
  //  orig.handler_ is now nullptr
}

Denoiser::~Denoiser() { handler_.reset(); }

GigaEdgeSpenchRetCode Denoiser::ReloadConfig() { return handler_->reload_config(); }

uint32_t Denoiser::GetEngineThreads() { return handler_->get_engine_threads_num(); }

uint64_t Denoiser::GetModelSampleRate() { return handler_->get_model_sample_rate(); }

uint64_t Denoiser::GetModelInputFrameSize() { return handler_->get_input_frame_size(); }

StreamStatStruct Denoiser::GetStreamStat(const Context<float>* ctx) { return handler_->get_stat_info(ctx); }

GigaEdgeSpenchRetCode Denoiser::GetFinalChunk(std::vector<float>& ret, Context<float>* ctx) {
  return handler_->finalize_inference(ctx, ret);
}

GigaEdgeSpenchRetCode Denoiser::GetFinalChunk(std::vector<int16_t>& ret16, Context<float>* ctx) {
  std::vector<float> output;

  GigaEdgeSpenchRetCode ret_code = handler_->finalize_inference(ctx, output);
  tools::convert_float32_to_pcm16(output, ret16);

  return ret_code;
}

std::vector<float> Denoiser::operator()(std::vector<float>& ret, Context<float>* ctx) {
  this->GetFinalChunk(ret, ctx);
  return ret;
}

std::vector<int16_t> Denoiser::operator()(std::vector<int16_t>& ret16, Context<float>* ctx) {
  this->GetFinalChunk(ret16, ctx);
  return ret16;
}


Context<float>* Denoiser::CreateContext(uint32_t in_audio_sr, uint32_t out_audio_sr) {
  return handler_->create_context(in_audio_sr, out_audio_sr);
}

void Denoiser::DestroyContext(Context<float>** ctx) { handler_->destroy_context(ctx); }

uint64_t Denoiser::GetContextId(const Context<float>* ctx) { return ctx->id; }

void Denoiser::SetContextId(Context<float>* ctx, uint64_t id) { ctx->id = id; }

GigaEdgeSpenchRetCode Denoiser::ProcessChunk(const std::vector<float>& chunk, std::vector<float>& ret,
                                             Context<float>* ctx) {
  return handler_->process_frame(ctx, chunk, ret);
}

GigaEdgeSpenchRetCode Denoiser::ProcessChunk(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                             Context<float>* ctx) {
  std::vector<float> input;
  std::vector<float> output;

  tools::convert_pcm16_to_float32(chunk16, input);
  GigaEdgeSpenchRetCode ret_code = handler_->process_frame(ctx, input, output);
  tools::convert_float32_to_pcm16(output, ret16);

  return ret_code;
}

std::vector<float> Denoiser::operator()(const std::vector<float>& chunk, std::vector<float>& ret, Context<float>* ctx) {
  this->ProcessChunk(chunk, ret, ctx);
  return ret;
}

std::vector<int16_t> Denoiser::operator()(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                          Context<float>* ctx) {
  this->ProcessChunk(chunk16, ret16, ctx);
  return ret16;
}
}  // namespace gigaedge::spench
