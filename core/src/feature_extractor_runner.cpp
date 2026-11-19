#include "src/feature_extractor_runner.h"

#include <src/config_reader.h>

#include <algorithm>
#include <cassert>
#include <chrono>  // NOLINT
#include <cmath>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace gigaedge::enhance {

uint64_t FeatureExtractorRunner::get_frame_size_ms() const { return params_.frame_size_ms; }

uint64_t FeatureExtractorRunner::get_sampling_freq() const { return params_.sampling_freq; }

uint64_t FeatureExtractorRunner::get_frame_size_samples() const { return params_.num_frame_size_samples(); }

uint64_t FeatureExtractorRunner::get_frame_hop_size_samples() const { return params_.num_frame_stride_samples(); }

uint64_t FeatureExtractorRunner::get_pow_spec_feat_size_samples() const { return params_.pow_spec_feat_sz(); }

void FeatureExtractorRunner::read_config(std::shared_ptr<gigaedge::enhance::NetworkConfig> config) {
  params_.read_feature_params_node(config);
  model_output_is_spectrum_ =
      config->params()["inference"]["model_type"].as<std::string>("") == "denoiser_hybrid_web_model";
}

gigaedge::enhance::ComplexNumber<float> FeatureExtractorRunner::merge_input_spectrogram(
    const std::vector<gigaedge::enhance::ComplexNumber<float>>& frames) {
  return gigaedge::enhance::ComplexNumber<float>::concat_parts(frames);
}

void FeatureExtractorRunner::calc_enhanced_spectrogram(const gigaedge::enhance::ComplexNumber<float>& merged_spec,
                                                       gigaedge::enhance::ComplexNumber<float>& out_new_spec) {
  // Computes enhanced parts (masking, etc.) into out_new_spec.
  feature_extractor_ptr_->calc_enhanced_parts_in_place(merged_spec, out_new_spec);
}

uint32_t FeatureExtractorRunner::validate_and_get_feat_num(uint32_t input_chunk_num,
                                                           const gigaedge::enhance::ComplexNumber<float>& new_spec) {
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

bool FeatureExtractorRunner::has_prev_spectrogram(const gigaedge::enhance::Context<float>& ctx) {
  return ctx.prev_spectrogram.real.size() != 0;
}

gigaedge::enhance::ComplexNumber<float> FeatureExtractorRunner::build_concat_with_prev(
    const gigaedge::enhance::ComplexNumber<float>& prev_spec, const gigaedge::enhance::ComplexNumber<float>& new_spec,
    uint32_t input_chunk_num, uint32_t feat_num) {
  if (prev_spec.real.size() != feat_num || prev_spec.imag.size() != feat_num) {
    throw std::runtime_error("BuildConcatWithPrev: prev_spec size mismatch");
  }
  if (new_spec.real.size() != feat_num * input_chunk_num || new_spec.imag.size() != feat_num * input_chunk_num) {
    throw std::runtime_error("BuildConcatWithPrev: new_spec size mismatch");
  }

  return gigaedge::enhance::ComplexNumber<float>::concat_with_head(prev_spec, new_spec);
}

void FeatureExtractorRunner::update_prev_from_new_spec(gigaedge::enhance::Context<float>& ctx,
                                                       const gigaedge::enhance::ComplexNumber<float>& new_spec,
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

void FeatureExtractorRunner::fill_warming_up_output(gigaedge::enhance::Task* task) {
  task->data.enhanced_wav.resize(params_.num_frame_stride_samples());
  task->state.advance_to(PipelineStage::enhanced_ready);
}

void FeatureExtractorRunner::handle_model_output(gigaedge::enhance::Task* task) {
  if (task->state.is(ChunkOutcome::warming_up)) {
    fill_warming_up_output(task);
    return;
  }

  auto& d = task->data;
  auto& ctx = *d.context;

  // Hybrid models already return the enhanced spectrum. Other models return a mask.
  if (!model_output_is_spectrum_) {
    const auto merged_spec = merge_input_spectrogram(d.input_spectrogram);
    calc_enhanced_spectrogram(merged_spec, d.new_spectrogram);
  }

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
      task->state.set_outcome(ChunkOutcome::warming_up);
      fill_warming_up_output(task);
      return;
    }

    // We can output N frames (but the overlap-add pipeline needs one prev for the next call).
    // new_spectrogram already contains N sequential frames, so we can iSTFT it directly.
    feature_extractor_ptr_->istft_apply(d.new_spectrogram, d.enhanced_wav);

    update_prev_from_new_spec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
    task->state.advance_to(PipelineStage::enhanced_ready);
    return;
  }

  // ---- Case 2: we have previous spectrogram ----
  const auto concat = build_concat_with_prev(ctx.prev_spectrogram, d.new_spectrogram, input_chunk_num, feat_num);

  feature_extractor_ptr_->istft_apply(concat, d.enhanced_wav);

  update_prev_from_new_spec(ctx, d.new_spectrogram, input_chunk_num, feat_num);
  task->state.advance_to(PipelineStage::enhanced_ready);
}

void FeatureExtractorRunner::handle_waveform(gigaedge::enhance::Task* task) {
  auto& enhance_data = task->data;

  if (enhance_data.waveform.empty()) {
    throw std::runtime_error("Input signal is empty!");
  }

  feature_extractor_ptr_->stft_apply(enhance_data.waveform, enhance_data.input_spectrogram);

  if (enhance_data.input_spectrogram.empty()) {
    throw std::runtime_error("Spectrogram is calculated incorrectly! Something with input data!");
  }

  task->state.advance_to(PipelineStage::input_spectrum_ready);
}

void FeatureExtractorRunner::run(gigaedge::enhance::Task* task) {
  if (!task) {
    throw std::invalid_argument("FeatureExtractorRunner::run: task is null");
  }

  switch (task->state.stage()) {
    case PipelineStage::waveform_ready:
      handle_waveform(task);
      return;
    case PipelineStage::model_output_ready:
      handle_model_output(task);
      return;
    case PipelineStage::input_spectrum_ready:
    case PipelineStage::enhanced_ready:
      break;
  }

  throw std::runtime_error(std::string("FeatureExtractorRunner::run: unexpected stage '") +
                           to_string(task->state.stage()) + "'");
}

void FeatureExtractorRunner::warm_up() {
  gigaedge::enhance::Task denoise_task;
  std::unique_ptr<gigaedge::enhance::Context<float>> ctx(new gigaedge::enhance::Context<float>);
  denoise_task.data.context = ctx.get();
  denoise_task.data.waveform.resize(get_frame_size_samples(), 0.0);
  run(&denoise_task);
}
}  // namespace gigaedge::enhance
