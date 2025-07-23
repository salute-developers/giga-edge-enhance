#include "spench/spench_audio_echo_cancelation.h"
#include <limits>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>
#include "spench/src/spench_task.h"
#include "spench/src/acoustic_echo_cancelation_handler.h"
#include "spench/src/spench_logger.h"

namespace spench {

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(const char* config) {
  std::string config_file(config);
  if (!config_file.empty() && std::filesystem::exists(config_file)) {
      m_aec = std::make_unique<spench::AudioEchoCancelationHandler>(config_file);
  } else if (config_file.empty()) {
    spdlog::info("SpenchAudioEchoCancelation input config is empty. Use default values");
    m_aec = std::make_unique<spench::AudioEchoCancelationHandler>();
  } else {
    spdlog::error("Invalid or missing config file.");
    throw std::invalid_argument("Invalid or missing config file.");
  }
  spdlog::info("Create Spench echo cancelation form config {}", std::string(config));
}

SpenchAudioEchoCancelation::~SpenchAudioEchoCancelation() {
  m_aec.reset();
}

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(SpenchAudioEchoCancelation&& other) noexcept
    : m_aec(std::move(other.m_aec)) {}

std::vector<float>  SpenchAudioEchoCancelation::process(const std::vector<float>& input,
                                                        const std::vector<float>& loopback,
                                                        std::vector<float>& ret) {
  m_aec->Process(input, loopback, ret);
  return ret;
}

std::vector<int16_t>  SpenchAudioEchoCancelation::process(const std::vector<int16_t>& input16,
                                                          const std::vector<int16_t>& loopback16,
                                                          std::vector<int16_t>& ret16) {
  std::vector<float> input;
  std::vector<float> loopback;
  std::vector<float> output;

  tools::convertPCM16ToFloat32(input16, input);
  tools::convertPCM16ToFloat32(loopback16, loopback);
  m_aec->Process(input, loopback, output);
  tools::convertFloat32ToPCM16(output, ret16);
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

  tools::convertPCM16ToFloat32(interleaved_chunk16, interleaved_chunk);
  this->ProcessChunk(interleaved_chunk, output);
  tools::convertFloat32ToPCM16(output, ret16);
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
  return this->m_aec->GetSampleRate();
}
const uint16_t SpenchAudioEchoCancelation::GetFilterLength() {
  return this->m_aec->GetFilterLength();
}
const uint16_t SpenchAudioEchoCancelation::GetFrameLength() {
  return this->m_aec->GetFrameLength();
}
}  // namespace spench
