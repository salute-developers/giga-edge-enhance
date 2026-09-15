#include "src/feature_extractor_runner.h"

#include <src/config_reader.h>

#include <algorithm>
#include <cassert>
#include <chrono>  // NOLINT
#include <cmath>
#include <deque>
#include <memory>
#include <vector>

namespace gigaedge::denoiser {

uint64_t FeatureExtractorRunner::get_frame_size_ms() const { return params_.frame_size_ms; }

uint64_t FeatureExtractorRunner::get_sampling_freq() const { return params_.sampling_freq; }

uint64_t FeatureExtractorRunner::get_frame_size_samples() const { return params_.num_frame_size_samples(); }

uint64_t FeatureExtractorRunner::get_frame_hop_size_samples() const { return params_.num_frame_stride_samples(); }

uint64_t FeatureExtractorRunner::get_pow_spec_feat_size_samples() const { return params_.pow_spec_feat_sz(); }

void FeatureExtractorRunner::read_config(std::shared_ptr<gigaedge::denoiser::NetworkConfig> config) {
  params_.read_feature_params_node(config);
}

gigaedge::denoiser::ComplexNumber<float> FeatureExtractorRunner::merge_input_spectrogram(
    const std::vector<gigaedge::denoiser::ComplexNumber<float>>& frames) {
  return gigaedge::denoiser::ComplexNumber<float>::concat_parts(frames);
}

void FeatureExtractorRunner::calc_enhanced_spectrogram(const gigaedge::denoiser::ComplexNumber<float>& merged_spec,
                                                       gigaedge::denoiser::ComplexNumber<float>& out_new_spec) {
  // Computes enhanced parts (masking, etc.) into out_new_spec.
  feature_extractor_ptr_->calc_enhanced_parts_in_place(merged_spec, out_new_spec);
}

uint32_t FeatureExtractorRunner::validate_and_get_feat_num(uint32_t input_chunk_num,
                                                           const gigaedge::denoiser::ComplexNumber<float>& new_spec) {
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

bool FeatureExtractorRunner::has_prev_spectrogram(const gigaedge::denoiser::Context<float>& ctx) {
  return ctx.prev_spectrogram.real.size() != 0;
}

gigaedge::denoiser::ComplexNumber<float> FeatureExtractorRunner::build_concat_with_prev(
    const gigaedge::denoiser::ComplexNumber<float>& prev_spec, const gigaedge::denoiser::ComplexNumber<float>& new_spec,
    uint32_t input_chunk_num, uint32_t feat_num) {
  if (prev_spec.real.size() != feat_num || prev_spec.imag.size() != feat_num) {
    throw std::runtime_error("BuildConcatWithPrev: prev_spec size mismatch");
  }
  if (new_spec.real.size() != feat_num * input_chunk_num || new_spec.imag.size() != feat_num * input_chunk_num) {
    throw std::runtime_error("BuildConcatWithPrev: new_spec size mismatch");
  }

  return gigaedge::denoiser::ComplexNumber<float>::concat_with_head(prev_spec, new_spec);
}

void FeatureExtractorRunner::update_prev_from_new_spec(gigaedge::denoiser::Context<float>& ctx,
                                                       const gigaedge::denoiser::ComplexNumber<float>& new_spec,
                                                       uint32_t input_chunk_num, uint32_t feat_num) {
  // Always keep the last "new" frame as prev for the next iteration.
  if (input_chunk_num == 0) {
    throw std::runtime_error("UpdatePrevFromNewSpec: input_chunk_num == 0");
  }

  if (ctx.prev_spectrogram.real.size() != feat_num || ctx.prev_spectrogram.imag.size() != feat_num) {
    ctx.prev_spectrogram.resize(feat_num);
  }

  new_spec.copy_frame_to(ctx.prev_spectrogram, feat_num, input_chunk_num - 1);
}

void FeatureExtractorRunner::handle_predicted_mask(gigaedge::denoiser::Task* task) {
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
      if (ctx.prev_spectrogram.real.size() != feat_num || ctx.prev_spectrogram.imag.size() != feat_num) {
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
  const auto concat = build_concat_with_prev(ctx.prev_spectrogram, d.new_spectrogram, input_chunk_num, feat_num);

  feature_extractor_ptr_->istft_apply(concat, d.enhanced_wav);

  update_prev_from_new_spec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
  task->stage = PipelineStage::enhanced_chunk;
}

void FeatureExtractorRunner::run(gigaedge::denoiser::Task* task) {
  if (!task) {
    throw std::invalid_argument("FeatureExtractorRunner::run: task is null");
  }
  auto& gigadenoiser_data = task->data;
  auto& context = gigadenoiser_data.context;

  if (task->stage == PipelineStage::orig_chunk) {
    if (gigadenoiser_data.waveform.size() == 0) {
      // error
      throw std::runtime_error("Input signal is empty!");
    }

    feature_extractor_ptr_->stft_apply(gigadenoiser_data.waveform, gigadenoiser_data.input_spectrogram);

    if (gigadenoiser_data.input_spectrogram.size() < 1) {
      // error
      throw std::runtime_error("Spectrogram is calculated incorectly! Something with input data!");
    }

    task->stage = PipelineStage::fourier_transformed_spec;
  } else if (task->stage == PipelineStage::predicted_mask) {
    handle_predicted_mask(task);
    return;
  } else if (task->stage == PipelineStage::predicted_spec) {
    uint32_t feat_num = context->prev_spectrogram.real.size();
    if (feat_num == 0) {
      context->prev_spectrogram = gigadenoiser_data.new_spectrogram;
      task->stage = PipelineStage::skipped_chunk;
      return;
    }

    ComplexNumber<float> concatenated_spec(feat_num * 2, feat_num * 2);

    auto previous_frame = std::slice(0, feat_num, 1);
    concatenated_spec.real[previous_frame] = context->prev_spectrogram.real;
    concatenated_spec.imag[previous_frame] = context->prev_spectrogram.imag;

    auto current_frame = std::slice(feat_num, feat_num, 1);
    concatenated_spec.real[current_frame] = gigadenoiser_data.new_spectrogram.real;
    concatenated_spec.imag[current_frame] = gigadenoiser_data.new_spectrogram.imag;

    feature_extractor_ptr_->istft_apply(concatenated_spec, gigadenoiser_data.enhanced_wav);
    context->prev_spectrogram = gigadenoiser_data.new_spectrogram;
    task->stage = PipelineStage::enhanced_chunk;
  } else if (task->stage == PipelineStage::skipped_chunk) {
    // network pass the input to output and nothing to do
    gigadenoiser_data.enhanced_wav.resize(params_.num_frame_stride_samples());
  } else {
    throw std::runtime_error("State is incorrect!");
  }
}

void FeatureExtractorRunner::warm_up() {
  gigaedge::denoiser::Task denoise_task;
  std::unique_ptr<gigaedge::denoiser::Context<float>> ctx(new gigaedge::denoiser::Context<float>);
  denoise_task.data.context = ctx.get();
  denoise_task.data.waveform.resize(get_frame_size_samples(), 0.0);
  run(&denoise_task);
}
}  // namespace gigaedge::denoiser
