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

uint64_t FeatureExtractorRunner::get_frame_size_ms() const {
  return params_.frame_size_ms;
}

uint64_t FeatureExtractorRunner::get_sampling_freq() const {
  return params_.sampling_freq;
}

uint64_t FeatureExtractorRunner::get_frame_size_samples() const {
  return params_.num_frame_size_samples();
}

uint64_t FeatureExtractorRunner::get_frame_hop_size_samples() const {
  return params_.num_frame_stride_samples();
}

uint64_t FeatureExtractorRunner::get_pow_spec_feat_size_samples() const {
  return params_.pow_spec_feat_sz();
}

void FeatureExtractorRunner::read_config(std::shared_ptr<spench::NetworkConfig> config) {
  params_.read_feature_params_node(config);
}

spench::ComplexNumber<float>
FeatureExtractorRunner::merge_input_spectrogram(const std::vector<spench::ComplexNumber<float>>& frames) {
  return spench::ComplexNumber<float>::concat_parts(frames);
}

void FeatureExtractorRunner::calc_enhanced_spectrogram(const spench::ComplexNumber<float>& merged_spec,
                                                     spench::ComplexNumber<float>& out_new_spec) {
  // Computes enhanced parts (masking, etc.) into out_new_spec.
  feature_extractor_ptr_->calc_enhanced_parts_in_place(merged_spec, out_new_spec);
}

uint32_t FeatureExtractorRunner::validate_and_get_feat_num(uint32_t input_chunk_num,
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

bool FeatureExtractorRunner::has_prev_spectrogram(const spench::SpenchContext<float>& ctx) {
  return ctx.prev_spectrogram.real.size() != 0;
}

spench::ComplexNumber<float>
FeatureExtractorRunner::build_concat_with_prev(const spench::ComplexNumber<float>& prev_spec,
                                            const spench::ComplexNumber<float>& new_spec,
                                            uint32_t input_chunk_num,
                                            uint32_t feat_num) {
  if (prev_spec.real.size() != feat_num || prev_spec.imag.size() != feat_num) {
    throw std::runtime_error("BuildConcatWithPrev: prev_spec size mismatch");
  }
  if (new_spec.real.size() != feat_num * input_chunk_num ||
      new_spec.imag.size() != feat_num * input_chunk_num) {
    throw std::runtime_error("BuildConcatWithPrev: new_spec size mismatch");
  }

  return spench::ComplexNumber<float>::concat_with_head(prev_spec, new_spec);
}

void FeatureExtractorRunner::update_prev_from_new_spec(spench::SpenchContext<float>& ctx,
                                                   const spench::ComplexNumber<float>& new_spec,
                                                   uint32_t input_chunk_num,
                                                   uint32_t feat_num) {
  // Always keep the last "new" frame as prev for the next iteration.
  if (input_chunk_num == 0) {
    throw std::runtime_error("UpdatePrevFromNewSpec: input_chunk_num == 0");
  }

  if (ctx.prev_spectrogram.real.size() != feat_num ||
      ctx.prev_spectrogram.imag.size() != feat_num) {
    ctx.prev_spectrogram.resize(feat_num);
  }

  new_spec.copy_frame_to(ctx.prev_spectrogram, feat_num, input_chunk_num - 1);
}

void FeatureExtractorRunner::handle_predicted_mask(spench::SpenchTask* task) {
  auto& d = task->data;
  auto& ctx = *d.context;

  // 1) Merge input frames into a single spectrogram.
  const auto merged_spec = merge_input_spectrogram(d.input_spectrogram);

  // 2) Apply enhancement/mask to build new_spectrogram.
  calc_enhanced_spectrogram(merged_spec, d.new_spectrogram);

  // 3) Validate and compute feat_num.
  const uint32_t input_chunk_num = static_cast<uint32_t>(d.input_spectrogram.size());
  const uint32_t feat_num = validate_and_get_feat_num(input_chunk_num, d.new_spectrogram);

  // ---- Case 1: no previous spectrogram yet ----
  if (!has_prev_spectrogram(ctx)) {
    if (input_chunk_num == 1) {
      // First ever chunk: store it as prev, nothing to output yet.
      if (ctx.prev_spectrogram.real.size() != feat_num ||
        ctx.prev_spectrogram.imag.size() != feat_num) {
        ctx.prev_spectrogram.resize(feat_num);
      }

      d.new_spectrogram.copy_frame_to(ctx.prev_spectrogram, feat_num, 0);
      task->stage = PipelineStage::skipped_chunk;
      return;
    }

    // We can output N frames (but the overlap-add pipeline needs one prev for the next call).
    // new_spectrogram already contains N sequential frames, so we can iSTFT it directly.
    feature_extractor_ptr_->istft_apply(d.new_spectrogram, d.enhanced_wav);

    update_prev_from_new_spec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
    task->stage = PipelineStage::enhanced_chunk;
    return;
  }

  // ---- Case 2: we have previous spectrogram ----
  const auto concat = build_concat_with_prev(ctx.prev_spectrogram,
                                         d.new_spectrogram,
                                         input_chunk_num,
                                         feat_num);

  feature_extractor_ptr_->istft_apply(concat, d.enhanced_wav);

  update_prev_from_new_spec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
  task->stage = PipelineStage::enhanced_chunk;
}

void FeatureExtractorRunner::run(spench::SpenchTask* spench_task) {
  if (!spench_task) {
    throw std::invalid_argument("FeatureExtractorRunner::run: spench_task is null");
  }
  auto& spench_data = spench_task->data;
  auto& spench_context = spench_data.context;

  if (spench_task->stage == PipelineStage::orig_chunk) {
    if (spench_data.waveform.size() == 0) {
      // error
      throw std::runtime_error("Input signal is empty!");
    }

    feature_extractor_ptr_->stft_apply(spench_data.waveform,
                                       spench_data.input_spectrogram);

    if (spench_data.input_spectrogram.size() < 1) {
      // error
      throw std::runtime_error("Spectrogram is calculated incorectly! Something with input data!");
    }

    spench_task->stage = PipelineStage::fourier_transformed_spec;
  } else if (spench_task->stage == PipelineStage::predicted_mask) {
    handle_predicted_mask(spench_task);
    return;
  } else if (spench_task->stage == PipelineStage::predicted_spec) {
    uint32_t feat_num = spench_context->prev_spectrogram.real.size();
    if (feat_num == 0) {
      spench_context->prev_spectrogram = spench_data.new_spectrogram;
      spench_task->stage = PipelineStage::skipped_chunk;
      return;
    }

    ComplexNumber<float> concatenated_spec(feat_num * 2, feat_num * 2);

    auto previous_frame = std::slice(0, feat_num, 1);
    concatenated_spec.real[previous_frame] = spench_context->prev_spectrogram.real;
    concatenated_spec.imag[previous_frame] = spench_context->prev_spectrogram.imag;

    auto current_frame = std::slice(feat_num, feat_num, 1);
    concatenated_spec.real[current_frame] = spench_data.new_spectrogram.real;
    concatenated_spec.imag[current_frame] = spench_data.new_spectrogram.imag;

    feature_extractor_ptr_->istft_apply(concatenated_spec,
                                spench_data.enhanced_wav);
    spench_context->prev_spectrogram = spench_data.new_spectrogram;
    spench_task->stage = PipelineStage::enhanced_chunk;
  } else if (spench_task->stage == PipelineStage::skipped_chunk) {
    // network pass the input to output and nothing to do
    spench_data.enhanced_wav.resize(params_.num_frame_stride_samples());
  } else {
    throw std::runtime_error("State is incorrect!");
  }
}

void FeatureExtractorRunner::warm_up() {
  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.data.context = ctx.get();
  denoise_task.data.waveform.resize(get_frame_size_samples(), 0.0);
  run(&denoise_task);
}
}  // namespace spench
