#include "spench/spench_audio_denoise.h"
#include "spench/src/spench_task.h"
#include "spench/src/handler/denoiser_audio_handler.h"
#include "spench/src/spench_logger.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace spench {

SpenchDenoise::SpenchDenoise(const char* config_file)
: ISpenchSequencer(),
  handler_(std::make_unique<DenoiserAudioHandler>(
                               std::make_shared<spench::DenoiserConfig>(
                                 FileConfigReader(std::string(config_file))))) {
  spdlog::info("Create Spench denoiser from config file {}", config_file);
}

SpenchDenoise::SpenchDenoise(const std::string& yaml_str)
: ISpenchSequencer(),
  handler_(std::make_unique<DenoiserAudioHandler>(
                               std::make_shared<spench::DenoiserConfig>(
                                 StringConfigReader(yaml_str)))) {
  spdlog::info("Create Spench denoiser from YAML string config");
}

SpenchDenoise::SpenchDenoise(std::shared_ptr<spench::PackedModelReader> bundle)
: ISpenchSequencer(),
  handler_(std::make_unique<DenoiserAudioHandler>(
                               std::make_shared<spench::DenoiserConfig>(bundle))) {
  spdlog::info("Create Spench denoiser from packed model");
}

SpenchDenoise::SpenchDenoise(SpenchDenoise&& orig) noexcept : ISpenchSequencer()
                                                            , handler_(std::move(orig.handler_)) {
//  orig.handler_ is now nullptr
}

SpenchDenoise::~SpenchDenoise() {
  handler_.reset();
}

SpenchRetCode SpenchDenoise::ReloadConfig() {
  return handler_->reload_config();
}

uint32_t SpenchDenoise::GetEngineThreads() {
  return handler_->get_engine_threads_num();
}

uint64_t SpenchDenoise::GetModelSampleRate() {
  return handler_->get_model_sample_rate();
}

uint64_t SpenchDenoise::GetModelInputFrameSize() {
  return handler_->get_input_frame_size();
}

StreamStatStruct SpenchDenoise::GetStreamStat(const SpenchContext<float>* ctx) {
  return handler_->get_stat_info(ctx);
}

SpenchRetCode SpenchDenoise::GetFinalChunk(std::vector<float>& ret,
                                           SpenchContext<float>* ctx) {
  return handler_->finalize_inference(ctx, ret);
}

SpenchRetCode SpenchDenoise::GetFinalChunk(std::vector<int16_t>& ret16,
                                           SpenchContext<float>* ctx) {
  std::vector<float> output;

  SpenchRetCode ret_code = handler_->finalize_inference(ctx, output);
  tools::convert_float32_to_pcm16(output, ret16);

  return ret_code;
}

std::vector<float>  SpenchDenoise::operator()(std::vector<float>& ret,
                                              SpenchContext<float>* ctx) {
  this->GetFinalChunk(ret, ctx);
  return ret;
}

std::vector<int16_t>  SpenchDenoise::operator()(std::vector<int16_t>& ret16,
                                                SpenchContext<float>* ctx) {
  this->GetFinalChunk(ret16, ctx);
  return ret16;
}


SpenchContext<float>* SpenchDenoise::CreateContext(uint32_t in_audio_sr,
                                                    uint32_t out_audio_sr) {
  return handler_->create_context(in_audio_sr, out_audio_sr);
}

void SpenchDenoise::DestroyContext(SpenchContext<float>** ctx) {
  handler_->destroy_context(ctx);
}

uint64_t SpenchDenoise::GetContextId(const SpenchContext<float>* ctx) {
  return ctx->id;
}

void SpenchDenoise::SetContextId(SpenchContext<float>* ctx, uint64_t id) {
  ctx->id = id;
}

SpenchRetCode SpenchDenoise::ProcessChunk(const std::vector<float>& chunk,
                                          std::vector<float>& ret,
                                          SpenchContext<float>* ctx) {
  return handler_->process_frame(ctx, chunk, ret);
}

SpenchRetCode SpenchDenoise::ProcessChunk(const std::vector<int16_t>& chunk16,
                                          std::vector<int16_t>& ret16,
                                          SpenchContext<float>* ctx) {
  std::vector<float> input;
  std::vector<float> output;

  tools::convert_pcm16_to_float32(chunk16, input);
  SpenchRetCode ret_code = handler_->process_frame(ctx, input, output);
  tools::convert_float32_to_pcm16(output, ret16);

  return ret_code;
}

std::vector<float>  SpenchDenoise::operator()(const std::vector<float>& chunk,
                                              std::vector<float>& ret,
                                              SpenchContext<float>* ctx) {
  this->ProcessChunk(chunk, ret, ctx);
  return ret;
}

std::vector<int16_t>  SpenchDenoise::operator()(const std::vector<int16_t>& chunk16,
                                                std::vector<int16_t>& ret16,
                                                SpenchContext<float>* ctx) {
  this->ProcessChunk(chunk16, ret16, ctx);
  return ret16;
}
}  // namespace spench
