#include "spench/src/feature_extractor_runner.h"
#include <cmath>
#include <vector>
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

void FeatureExtractorRunner::Run(spench::SpenchTask* spench_task) {
  auto& spench_data = spench_task->m_data;
  auto& spench_context = spench_data.context;

  if (spench_task->m_stage == PipelineStage::ORIG_CHUNK) {
    if (spench_data.waveform.size() == 0) {
      // error
      throw("Input signal is empty!");
    }

    m_feature_extractor_ptr->STFTApply(spench_data.waveform,
                                spench_data.input_spectrogram);

    if (spench_data.input_spectrogram.size() < 1) {
      // error
      throw("Spectrogram is calculated incorectly! Something with input data!");
    }

    spench_task->m_stage = PipelineStage::FOURIER_TRANSFORMED_SPEC;
  } else if (spench_task->m_stage == PipelineStage::PREDICTED_MASK) {
    m_feature_extractor_ptr->calcEnhancedPartsInPlace(spench_context->prev_features.front(),
                                                      spench_data.new_spectrogram);

    uint32_t feat_num = spench_context->prev_spectrogram.real.size();
    if (feat_num == 0) {
      spench_context->prev_spectrogram = spench_data.new_spectrogram;
      spench_task->m_stage = PipelineStage::SKIPPED_CHUNK;
      return;
    }
    ComplexNumber<float> concatenatedSpec(feat_num * 2, feat_num * 2);

    auto previous_frame = std::slice(0, feat_num, 1);
    concatenatedSpec.real[previous_frame] = spench_context->prev_spectrogram.real;
    concatenatedSpec.imag[previous_frame] = spench_context->prev_spectrogram.imag;

    auto current_frame = std::slice(feat_num, feat_num, 1);
    concatenatedSpec.real[current_frame] = spench_data.new_spectrogram.real;
    concatenatedSpec.imag[current_frame] = spench_data.new_spectrogram.imag;

    m_feature_extractor_ptr->iSTFTApply(concatenatedSpec,
                                spench_data.enhanced_wav);
    spench_context->prev_spectrogram = spench_data.new_spectrogram;
    spench_task->m_stage = PipelineStage::ENHANCED_CHUNK;
  } else if (spench_task->m_stage == PipelineStage::PREDICTED_SPEC) {
    uint32_t feat_num = spench_context->prev_spectrogram.real.size();
    if (feat_num == 0) {
      spench_context->prev_spectrogram = spench_data.new_spectrogram;
      spench_task->m_stage = PipelineStage::SKIPPED_CHUNK;
      return;
    }

    ComplexNumber<float> concatenatedSpec(feat_num * 2, feat_num * 2);

    auto previous_frame = std::slice(0, feat_num, 1);
    concatenatedSpec.real[previous_frame] = spench_context->prev_spectrogram.real;
    concatenatedSpec.imag[previous_frame] = spench_context->prev_spectrogram.imag;

    auto current_frame = std::slice(feat_num, feat_num, 1);
    concatenatedSpec.real[current_frame] = spench_data.new_spectrogram.real;
    concatenatedSpec.imag[current_frame] = spench_data.new_spectrogram.imag;

    m_feature_extractor_ptr->iSTFTApply(concatenatedSpec,
                                spench_data.enhanced_wav);
    spench_context->prev_spectrogram = spench_data.new_spectrogram;
    spench_task->m_stage = PipelineStage::ENHANCED_CHUNK;
  } else if (spench_task->m_stage == PipelineStage::SKIPPED_CHUNK) {
    // network pass the input to output and nothing to do
    spench_data.enhanced_wav.resize(m_params.NumFrameStrideSamples());
  } else {
    throw("State is incorrect!");
  }
}

void FeatureExtractorRunner::WarmUp() {
  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_data.waveform.resize(GetFrameSizeSamples(), 0.0);
  Run(&denoise_task);
}
}  // namespace spench
