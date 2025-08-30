#include "spench/spench_audio_denoise.h"
#include "spench/src/spench_task.h"
#include "spench/src/base_audio_handler.h"
#include "spench/src/denoiser_audio_handler.h"
#include "spench/src/spench_logger.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace spench {

// File-based config constructor
SpenchDenoise::SpenchDenoise(const char* config_file,
                             const char* password,
                             const char* checksum)
: ISpenchSequencer(),
  m_handler(std::make_unique<DenoiserAudioHandler>(
                              std::make_unique<spench::DenoiserConfigReader>(
                                std::make_unique<FileConfigReader>(std::string(config_file))),
                              std::string(password),
                              std::string(checksum))) {
  spdlog::info("Create Spench denoiser from config file {}", config_file);
}

// YAML-based config constructor
SpenchDenoise::SpenchDenoise(const std::string& yaml_str,
                             const char* password,
                             const char* checksum)
: ISpenchSequencer(),
  m_handler(std::make_unique<DenoiserAudioHandler>(
                              std::make_unique<spench::DenoiserConfigReader>(
                                std::make_unique<StringConfigReader>(yaml_str)),
                              password,
                              checksum)) {
  spdlog::info("Create Spench denoiser from YAML string config");
}

SpenchDenoise::SpenchDenoise(SpenchDenoise&& orig) noexcept : ISpenchSequencer()
                                                            , m_handler(std::move(orig.m_handler)) {
//  orig.m_handler is now nullptr
}

SpenchDenoise::~SpenchDenoise() {
  m_handler.reset();
}

SpenchRetCode SpenchDenoise::ReloadConfig() {
  return m_handler->ReloadConfig();
}

uint32_t SpenchDenoise::GetEngineThreads() {
  return m_handler->GetEngineThreadsNum();
}

uint64_t SpenchDenoise::GetModelSampleRate() {
  return m_handler->GetModelSampleRate();
}

uint64_t SpenchDenoise::GetModelInputFrameSize() {
  return m_handler->GetInputFrameSize();
}

StreamStatStruct SpenchDenoise::GetStreamStat(const SpenchContext<float>* ctx) {
  return m_handler->GetStatInfo(ctx);
}

SpenchRetCode SpenchDenoise::GetFinalChunk(std::vector<float>& ret,
                                           SpenchContext<float>* ctx) {
  return m_handler->FinalizeInference(ctx, ret);
}

SpenchRetCode SpenchDenoise::GetFinalChunk(std::vector<int16_t>& ret16,
                                           SpenchContext<float>* ctx) {
  std::vector<float> output;

  SpenchRetCode ret_code = m_handler->FinalizeInference(ctx, output);
  tools::convertFloat32ToPCM16(output, ret16);

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
  return m_handler->CreateContext(in_audio_sr, out_audio_sr);
}

void SpenchDenoise::DestroyContext(SpenchContext<float>** ctx) {
  m_handler->DestroyContext(ctx);
}

uint64_t SpenchDenoise::GetContextId(const SpenchContext<float>* ctx) {
  return ctx->m_id;
}

void SpenchDenoise::SetContextId(SpenchContext<float>* ctx, uint64_t id) {
  ctx->m_id = id;
}

SpenchRetCode SpenchDenoise::ProcessChunk(const std::vector<float>& chunk,
                                          std::vector<float>& ret,
                                          SpenchContext<float>* ctx) {
  return m_handler->ProcessFrame(ctx, chunk, ret);
}

SpenchRetCode SpenchDenoise::ProcessChunk(const std::vector<int16_t>& chunk16,
                                          std::vector<int16_t>& ret16,
                                          SpenchContext<float>* ctx) {
  std::vector<float> input;
  std::vector<float> output;

  tools::convertPCM16ToFloat32(chunk16, input);
  SpenchRetCode ret_code = m_handler->ProcessFrame(ctx, input, output);
  tools::convertFloat32ToPCM16(output, ret16);

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
