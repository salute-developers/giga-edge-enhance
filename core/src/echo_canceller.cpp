#include "spench/echo_canceller.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>

#include "src/handler/acoustic_echo_cancelation_handler.h"
#include "src/logger.h"
#include "src/task.h"

namespace gigaedge::spench {

EchoCanceller::EchoCanceller(const char* config) : aec_(nullptr) {
  std::string config_file(config);
  if (!config_file.empty() && std::filesystem::exists(config_file)) {
    aec_ = std::make_unique<gigaedge::spench::AudioEchoCancelationHandler>(
        std::make_shared<gigaedge::spench::AecConfig>(gigaedge::spench::FileConfigReader(config)));
  } else if (config_file.empty()) {
    spdlog::info("EchoCanceller input config is empty. Use default values");
    aec_ = std::make_unique<gigaedge::spench::AudioEchoCancelationHandler>(
        std::make_shared<gigaedge::spench::AecConfig>());
  } else {
    spdlog::error("Invalid or missing config file.");
    throw std::invalid_argument("Invalid or missing config file.");
  }
  spdlog::info("Create GigaEdgeSpench echo cancelation form config {}", std::string(config));
}

EchoCanceller::EchoCanceller(const std::string& yaml_conf) : aec_(nullptr) {
  aec_ = std::make_unique<gigaedge::spench::AudioEchoCancelationHandler>(
      std::make_shared<gigaedge::spench::AecConfig>(gigaedge::spench::StringConfigReader(yaml_conf)));

  spdlog::info("Create GigaEdgeSpench echo cancelation from yaml string");
}

EchoCanceller::~EchoCanceller() { aec_.reset(); }

EchoCanceller::EchoCanceller(EchoCanceller&& other) noexcept : aec_(std::move(other.aec_)) {}

std::vector<float> EchoCanceller::process(const std::vector<float>& input, const std::vector<float>& loopback,
                                          std::vector<float>& ret) {
  aec_->process(input, loopback, ret);
  return ret;
}

std::vector<int16_t> EchoCanceller::process(const std::vector<int16_t>& input16, const std::vector<int16_t>& loopback16,
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

GigaEdgeSpenchRetCode EchoCanceller::ProcessChunk(const std::vector<float>& interleaved_chunk, std::vector<float>& ret,
                                                  Context<float>* ctx) {
  (void)ctx;
  std::vector<float> input_signal, loopback;

  convertFromInterleaved(interleaved_chunk, input_signal, loopback);
  this->process(input_signal, loopback, ret);
  return GigaEdgeSpenchRetCode::SUCCESS;
}

GigaEdgeSpenchRetCode EchoCanceller::ProcessChunk(const std::vector<float>& input_chunk,
                                                  const std::vector<float>& loopback, std::vector<float>& ret) {
  this->process(input_chunk, loopback, ret);
  return GigaEdgeSpenchRetCode::SUCCESS;
}

GigaEdgeSpenchRetCode EchoCanceller::ProcessChunk(const std::vector<int16_t>& interleaved_chunk16,
                                                  std::vector<int16_t>& ret16, Context<float>* ctx) {
  (void)ctx;
  std::vector<float> interleaved_chunk;
  std::vector<float> output;

  tools::convert_pcm16_to_float32(interleaved_chunk16, interleaved_chunk);
  this->ProcessChunk(interleaved_chunk, output);
  tools::convert_float32_to_pcm16(output, ret16);
  return GigaEdgeSpenchRetCode::SUCCESS;
}

std::vector<float> EchoCanceller::operator()(const std::vector<float>& interleaved_chunk, std::vector<float>& ret,
                                             Context<float>* ctx) {
  (void)ctx;
  this->ProcessChunk(interleaved_chunk, ret);
  return ret;
}

std::vector<int16_t> EchoCanceller::operator()(const std::vector<int16_t>& interleaved_chunk16,
                                               std::vector<int16_t>& ret16, Context<float>* ctx) {
  (void)ctx;
  this->ProcessChunk(interleaved_chunk16, ret16);
  return ret16;
}

GigaEdgeSpenchRetCode EchoCanceller::GetFinalChunk(std::vector<float>& interleaved_chunk, Context<float>* ctx) {
  (*this)(interleaved_chunk);
  return GigaEdgeSpenchRetCode::SUCCESS;
}

GigaEdgeSpenchRetCode EchoCanceller::GetFinalChunk(std::vector<int16_t>& interleaved_chunk16, Context<float>* ctx) {
  (*this)(interleaved_chunk16);
  return GigaEdgeSpenchRetCode::SUCCESS;
}

std::vector<float> EchoCanceller::operator()(std::vector<float>& interleaved_chunk, Context<float>* ctx) {
  std::vector<float> ret;
  this->ProcessChunk(interleaved_chunk, ret);
  std::copy(ret.begin(), ret.end(), interleaved_chunk.begin());
  return interleaved_chunk;
}

std::vector<int16_t> EchoCanceller::operator()(std::vector<int16_t>& interleaved_chunk16, Context<float>* ctx) {
  std::vector<int16_t> ret16;
  this->ProcessChunk(interleaved_chunk16, ret16);
  std::copy(ret16.begin(), ret16.end(), interleaved_chunk16.begin());
  return interleaved_chunk16;
}

// NOLINTNEXTLINE — frozen GES_API (convertToInterleaved)
std::vector<float> EchoCanceller::convertToInterleaved(const std::vector<float>& mic, const std::vector<float>& lbk) {
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

// NOLINTNEXTLINE — frozen GES_API (convertFromInterleaved)
void EchoCanceller::convertFromInterleaved(const std::vector<float>& interlvd, std::vector<float>& mic,
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

const uint32_t EchoCanceller::GetSampleRate() { return this->aec_->get_sample_rate(); }
const uint16_t EchoCanceller::GetFilterLength() { return this->aec_->get_filter_length(); }
const uint16_t EchoCanceller::GetFrameLength() { return this->aec_->get_frame_length(); }
}  // namespace gigaedge::spench
