#include "include/spench_audio_echo_cancelation.h"
#include "spench/src/spench_task.h"
// #include "spench/src/audio_processing.h"
#include <iostream>
#include <limits>
#include <algorithm>
#include <memory>

namespace spench {

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(const char* config_file) {
  auto cfg = std::string(config_file);
  if (cfg.empty()) {
    // throw exception
  } else {
    // m_dsp = std::make_unique<spench::AcouscticEchoCancelation>(cfg);
  }
}

SpenchAudioEchoCancelation::~SpenchAudioEchoCancelation() {
//   m_dsp.reset();
}

SpenchAudioEchoCancelation::SpenchAudioEchoCancelation(SpenchAudioEchoCancelation&&) {

}

std::vector<float>& SpenchAudioEchoCancelation::process(const std::vector<float>& chunk,
                                                        std::vector<float>& ret) {
//   return m_dsp->process(chunk, ret);
std::vector<float> tmp;
return tmp;
}

std::vector<int16_t>& SpenchAudioEchoCancelation::process(const std::vector<int16_t>& chunk16,
                                                          std::vector<int16_t>& ret16) {
  std::vector<float> input;
  std::vector<float> output;

  tools::convertPCM16ToFloat32(chunk16, input);
//   m_dsp->process(input, output);
  tools::convertFloat32ToPCM16(output, ret16);
  return ret16;
}

SpenchRetCode SpenchAudioEchoCancelation::ProcessChunk(const std::vector<float>& chunk,
                                                       std::vector<float>& ret,
                                                       SpenchContext<float>* ctx) {
  (void)ctx;
//   this->process(chunk, ret);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioEchoCancelation::ProcessChunk(const std::vector<int16_t>& chunk16,
                                               std::vector<int16_t>& ret16,
                                               SpenchContext<float>* ctx) {
  (void)ctx;
//   this->process(chunk16, ret16);
  return SpenchRetCode::SUCCESS;
}

std::vector<float>& SpenchAudioEchoCancelation::operator()(const std::vector<float>& chunk,
                                                   std::vector<float>& ret,
                                                   SpenchContext<float>* ctx) {
  (void)ctx;
//   return this->process(chunk, ret);
  std::vector<float> tmp;
  return tmp;
}

std::vector<int16_t>& SpenchAudioEchoCancelation::operator()(const std::vector<int16_t>& chunk16,
                                                     std::vector<int16_t>& ret16,
                                                     SpenchContext<float>* ctx) {
  (void)ctx;
//   return this->process(chunk16, ret16);
  std::vector<int16_t> tmp;
  return tmp;
}

SpenchRetCode SpenchAudioEchoCancelation::GetFinalChunk(std::vector<float>& in,
                                                SpenchContext<float>* ctx) {
  (*this)(in);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioEchoCancelation::GetFinalChunk(std::vector<int16_t>& in16,
                                                SpenchContext<float>* ctx) {
  (*this)(in16);
  return SpenchRetCode::SUCCESS;
}

std::vector<float>& SpenchAudioEchoCancelation::operator()(std::vector<float>& in,
                                                   SpenchContext<float>* ctx) {
  std::vector<float> ret;
  this->ProcessChunk(in, ret);
  std::copy(ret.begin(), ret.end(), in.begin());
  return in;
}

std::vector<int16_t>& SpenchAudioEchoCancelation::operator()(std::vector<int16_t>& in16,
                                                     SpenchContext<float>* ctx) {
  std::vector<int16_t> ret16;
  this->ProcessChunk(in16, ret16);
  std::copy(ret16.begin(), ret16.end(), in16.begin());
  return in16;
}

}  // namespace spench
