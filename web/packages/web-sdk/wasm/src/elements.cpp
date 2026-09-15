#include <string>

#include "elements.h"

PipelineElement:: PipelineElement(uint32_t sample_rate) noexcept
: m_sample_rate(sample_rate) {}

#ifdef BUILD_DSP

AudioProcessPipelineElement::AudioProcessPipelineElement(const std::string& yaml_config, uint32_t sample_rate)
    : PipelineElement(sample_rate),
      m_yaml_config(yaml_config),
      m_normalizer(std::make_unique<gigaedge::denoiser::AudioProcessor>(yaml_config)) {
  m_normalizer->AddLC().AddNG().AddLim();
}

uint32_t AudioProcessPipelineElement::process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept {
  try {
    float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
    this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
    this->m_normalized.clear();
    if (m_normalizer->ProcessChunk(this->m_input, this->m_normalized) < GigaDenoiserRetCode::SUCCESS ||
        this->m_normalized.size() > chunk_size) {
      this->m_normalized.clear();
      return 0;
    }
    if (!this->m_normalized.empty()) {
      memcpy(src_ptr_, this->m_normalized.data(), this->m_normalized.size() * sizeof(float));
    }
    return this->m_normalized.size();
  } catch (...) {
    this->m_normalized.clear();
    return 0;
  }
}

void AudioProcessPipelineElement::resetContext() noexcept {
  try {
    auto normalizer = std::make_unique<gigaedge::denoiser::AudioProcessor>(m_yaml_config);
    normalizer->AddLC().AddNG().AddLim();
    m_normalizer = std::move(normalizer);
    m_input.clear();
    m_normalized.clear();
  } catch (...) {
  }
}

EchoCancellationElement::EchoCancellationElement(const std::string& yaml_config)
    : PipelineElement(0),
      m_yaml_config(yaml_config),
      m_aec(std::make_unique<gigaedge::denoiser::EchoCanceller>(yaml_config)) {}

uint32_t EchoCancellationElement::process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept {
  try {
    float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
    this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
    this->m_result.clear();
    if (m_aec->ProcessChunk(this->m_input, this->m_result) < GigaDenoiserRetCode::SUCCESS ||
        this->m_result.size() > chunk_size) {
      this->m_result.clear();
      return 0;
    }
    if (!this->m_result.empty()) {
      memcpy(src_ptr_, this->m_result.data(), this->m_result.size() * sizeof(float));
    }
    return this->m_result.size();
  } catch (...) {
    this->m_result.clear();
    return 0;
  }
}

uint32_t EchoCancellationElement::processMono(uintptr_t src_ptr,
                                              uintptr_t lbk_ptr,
                                              const uint32_t chunk_size) noexcept {
  try {
    float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
    float* lbk_ptr_ = reinterpret_cast<float*>(lbk_ptr);
    this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
    this->m_loopback.assign(lbk_ptr_, lbk_ptr_ + chunk_size);
    this->m_result.clear();
    if (m_aec->ProcessChunk(this->m_input, this->m_loopback, this->m_result) < GigaDenoiserRetCode::SUCCESS ||
        this->m_result.size() > chunk_size) {
      this->m_result.clear();
      return 0;
    }
    if (!this->m_result.empty()) {
      memcpy(src_ptr_, this->m_result.data(), this->m_result.size() * sizeof(float));
    }
    return this->m_result.size();
  } catch (...) {
    this->m_result.clear();
    return 0;
  }
}

void EchoCancellationElement::resetContext() noexcept {
  try {
    auto aec = std::make_unique<gigaedge::denoiser::EchoCanceller>(m_yaml_config);
    m_aec = std::move(aec);
    m_input.clear();
    m_loopback.clear();
    m_result.clear();
  } catch (...) {
  }
}

#endif

#ifndef BUILD_DSP

DenoisePipelineElement::DenoisePipelineElement(uint32_t sample_rate,
                                               std::shared_ptr<gigaedge::denoiser::PackedModelReader> model)
    : PipelineElement(sample_rate), m_denoiser(model) {
  ctx = m_denoiser.CreateContext(sample_rate, sample_rate);
}

DenoisePipelineElement::~DenoisePipelineElement() {
  try {
    if (ctx != nullptr) {
      m_denoiser.DestroyContext(&ctx);
    }
  } catch (...) {
  }
}

uint32_t DenoisePipelineElement::process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept {
  try {
    float* src_ptr_ = reinterpret_cast<float*>(src_ptr);
    this->m_input.assign(src_ptr_, src_ptr_ + chunk_size);
    this->m_result.clear();
    const GigaDenoiserRetCode ret_code = m_denoiser.ProcessChunk(this->m_input, this->m_result, ctx);
    if (ret_code < GigaDenoiserRetCode::SUCCESS || this->m_result.size() > chunk_size) {
      this->m_result.clear();
      return 0;
    }
    if (!this->m_result.empty()) {
      memcpy(src_ptr_, this->m_result.data(), this->m_result.size() * sizeof(float));
    }
    return this->m_result.size();
  } catch (...) {
    this->m_result.clear();
    return 0;
  }
}

void DenoisePipelineElement::resetContext() noexcept {
  try {
    m_denoiser.DestroyContext(&ctx);
    ctx = m_denoiser.CreateContext(m_sample_rate, m_sample_rate);
    m_input.clear();
    m_result.clear();
  } catch (...) {
    ctx = nullptr;
    m_result.clear();
  }
}

#endif
