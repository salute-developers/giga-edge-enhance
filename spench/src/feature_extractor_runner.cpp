#include "spench/src/feature_extractor_runner.h"
#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include <deque>
#include <algorithm>
#include <chrono> // NOLINT
#include <cassert>
#include <memory>

namespace spench {

uint64_t FeatureExtractorRunner::GetFrameSizeMs() const {
  return m_params.frame_size_ms;
}

uint64_t FeatureExtractorRunner::GetSamplingFreq() const {
  return m_params.sampling_freq;
}

uint64_t FeatureExtractorRunner::GetFrameSizeSamples() const {
  return m_params.NumFrameSizeSamples();
}

uint64_t FeatureExtractorRunner::GetFrameHopSizeSamples() const {
  return m_params.NumFrameStrideSamples();
}

uint64_t FeatureExtractorRunner::GetPowSpecFeatSizeSamples() const {
  return m_params.PowSpecFeatSz();
}

void FeatureExtractorRunner::ReadConfig(const YAML::Node& yaml_config) {
  m_params.ReadFeatureParamsNode(yaml_config);
}

void FeatureExtractorRunner::Run(spench::DenoiseTask* denoise_task) {
  auto& denoise_data = denoise_task->m_data;
  auto& denoise_context = denoise_data.m_context;

  if (denoise_task->m_stage == PipelineStage::NOISY_WAV) {
    if (denoise_data.m_waveform.size() == 0) {
      // error
      throw("Input signal is empty!");
    }

    m_feature_extractor_ptr->STFTApply(denoise_data.m_waveform,
                                denoise_data.m_input_spec);

    if (denoise_data.m_input_spec.size() < 1) {
      // error
      throw("Spectrogram is calculated incorectly! Something with input data!");
    }

    denoise_task->m_stage = PipelineStage::SPECTROGRAM;
  } else if (denoise_task->m_stage == PipelineStage::PREDICTIONS) {
    m_feature_extractor_ptr->calcEnhancedPartsInPlace(denoise_context->m_prev_features.front(),
                                                      denoise_data.m_network_output);

    uint32_t feat_num = denoise_context->m_prev_masked_spectrogram.real.size();
    if (feat_num == 0) {
       denoise_context->m_prev_masked_spectrogram = denoise_data.m_network_output;
       denoise_task->m_stage = PipelineStage::SKIPPED_WAV;
       return;
    }
    ComplexNumber<float> concatenatedSpec(feat_num * 2, feat_num * 2);

    auto previous_frame = std::slice(0, feat_num, 1);
    concatenatedSpec.real[previous_frame] = denoise_context->m_prev_masked_spectrogram.real;
    concatenatedSpec.imag[previous_frame] = denoise_context->m_prev_masked_spectrogram.imag;

    auto current_frame = std::slice(feat_num, feat_num, 1);
    concatenatedSpec.real[current_frame] = denoise_data.m_network_output.real;
    concatenatedSpec.imag[current_frame] = denoise_data.m_network_output.imag;

    m_feature_extractor_ptr->iSTFTApply(concatenatedSpec,
                                denoise_data.m_enhanced_wav);
    denoise_context->m_prev_masked_spectrogram = denoise_data.m_network_output;
    denoise_task->m_stage = PipelineStage::ENHANCED_WAV;
  } else if (denoise_task->m_stage == PipelineStage::SKIPPED_WAV) {
    // network pass the input to output and nothing to do
    denoise_data.m_enhanced_wav.resize(m_params.NumFrameStrideSamples());
  } else {
    throw("State is incorrect!");
  }
}

void FeatureExtractorRunner::WarmUp() {
  spench::DenoiseTask denoise_task;
  std::unique_ptr<spench::DenoiseContext<float>> ctx(new spench::DenoiseContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_data.m_waveform.resize(GetFrameSizeSamples(), 0.0);
  Run(&denoise_task);
}
}  // namespace spench
