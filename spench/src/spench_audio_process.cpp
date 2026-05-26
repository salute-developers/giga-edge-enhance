#include "spench/spench_audio_process.h"

#include <limits>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>

#include "spench/src/spench_task.h"
#include "spench/src/handler/audio_processor_handler.h"
#include "spench/src/spench_logger.h"

namespace spench {
SpenchAudioProcess::SpenchAudioProcess()
: dsp_(nullptr) {
  dsp_ = std::make_unique<spench::AudioProcessorHandler>(
            std::make_shared<spench::AudioProcessorConfig>());
}

SpenchAudioProcess::SpenchAudioProcess(const char* config)
: dsp_(nullptr) {
  std::string config_file(config);
  if (!config_file.empty()) {
    dsp_ = std::make_unique<spench::AudioProcessorHandler>(
            std::make_shared<spench::AudioProcessorConfig>(spench::FileConfigReader(config)));
  } else {
    spdlog::info("SpenchAudioProcess input config is empty. Use default values");
    dsp_ = std::make_unique<spench::AudioProcessorHandler>(
              std::make_shared<spench::AudioProcessorConfig>());
  }
  spdlog::info("Create Spench audio processor from a configuration file");
}

SpenchAudioProcess::SpenchAudioProcess(const std::string& yaml_conf)
: dsp_(nullptr) {
  dsp_ = std::make_unique<spench::AudioProcessorHandler>(
              std::make_shared<spench::AudioProcessorConfig>(spench::StringConfigReader(yaml_conf)));
  spdlog::info("Create Spench audio processor from yaml string");
}

SpenchAudioProcess::~SpenchAudioProcess() {
  dsp_.reset();
}

std::vector<float>  SpenchAudioProcess::process(const std::vector<float>& chunk,
                                                      std::vector<float>& ret) {
  return dsp_->process(chunk, ret);
}

std::vector<int16_t>  SpenchAudioProcess::process(const std::vector<int16_t>& chunk16,
                                                        std::vector<int16_t>& ret16) {
  std::vector<float> input;
  std::vector<float> output;

  tools::convert_pcm16_to_float32(chunk16, input);
  dsp_->process(input, output);
  tools::convert_float32_to_pcm16(output, ret16);
  return ret16;
}

SpenchRetCode SpenchAudioProcess::ProcessChunk(const std::vector<float>& chunk,
                                               std::vector<float>& ret,
                                               SpenchContext<float>* ctx) {
  (void)ctx;
  this->process(chunk, ret);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcess::ProcessChunk(const std::vector<int16_t>& chunk16,
                                               std::vector<int16_t>& ret16,
                                               SpenchContext<float>* ctx) {
  (void)ctx;
  this->process(chunk16, ret16);
  return SpenchRetCode::SUCCESS;
}

std::vector<float>  SpenchAudioProcess::operator()(const std::vector<float>& chunk,
                                                   std::vector<float>& ret,
                                                   SpenchContext<float>* ctx) {
  (void)ctx;
  return this->process(chunk, ret);
}

std::vector<int16_t>  SpenchAudioProcess::operator()(const std::vector<int16_t>& chunk16,
                                                     std::vector<int16_t>& ret16,
                                                     SpenchContext<float>* ctx) {
  (void)ctx;
  return this->process(chunk16, ret16);
}

SpenchRetCode SpenchAudioProcess::GetFinalChunk(std::vector<float>& in,
                                                SpenchContext<float>* ctx) {
  (*this)(in);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcess::GetFinalChunk(std::vector<int16_t>& in16,
                                                SpenchContext<float>* ctx) {
  (*this)(in16);
  return SpenchRetCode::SUCCESS;
}

std::vector<float>  SpenchAudioProcess::operator()(std::vector<float>& in,
                                                   SpenchContext<float>* ctx) {
  std::vector<float> ret;
  this->ProcessChunk(in, ret);
  std::copy(ret.begin(), ret.end(), in.begin());
  return in;
}

std::vector<int16_t>  SpenchAudioProcess::operator()(std::vector<int16_t>& in16,
                                                     SpenchContext<float>* ctx) {
  std::vector<int16_t> ret16;
  this->ProcessChunk(in16, ret16);
  std::copy(ret16.begin(), ret16.end(), in16.begin());
  return in16;
}

SpenchAudioProcess& SpenchAudioProcess::AddAGC() {
  dsp_->addAGC();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddAGC(uint32_t audio_sample_rate,
                                               float target_power_level,
                                               float max_power_gain,
                                               float time_rise,
                                               float time_fall) {
  dsp_->addAGC({target_power_level, max_power_gain,
                  audio_sample_rate / 4, time_rise, time_fall});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddRC() {
  dsp_->addRC();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddRC(uint32_t audio_sample_rate,
                                              float target_level_db,
                                              float max_gain_db,
                                              float attack_time,
                                              float release_time,
                                              float hold_time) {
  dsp_->addRC({audio_sample_rate, target_level_db, max_gain_db,
                attack_time, release_time, hold_time});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLC() {
  dsp_->addLC();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLC(uint32_t audio_sample_rate,
                                              float target_level_db,
                                              float bypass_time,
                                              float max_gain_db,
                                              bool vad,
                                              bool post_processing) {
  dsp_->addLC({audio_sample_rate, target_level_db, bypass_time, max_gain_db, vad, post_processing});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddNG() {
  dsp_->addNoiseGate();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddNG(uint32_t audio_sample_rate,
                                              float threshold_db,
                                              float attack_time,
                                              float release_time,
                                              float hold_time) {
  dsp_->addNoiseGate({audio_sample_rate, threshold_db,
                  attack_time, release_time, hold_time});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLim() {
  dsp_->addLimiter();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLim(uint32_t audio_sample_rate,
                                              float threshold_db,
                                              float knee_width,
                                              float attack_time,
                                              float release_time) {
  dsp_->addLimiter({audio_sample_rate, threshold_db,
                  knee_width, attack_time, release_time});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddDRC() {
  dsp_->addDRCompressor();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddDRC(uint32_t audio_sample_rate,
                                               float threshold_db,
                                               uint32_t ratio,
                                               float knee_width,
                                               float attack_time,
                                               float release_time) {
  dsp_->addDRCompressor({audio_sample_rate, threshold_db,
                          ratio, knee_width, attack_time,
                          release_time});
  return *this;
}
}  // namespace spench
