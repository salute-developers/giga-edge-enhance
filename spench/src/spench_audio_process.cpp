#include "spench/spench_audio_process.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>

#include "spench/src/handler/audio_processor_handler.h"
#include "spench/src/spench_logger.h"
#include "spench/src/spench_task.h"

namespace {

void require_parameter(bool condition, const char* message) {
  if (!condition) {
    throw std::invalid_argument(message);
  }
}

void validate_sample_rate(uint32_t sample_rate) {
  require_parameter(sample_rate > 0 && sample_rate <= 100000, "Audio sample rate must be in (0, 100000].");
}

void validate_finite(float value, const char* message) { require_parameter(std::isfinite(value), message); }

}  // namespace

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
  dsp_->add_agc();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddAGC(uint32_t audio_sample_rate,
                                               float target_power_level,
                                               float max_power_gain,
                                               float time_rise,
                                               float time_fall) {
  validate_sample_rate(audio_sample_rate);
  validate_finite(target_power_level, "AGC target level must be finite.");
  validate_finite(max_power_gain, "AGC maximum gain must be finite.");
  validate_finite(time_rise, "AGC rise time must be finite.");
  validate_finite(time_fall, "AGC fall time must be finite.");
  require_parameter(target_power_level > 0.0F, "AGC target level must be positive.");
  require_parameter(max_power_gain > 0.0F, "AGC maximum gain must be positive.");
  require_parameter(time_rise > 0.0F && time_fall > 0.0F, "AGC rise and fall times must be positive.");
  dsp_->add_agc(
      {target_power_level, max_power_gain, std::max<uint32_t>(1, audio_sample_rate / 4), time_rise, time_fall});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddRC() {
  dsp_->add_rc();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddRC(uint32_t audio_sample_rate,
                                              float target_level_db,
                                              float max_gain_db,
                                              float attack_time,
                                              float release_time,
                                              float hold_time) {
  validate_sample_rate(audio_sample_rate);
  validate_finite(target_level_db, "Range Control target level must be finite.");
  validate_finite(max_gain_db, "Range Control maximum gain must be finite.");
  validate_finite(attack_time, "Range Control attack time must be finite.");
  validate_finite(release_time, "Range Control release time must be finite.");
  validate_finite(hold_time, "Range Control hold time must be finite.");
  require_parameter(max_gain_db >= 0.0F, "Range Control maximum gain must be non-negative.");
  require_parameter(attack_time >= 0.0F && release_time >= 0.0F && hold_time >= 0.0F,
                    "Range Control timing values must be non-negative.");
  dsp_->add_rc({audio_sample_rate, target_level_db, max_gain_db,
                attack_time, release_time, hold_time});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLC() {
  dsp_->add_lc();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLC(uint32_t audio_sample_rate,
                                              float target_level_db,
                                              float bypass_time,
                                              float max_gain_db,
                                              bool vad,
                                              bool post_processing) {
  validate_sample_rate(audio_sample_rate);
  validate_finite(target_level_db, "Loudness Control target level must be finite.");
  validate_finite(bypass_time, "Loudness Control bypass time must be finite.");
  validate_finite(max_gain_db, "Loudness Control maximum gain must be finite.");
  require_parameter(bypass_time >= 0.0F, "Loudness Control bypass time must be non-negative.");
  require_parameter(max_gain_db >= 0.0F, "Loudness Control maximum gain must be non-negative.");
  dsp_->add_lc({audio_sample_rate, target_level_db, bypass_time, max_gain_db, vad, post_processing});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddNG() {
  dsp_->add_noise_gate();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddNG(uint32_t audio_sample_rate,
                                              float threshold_db,
                                              float attack_time,
                                              float release_time,
                                              float hold_time) {
  validate_sample_rate(audio_sample_rate);
  validate_finite(threshold_db, "Noise Gate threshold must be finite.");
  validate_finite(attack_time, "Noise Gate attack time must be finite.");
  validate_finite(release_time, "Noise Gate release time must be finite.");
  validate_finite(hold_time, "Noise Gate hold time must be finite.");
  require_parameter(attack_time > 0.0F && release_time > 0.0F && hold_time > 0.0F,
                    "Noise Gate timing values must be positive.");
  dsp_->add_noise_gate({audio_sample_rate, threshold_db,
                  attack_time, release_time, hold_time});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLim() {
  dsp_->add_limiter();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddLim(uint32_t audio_sample_rate,
                                              float threshold_db,
                                              float knee_width,
                                              float attack_time,
                                              float release_time) {
  validate_sample_rate(audio_sample_rate);
  validate_finite(threshold_db, "Limiter threshold must be finite.");
  validate_finite(knee_width, "Limiter knee width must be finite.");
  validate_finite(attack_time, "Limiter attack time must be finite.");
  validate_finite(release_time, "Limiter release time must be finite.");
  require_parameter(knee_width >= 0.0F && attack_time >= 0.0F && release_time >= 0.0F,
                    "Limiter knee width and timing values must be non-negative.");
  dsp_->add_limiter({audio_sample_rate, threshold_db,
                  knee_width, attack_time, release_time});
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddDRC() {
  dsp_->add_dr_compressor();
  return *this;
}

SpenchAudioProcess& SpenchAudioProcess::AddDRC(uint32_t audio_sample_rate,
                                               float threshold_db,
                                               uint32_t ratio,
                                               float knee_width,
                                               float attack_time,
                                               float release_time) {
  validate_sample_rate(audio_sample_rate);
  validate_finite(threshold_db, "Compressor threshold must be finite.");
  validate_finite(knee_width, "Compressor knee width must be finite.");
  validate_finite(attack_time, "Compressor attack time must be finite.");
  validate_finite(release_time, "Compressor release time must be finite.");
  require_parameter(ratio >= 1, "Compressor ratio must be at least one.");
  require_parameter(knee_width >= 0.0F && attack_time >= 0.0F && release_time >= 0.0F,
                    "Compressor knee width and timing values must be non-negative.");
  dsp_->add_dr_compressor({audio_sample_rate, threshold_db,
                          ratio, knee_width, attack_time,
                          release_time});
  return *this;
}
}  // namespace spench
