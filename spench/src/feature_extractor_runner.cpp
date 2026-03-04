#include "spench/src/feature_extractor_runner.h"

#include <cmath>
#include <vector>
#include <deque>
#include <algorithm>
#include <chrono> // NOLINT
#include <cassert>
#include <memory>

#include <spench/src/spench_config_reader.h>

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

void FeatureExtractorRunner::ReadConfig(std::shared_ptr<spench::NetworkConfig> config) {
  m_params.ReadFeatureParamsNode(config);
}

spench::ComplexNumber<float>
FeatureExtractorRunner::MergeInputSpectrogram(const std::vector<spench::ComplexNumber<float>>& frames) {
  spench::ComplexNumber<float> merged;
  for (const auto& frame : frames) {
    merged.append(frame);
  }
  return merged;
}

void FeatureExtractorRunner::CalcEnhancedSpectrogram(const spench::ComplexNumber<float>& merged_spec,
                                                     spench::ComplexNumber<float>& out_new_spec) {
  // Computes enhanced parts (masking, etc.) into out_new_spec.
  m_feature_extractor_ptr->calcEnhancedPartsInPlace(merged_spec, out_new_spec);
}

uint32_t FeatureExtractorRunner::ValidateAndGetFeatNum(uint32_t input_chunk_num,
                                                       const spench::ComplexNumber<float>& new_spec) {
  if (input_chunk_num == 0) {
    throw std::runtime_error("ValidateAndGetFeatNum: input_chunk_num == 0");
  }

  const uint32_t real_size = static_cast<uint32_t>(new_spec.real.size());
  const uint32_t imag_size = static_cast<uint32_t>(new_spec.imag.size());

  if (real_size != imag_size) {
    throw std::runtime_error("ValidateAndGetFeatNum: real/imag size mismatch in new_spectrogram");
  }
  if (real_size == 0) {
    throw std::runtime_error("ValidateAndGetFeatNum: new_spectrogram is empty");
  }
  if (real_size % input_chunk_num != 0) {
    throw std::runtime_error("ValidateAndGetFeatNum: new_spectrogram size is not divisible by input_chunk_num");
  }

  return real_size / input_chunk_num;
}

bool FeatureExtractorRunner::HasPrevSpectrogram(const spench::SpenchContext<float>& ctx) {
  return ctx.prev_spectrogram.real.size() != 0;
}

spench::ComplexNumber<float>
FeatureExtractorRunner::ExtractFrame(const spench::ComplexNumber<float>& spec,
                                     uint32_t feat_num,
                                     uint32_t frame_idx) {
  spench::ComplexNumber<float> out(feat_num, feat_num);
  const std::slice s(frame_idx * feat_num, feat_num, 1);
  out.real = spec.real[s];
  out.imag = spec.imag[s];
  return out;
}

spench::ComplexNumber<float>
FeatureExtractorRunner::BuildConcatWithPrev(const spench::ComplexNumber<float>& prev_spec,
                                            const spench::ComplexNumber<float>& new_spec,
                                            uint32_t input_chunk_num,
                                            uint32_t feat_num) {
  // Layout:
  //   concat frame 0   = prev_spec
  //   concat frames 1..N = new_spec frames 0..N-1 (already sequential)
  spench::ComplexNumber<float> concat(feat_num * (input_chunk_num + 1),
                                      feat_num * (input_chunk_num + 1));

  // Copy prev into first frame.
  const std::slice dst_prev(0, feat_num, 1);
  concat.real[dst_prev] = prev_spec.real;
  concat.imag[dst_prev] = prev_spec.imag;

  // Copy entire new_spec into the tail in one shot (contiguous layout).
  const std::slice dst_tail(feat_num, feat_num * input_chunk_num, 1);
  concat.real[dst_tail] = new_spec.real;
  concat.imag[dst_tail] = new_spec.imag;

  return concat;
}

void FeatureExtractorRunner::UpdatePrevFromNewSpec(spench::SpenchContext<float>& ctx,
                                                   const spench::ComplexNumber<float>& new_spec,
                                                   uint32_t input_chunk_num,
                                                   uint32_t feat_num) {
  // Always keep the last "new" frame as prev for the next iteration.
  ctx.prev_spectrogram = ExtractFrame(new_spec, feat_num, input_chunk_num - 1);
}

void FeatureExtractorRunner::HandlePredictedMask(spench::SpenchTask* task) {
  auto& d = task->m_data;
  auto& ctx = *d.context;

  // 1) Merge input frames into a single spectrogram.
  const auto merged_spec = MergeInputSpectrogram(d.input_spectrogram);

  // 2) Apply enhancement/mask to build new_spectrogram.
  CalcEnhancedSpectrogram(merged_spec, d.new_spectrogram);

  // 3) Validate and compute feat_num.
  const uint32_t input_chunk_num = static_cast<uint32_t>(d.input_spectrogram.size());
  const uint32_t feat_num = ValidateAndGetFeatNum(input_chunk_num, d.new_spectrogram);

  // ---- Case 1: no previous spectrogram yet ----
  if (!HasPrevSpectrogram(ctx)) {
    if (input_chunk_num == 1) {
      // First ever chunk: store it as prev, nothing to output yet.
      ctx.prev_spectrogram = ExtractFrame(d.new_spectrogram, feat_num, 0);
      task->m_stage = PipelineStage::SKIPPED_CHUNK;
      return;
    }

    // We can output N frames (but the overlap-add pipeline needs one prev for the next call).
    // new_spectrogram already contains N sequential frames, so we can iSTFT it directly.
    m_feature_extractor_ptr->iSTFTApply(d.new_spectrogram, d.enhanced_wav);

    UpdatePrevFromNewSpec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
    task->m_stage = PipelineStage::ENHANCED_CHUNK;
    return;
  }

  // ---- Case 2: we have previous spectrogram ----
  const auto concat = BuildConcatWithPrev(ctx.prev_spectrogram,
                                         d.new_spectrogram,
                                         input_chunk_num,
                                         feat_num);

  m_feature_extractor_ptr->iSTFTApply(concat, d.enhanced_wav);

  UpdatePrevFromNewSpec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
  task->m_stage = PipelineStage::ENHANCED_CHUNK;
}

void FeatureExtractorRunner::Run(spench::SpenchTask* spench_task) {
  if (!spench_task) {
    throw std::invalid_argument("FeatureExtractorRunner::Run: spench_task is null");
  }
  auto& spench_data = spench_task->m_data;
  auto& spench_context = spench_data.context;

  if (spench_task->m_stage == PipelineStage::ORIG_CHUNK) {
    if (spench_data.waveform.size() == 0) {
      // error
      throw std::runtime_error("Input signal is empty!");
    }

    m_feature_extractor_ptr->STFTApply(spench_data.waveform,
                                       spench_data.input_spectrogram);

    if (spench_data.input_spectrogram.size() < 1) {
      // error
      throw std::runtime_error("Spectrogram is calculated incorectly! Something with input data!");
    }

    spench_task->m_stage = PipelineStage::FOURIER_TRANSFORMED_SPEC;
  } else if (spench_task->m_stage == PipelineStage::PREDICTED_MASK) {
    HandlePredictedMask(spench_task);
    return;
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
    throw std::runtime_error("State is incorrect!");
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
