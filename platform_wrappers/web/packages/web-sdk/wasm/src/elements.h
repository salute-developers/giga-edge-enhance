#pragma once

#include <vector>
#include <string>
#include <memory>

#include "spench.h"
#include "spench/packed_model.h"

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
  spench::SpenchAudioProcess m_normalizer;

 public:
  AudioProcessPipelineElement(const std::string& yaml_config, uint32_t sample_rate = 0);

  uint32_t process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept final;

  void resetContext() noexcept;
};

class EchoCancellationElement : public PipelineElement {
 private:
  spench::SpenchAudioEchoCancelation m_aec;
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
  spench::SpenchDenoise m_denoiser;
  spench::SpenchContext<float>* ctx;

  public:
   DenoisePipelineElement(uint32_t sample_rate, std::shared_ptr<spench::PackedModelReader> model);
   ~DenoisePipelineElement() override;
   uint32_t process(uintptr_t src_ptr, const uint32_t chunk_size) noexcept final;

  void resetContext() noexcept;
};

#endif
