#include "spench/spench_audio_echo_cancelation.h"
#include <limits>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>
#include "spench/src/spench_task.h"
#include "spench/src/handler/acoustic_echo_cancelation_handler.h"
#include "spench/src/spench_logger.h"

namespace spench {

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(const char* config) : aec_(nullptr) {
  std::string config_file(config);
  if (!config_file.empty() && std::filesystem::exists(config_file)) {
    aec_ = std::make_unique<spench::AudioEchoCancelationHandler>(
            std::make_shared<spench::AecConfig>(spench::FileConfigReader(config)));
  } else if (config_file.empty()) {
    spdlog::info("SpenchAudioEchoCancelation input config is empty. Use default values");
    aec_ = std::make_unique<spench::AudioEchoCancelationHandler>(
                std::make_shared<spench::AecConfig>());
  } else {
    spdlog::error("Invalid or missing config file.");
    throw std::invalid_argument("Invalid or missing config file.");
  }
  spdlog::info("Create Spench echo cancelation form config {}", std::string(config));
}

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(const std::string& yaml_conf)
: aec_(nullptr) {
  aec_ = std::make_unique<spench::AudioEchoCancelationHandler>(
            std::make_shared<spench::AecConfig>(spench::StringConfigReader(yaml_conf)));

  spdlog::info("Create Spench echo cancelation from yaml string");
}

SpenchAudioEchoCancelation::~SpenchAudioEchoCancelation() {
  aec_.reset();
}

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(SpenchAudioEchoCancelation&& other) noexcept
    : aec_(std::move(other.aec_)) {}

std::vector<float>  SpenchAudioEchoCancelation::process(const std::vector<float>& input,
                                                        const std::vector<float>& loopback,
                                                        std::vector<float>& ret) {
  aec_->process(input, loopback, ret);
  return ret;
}

std::vector<int16_t>  SpenchAudioEchoCancelation::process(const std::vector<int16_t>& input16,
                                                          const std::vector<int16_t>& loopback16,
                                                          std::vector<int16_t>& ret16) {
  std::vector<float> input;
  std::vector<float> loopback;
  std::vector<float> output;

  tools::convert_pcm16_to_float32(input16, input);
  tools::convert_pcm16_to_float32(loopback16, loopback);
  aec_->process(input, loopback, output);
  tools::convert_float32_to_pcm16(output, ret16);
  return ret16;
}

SpenchRetCode SpenchAudioEchoCancelation::ProcessChunk(const std::vector<float>& interleaved_chunk,
                                                       std::vector<float>& ret,
                                                       SpenchContext<float>* ctx) {
  (void)ctx;
  std::vector<float> input_signal, loopback;

  convertFromInterleaved(interleaved_chunk, input_signal, loopback);
  this->process(input_signal, loopback, ret);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioEchoCancelation::ProcessChunk(const std::vector<float>& input_chunk,
                                                       const std::vector<float>& loopback,
                                                       std::vector<float>& ret) {
  this->process(input_chunk, loopback, ret);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioEchoCancelation::ProcessChunk(const std::vector<int16_t>& interleaved_chunk16,
                                               std::vector<int16_t>& ret16,
                                               SpenchContext<float>* ctx) {
  (void)ctx;
  std::vector<float> interleaved_chunk;
  std::vector<float> output;

  tools::convert_pcm16_to_float32(interleaved_chunk16, interleaved_chunk);
  this->ProcessChunk(interleaved_chunk, output);
  tools::convert_float32_to_pcm16(output, ret16);
  return SpenchRetCode::SUCCESS;
}

std::vector<float>  SpenchAudioEchoCancelation::operator()(const std::vector<float>& interleaved_chunk,
                                                   std::vector<float>& ret,
                                                   SpenchContext<float>* ctx) {
  (void)ctx;
  this->ProcessChunk(interleaved_chunk, ret);
  return ret;
}

std::vector<int16_t>  SpenchAudioEchoCancelation::operator()(const std::vector<int16_t>& interleaved_chunk16,
                                                     std::vector<int16_t>& ret16,
                                                     SpenchContext<float>* ctx) {
  (void)ctx;
  this->ProcessChunk(interleaved_chunk16, ret16);
  return ret16;
}

SpenchRetCode SpenchAudioEchoCancelation::GetFinalChunk(std::vector<float>& interleaved_chunk,
                                                SpenchContext<float>* ctx) {
  (*this)(interleaved_chunk);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioEchoCancelation::GetFinalChunk(std::vector<int16_t>& interleaved_chunk16,
                                                SpenchContext<float>* ctx) {
  (*this)(interleaved_chunk16);
  return SpenchRetCode::SUCCESS;
}

std::vector<float>  SpenchAudioEchoCancelation::operator()(std::vector<float>& interleaved_chunk,
                                                   SpenchContext<float>* ctx) {
  std::vector<float> ret;
  this->ProcessChunk(interleaved_chunk, ret);
  std::copy(ret.begin(), ret.end(), interleaved_chunk.begin());
  return interleaved_chunk;
}

std::vector<int16_t>  SpenchAudioEchoCancelation::operator()(std::vector<int16_t>& interleaved_chunk16,
                                                     SpenchContext<float>* ctx) {
  std::vector<int16_t> ret16;
  this->ProcessChunk(interleaved_chunk16, ret16);
  std::copy(ret16.begin(), ret16.end(), interleaved_chunk16.begin());
  return interleaved_chunk16;
}

std::vector<float> SpenchAudioEchoCancelation::convertToInterleaved(const std::vector<float>& mic,
                                                                    const std::vector<float>& lbk) {
  if (mic.size() != lbk.size()) {
    spdlog::error("Original and loopback signals must have the same size.");
    throw std::invalid_argument("Original and loopback signals must have the same size.");
  }

  std::vector<float> interlvd;
  interlvd.reserve(mic.size() * 2);

  for (size_t i = 0; i < mic.size(); ++i) {
      interlvd.push_back(mic[i]);
      interlvd.push_back(lbk[i]);
  }

  return interlvd;
}

void SpenchAudioEchoCancelation::convertFromInterleaved(const std::vector<float>& interlvd,
                                                              std::vector<float>& mic,
                                                              std::vector<float>& lbk) {
  if (interlvd.size() % 2 != 0) {
      throw std::invalid_argument("Interleaved signal size must be even.");
  }

  mic.clear();
  lbk.clear();
  mic.reserve(interlvd.size() / 2);
  lbk.reserve(interlvd.size() / 2);

  for (size_t i = 0; i < interlvd.size(); i += 2) {
      mic.push_back(interlvd[i]);
      lbk.push_back(interlvd[i + 1]);
  }
}

const uint32_t SpenchAudioEchoCancelation::GetSampleRate() {
  return this->aec_->get_sample_rate();
}
const uint16_t SpenchAudioEchoCancelation::GetFilterLength() {
  return this->aec_->get_filter_length();
}
const uint16_t SpenchAudioEchoCancelation::GetFrameLength() {
  return this->aec_->get_frame_length();
}
}  // namespace spench
