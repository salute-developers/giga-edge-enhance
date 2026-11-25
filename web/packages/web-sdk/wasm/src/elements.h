#pragma once

#include <memory>
#include <string>
#include <vector>

#include "enhance/packed_model.h"
#include "giga-edge-enhance.h"

class PipelineElement {
 protected:
  uint32_t m_sample_rate = 0;
  std::vector<float> m_input;
  std::vector<float> m_result;
  std::vector<float> m_normalized;
 public:
  explicit PipelineElement(uint32_t sample_rate) noexcept;
  PipelineElement() = delete;
  PipelineElement& operator=(const PipelineElement&) = delete;
  PipelineElement(const PipelineElement&) = delete;
  virtual uint32_t process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept = 0;
  virtual void resetContext() noexcept = 0;
  virtual ~PipelineElement() = default;
};

#ifdef BUILD_DSP

class AudioProcessPipelineElement : public PipelineElement {
 private:
  std::string m_yaml_config;
  std::unique_ptr<giga_edge::enhance::AudioProcessor> m_normalizer;

 public:
  AudioProcessPipelineElement(const std::string& yaml_config, uint32_t sample_rate = 0);

  uint32_t process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept final;

  void resetContext() noexcept final;
};

class EchoCancellationElement : public PipelineElement {
 private:
  std::string m_yaml_config;
  std::unique_ptr<giga_edge::enhance::EchoCanceller> m_aec;
  std::vector<float> m_loopback;

 public:
  EchoCancellationElement(const std::string& yaml_config);
  uint32_t process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept final;

  uint32_t processMono(uintptr_t src_ptr, uintptr_t lbk_ptr, const uint32_t chunk_size) noexcept;

  void resetContext() noexcept final;
};

#endif

#ifndef BUILD_DSP

class DenoisePipelineElement : public PipelineElement {
 private:
  giga_edge::enhance::Denoiser m_denoiser;
  giga_edge::enhance::Context<float>* ctx;

 public:
  DenoisePipelineElement(uint32_t sample_rate, std::shared_ptr<giga_edge::enhance::PackedModelReader> model);
  ~DenoisePipelineElement() override;
  uint32_t process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept final;

  void resetContext() noexcept final;
};

#endif
