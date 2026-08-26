#include "util/onnx/denoise_onnx_runner.h"

#include <algorithm>
#include <stdexcept>

namespace util {
namespace onnx {

DenoiserQOnnxRunner::DenoiserQOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                         uint32_t inference_engine_threads, bool on_gpu) {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  load_model_io_metadata();
  warm_up<float>();
}

void DenoiserQOnnxRunner::prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                                                 gigaedge::denoiser::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  gigaedge::denoiser::DenoiseContextQ<float>* denoise_context =
      dynamic_cast<gigaedge::denoiser::DenoiseContextQ<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserQOnnxRunner");
  }
  inputs.clear();

  const uint32_t num_feats = static_cast<uint32_t>(model_input_shapes_[0][2]);
  if (mag.size() < num_feats) {
    throw std::runtime_error("mag size is smaller than expected");
  }
  if (num_feats == 0) {
    throw std::runtime_error("Invalid model input shape: num_feats is zero");
  }
  if (mag.size() % num_feats != 0) {
    throw std::runtime_error("mag size is not divisible by num_feats");
  }
  const uint32_t num_frames = static_cast<uint32_t>(mag.size() / num_feats);

  std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], num_frames);

  resize_if_needed_in_place(denoise_context->noisy_mag, resolved_input_shapes[0]);  // noisy_mag_in [1,1,769,T]
  resize_if_needed_in_place(denoise_context->fullband_hidden, resolved_input_shapes[1]);
  resize_if_needed_in_place(denoise_context->fb_snr_noise_psd, resolved_input_shapes[2]);
  resize_if_needed_in_place(denoise_context->fb_snr_noise_pg, resolved_input_shapes[3]);
  resize_if_needed_in_place(denoise_context->sb_snr_noise_psd, resolved_input_shapes[4]);
  resize_if_needed_in_place(denoise_context->sb_snr_noise_pg, resolved_input_shapes[5]);
  resize_if_needed_in_place(denoise_context->fb_conv1_buf, resolved_input_shapes[6]);
  resize_if_needed_in_place(denoise_context->fb_conv3_buf, resolved_input_shapes[7]);

  std::copy(mag.begin(), mag.end(), denoise_context->noisy_mag.begin());

  inputs.push_back(&denoise_context->noisy_mag);
  inputs.push_back(&denoise_context->fullband_hidden);
  inputs.push_back(&denoise_context->fb_snr_noise_psd);
  inputs.push_back(&denoise_context->fb_snr_noise_pg);
  inputs.push_back(&denoise_context->sb_snr_noise_psd);
  inputs.push_back(&denoise_context->sb_snr_noise_pg);
  inputs.push_back(&denoise_context->fb_conv1_buf);
  inputs.push_back(&denoise_context->fb_conv3_buf);
}

void DenoiserQOnnxRunner::init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t out_shape) {
  resize_if_needed_in_place(data.new_spectrogram.real, {out_shape}, 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, {out_shape}, 2);
}

void DenoiserQOnnxRunner::infer(gigaedge::denoiser::Task* denoise_task) {
  if (denoise_task == nullptr || denoise_task->data.context == nullptr) {
    throw std::runtime_error("Task data or context are not initialized!");
  }
  if (denoise_task->data.input_spectrogram.size() == 0) {
    throw std::runtime_error("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() == model_params_.network_input_ctx_len + 1) {
    denoise_context->prev_features.pop();
  }

  denoise_task->state.require(gigaedge::denoiser::PipelineStage::input_spectrum_ready);

  std::vector<std::vector<float>*> inputs;
  std::vector<float> mag;
  const auto& specs = denoise_data.input_spectrogram;
  const std::size_t num_feats = specs.front().size();
  const std::size_t num_frames = specs.size();

  mag.resize(num_feats * num_frames);
  for (std::size_t t = 0; t < num_frames; ++t) {
    specs[t].write_magnitude_interleaved(mag, t, num_frames);
  }

  if (mag.size() % num_feats != 0) {
    throw std::runtime_error("mag size is not divisible by num_feats");
  }

  std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], num_frames);
  prepare_input_in_place(mag, inputs, denoise_context);
  init_output_tensors(denoise_data, num_feats * num_frames * 2);
  std::vector<Ort::Value> outputs;
  run(inputs, outputs, resolved_input_shapes);
  parse_results<float>(outputs, denoise_data);

  if (denoise_context->prev_features.size() < model_params_.network_input_ctx_len) {
    denoise_task->state.set_outcome(gigaedge::denoiser::ChunkOutcome::warming_up);
  }
  denoise_task->state.advance_to(gigaedge::denoiser::PipelineStage::model_output_ready);
  denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);
}

DenoiserFOnnxRunner::DenoiserFOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                         uint32_t inference_engine_threads, bool on_gpu) {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  load_model_io_metadata();
  warm_up<float>();
}

void DenoiserFOnnxRunner::init_output_tensors(gigaedge::denoiser::TaskData& data) {
  // other outputs were resized in prepare_input_in_place
  resize_if_needed_in_place(data.new_spectrogram.real, model_output_shapes_[3], 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, model_output_shapes_[3], 2);
}

void DenoiserFOnnxRunner::prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                                                 gigaedge::denoiser::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  gigaedge::denoiser::DenoiseContextF<float>* denoise_context =
      dynamic_cast<gigaedge::denoiser::DenoiseContextF<float>*>(context);
  resize_if_needed_in_place(denoise_context->noisy_mag, model_input_shapes_[0]);            // noisy_mag_5
  resize_if_needed_in_place(denoise_context->fullband_hidden, model_input_shapes_[1]);      // fb_h
  resize_if_needed_in_place(denoise_context->fullband_cell_state, model_input_shapes_[2]);  // fb_c
  resize_if_needed_in_place(denoise_context->prev_fb_out, model_input_shapes_[3]);          // new_fb_out_5

  // forget 1 frame out of num_frames (5) and move remaining to the left
  uint32_t from, to;
  uint32_t num_feats = model_input_shapes_[0][1];
  uint32_t num_frames = model_input_shapes_[0][2];
  for (uint32_t frame_i = 1; frame_i < num_frames; ++frame_i) {
    for (uint32_t feat_i = 0; feat_i < num_feats; ++feat_i) {
      from = frame_i + feat_i * num_frames;
      to = (frame_i - 1) + feat_i * num_frames;
      denoise_context->noisy_mag[to] = denoise_context->noisy_mag[from];
    }
  }

  // add new noisy_mag to the input
  for (uint32_t feat_i = 0; feat_i < num_feats; ++feat_i) {
    to = num_frames - 1 + feat_i * num_frames;
    denoise_context->noisy_mag[to] = mag[feat_i];
  }

  inputs.push_back(&denoise_context->noisy_mag);
  inputs.push_back(&denoise_context->fullband_hidden);
  inputs.push_back(&denoise_context->fullband_cell_state);
  inputs.push_back(&denoise_context->prev_fb_out);
}

void DenoiserFOnnxRunner::infer(gigaedge::denoiser::Task* denoise_task) {
  if (denoise_task == nullptr || denoise_task->data.context == nullptr) {
    throw std::runtime_error("Task data or context are not initialized!");
  }
  if (denoise_task->data.input_spectrogram.size() == 0) {
    throw std::runtime_error("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len) {
    denoise_context->prev_features.pop();
  }
  denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);

  denoise_task->state.require(gigaedge::denoiser::PipelineStage::input_spectrum_ready);

  std::vector<std::vector<float>*> inputs;
  std::vector<float> mag = denoise_data.input_spectrogram[0].get_magnitude();
  DenoiserFOnnxRunner::prepare_input_in_place(mag, inputs, denoise_context);
  init_output_tensors(denoise_data);
  std::vector<Ort::Value> outputs;
  run(inputs, outputs);
  parse_results<float>(outputs, denoise_data);

  if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len + 1) {
    throw std::runtime_error("Incorrect number of previous states!");
  }
  denoise_task->state.advance_to(gigaedge::denoiser::PipelineStage::model_output_ready);
}

DenoiserSFOnnxRunner::DenoiserSFOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                           uint32_t inference_engine_threads, bool on_gpu) {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  load_model_io_metadata();
  warm_up<float>();
}

void DenoiserSFOnnxRunner::init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t out_shape) {
  resize_if_needed_in_place(data.new_spectrogram.real, {out_shape}, 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, {out_shape}, 2);
}

void DenoiserSFOnnxRunner::prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                                                  gigaedge::denoiser::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }

  gigaedge::denoiser::DenoiseContextSF<float>* denoise_context =
      dynamic_cast<gigaedge::denoiser::DenoiseContextSF<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserSFOnnxRunner");
  }

  inputs.clear();
  const uint32_t num_feats = static_cast<uint32_t>(model_input_shapes_[0][1]);

  if (mag.size() < num_feats) {
    throw std::runtime_error("mag size is smaller than expected");
  }
  if (num_feats == 0) {
    throw std::runtime_error("Invalid model input shape: num_feats is zero");
  }
  if (mag.size() % num_feats != 0) {
    throw std::runtime_error("mag size is not divisible by num_feats");
  }

  const uint32_t num_frames = static_cast<uint32_t>(mag.size() / num_feats);

  std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], num_frames);

  resize_if_needed_in_place(denoise_context->noisy_mag, resolved_input_shapes[0]);
  resize_if_needed_in_place(denoise_context->fullband_hidden_state, resolved_input_shapes[1]);
  resize_if_needed_in_place(denoise_context->fullband_cell_state, resolved_input_shapes[2]);
  resize_if_needed_in_place(denoise_context->fb_conv1_buf, resolved_input_shapes[3]);
  resize_if_needed_in_place(denoise_context->fb_conv3_buf, resolved_input_shapes[4]);

  std::copy(mag.begin(), mag.end(), denoise_context->noisy_mag.begin());

  inputs.push_back(&denoise_context->noisy_mag);
  inputs.push_back(&denoise_context->fullband_hidden_state);
  inputs.push_back(&denoise_context->fullband_cell_state);
  inputs.push_back(&denoise_context->fb_conv1_buf);
  inputs.push_back(&denoise_context->fb_conv3_buf);
}

void DenoiserSFOnnxRunner::infer(gigaedge::denoiser::Task* denoise_task) {
  if (denoise_task == nullptr || denoise_task->data.context == nullptr) {
    throw std::runtime_error("Task data or context are not initialized!");
  }
  if (denoise_task->data.input_spectrogram.empty()) {
    throw std::runtime_error("Input spectrogram should be calculated before this step!");
  }

  auto& denoise_data = denoise_task->data;
  auto& denoise_context = denoise_data.context;

  if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len) {
    denoise_context->prev_features.pop();
  }
  denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);

  denoise_task->state.require(gigaedge::denoiser::PipelineStage::input_spectrum_ready);

  std::vector<std::vector<float>*> inputs;
  std::vector<float> mag;
  const auto& specs = denoise_data.input_spectrogram;
  const std::size_t num_feats = specs.front().size();
  const std::size_t num_frames = specs.size();

  mag.resize(num_feats * num_frames);
  for (std::size_t t = 0; t < num_frames; ++t) {
    specs[t].write_magnitude_interleaved(mag, t, num_frames);
  }
  if (mag.size() % num_feats != 0) {
    throw std::runtime_error("mag size is not divisible by num_feats");
  }

  std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], num_frames);
  prepare_input_in_place(mag, inputs, denoise_context);
  init_output_tensors(denoise_data, static_cast<uint32_t>(num_feats * num_frames * 2));
  std::vector<Ort::Value> outputs;
  run(inputs, outputs, resolved_input_shapes);
  parse_results<float>(outputs, denoise_data);

  if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len + 1) {
    throw std::runtime_error("Incorrect number of previous states!");
  }
  denoise_task->state.advance_to(gigaedge::denoiser::PipelineStage::model_output_ready);
}
}  // namespace onnx
}  // namespace util
