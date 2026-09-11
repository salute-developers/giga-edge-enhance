#include <string>

#include "elements.h"

PipelineElement:: PipelineElement(uint32_t sample_rate) noexcept
: m_sample_rate(sample_rate) {}

#ifdef BUILD_DSP

AudioProcessPipelineElement::AudioProcessPipelineElement(const std::string& yaml_config, uint32_t sample_rate) :
                                                         PipelineElement(sample_rate),
                                                         m_normalizer(yaml_config) {
  m_normalizer.AddLC().AddNG().AddLim();
}

uint32_t AudioProcessPipelineElement::process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept {
  float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
  this->m_input.resize(chunk_size);
  this->m_normalized.resize(chunk_size);
  this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
  m_normalizer.ProcessChunk(this->m_input, this->m_normalized);

  if (this->m_normalized.size() > chunk_size) {
    return 0;
  }
  if (this->m_normalized.size()) {
    memcpy(src_ptr_, this->m_normalized.data(), this->m_normalized.size() * sizeof(float));
  }

  return this->m_normalized.size();
}

void AudioProcessPipelineElement::resetContext() noexcept {
}

EchoCancellationElement::EchoCancellationElement(const std::string& yaml_config)
: PipelineElement(0),
  m_aec(yaml_config) {
}

uint32_t EchoCancellationElement::process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept {
  float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
  this->m_input.resize(chunk_size);
  this->m_result.resize(chunk_size);
  this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
  m_aec.ProcessChunk(this->m_input, this->m_result);

  if (this->m_result.size() > chunk_size) {
    return 0;
  }
  if (this->m_result.size()) {
    memcpy(src_ptr_, this->m_result.data(), this->m_result.size() * sizeof(float));
  }

  return this->m_result.size();
}

uint32_t EchoCancellationElement::processMono(uintptr_t src_ptr,
                                              uintptr_t lbk_ptr,
                                              const uint32_t chunk_size) noexcept {
  float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
  float* lbk_ptr_ = reinterpret_cast<float*>(lbk_ptr);
  this->m_input.resize(chunk_size);
  this->m_loopback.resize(chunk_size);
  this->m_result.resize(chunk_size);
  this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
  this->m_loopback.assign(lbk_ptr_, lbk_ptr_ + chunk_size);
  m_aec.ProcessChunk(this->m_input, this->m_loopback, this->m_result);

  if (this->m_result.size() > chunk_size) {
    return 0;
  }
  if (this->m_result.size()) {
    memcpy(src_ptr_, this->m_result.data(), this->m_result.size() * sizeof(float));
  }

  return this->m_result.size();
}

void EchoCancellationElement::resetContext() noexcept {
}

#endif

#ifndef BUILD_DSP

DenoisePipelineElement::DenoisePipelineElement(uint32_t sample_rate, std::shared_ptr<spench::PackedModelReader> model)
  : PipelineElement(sample_rate),
    m_denoiser(model) {
  ctx = m_denoiser.CreateContext(sample_rate, sample_rate);
}

DenoisePipelineElement::~DenoisePipelineElement() {
  if (ctx != nullptr) {
    m_denoiser.DestroyContext(&ctx);
  }
}

uint32_t DenoisePipelineElement::process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept {
  float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
  this->m_input.resize(chunk_size);
  // this->m_result.resize run inside of ProcessChunk
  this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
  SpenchRetCode ret_code = m_denoiser.ProcessChunk(this->m_input,
                                                   this->m_result,
                                                   ctx);
  if (this->m_result.size() > chunk_size) {
    return 0;
  }
  if (ret_code >= SpenchRetCode::SUCCESS && this->m_result.size()) {
    memcpy(src_ptr_, this->m_result.data(), this->m_result.size() * sizeof(float));
  }

  return this->m_result.size();
}

void DenoisePipelineElement::resetContext() noexcept {
  m_denoiser.DestroyContext(&ctx);
  ctx = m_denoiser.CreateContext(m_sample_rate, m_sample_rate);
}

#endif
