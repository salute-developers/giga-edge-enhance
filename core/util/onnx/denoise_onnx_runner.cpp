#include "util/onnx/denoise_onnx_runner.h"

#include <algorithm>
#include <stdexcept>

namespace util {
namespace onnx {

DenoiserDiscriminativeHqOnnxRunner::DenoiserDiscriminativeHqOnnxRunner(
    const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config, uint32_t inference_engine_threads, bool on_gpu) {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  load_model_io_metadata();
  warm_up<float>();
}

void DenoiserDiscriminativeHqOnnxRunner::prepare_input_in_place(std::vector<float>& mag,
                                                                std::vector<std::vector<float>*>& inputs,
                                                                gigaedge::denoiser::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  gigaedge::denoiser::DenoiseContextDiscriminativeHq<float>* denoise_context =
      dynamic_cast<gigaedge::denoiser::DenoiseContextDiscriminativeHq<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserDiscriminativeHqOnnxRunner");
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

void DenoiserDiscriminativeHqOnnxRunner::init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t out_shape) {
  resize_if_needed_in_place(data.new_spectrogram.real, {out_shape}, 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, {out_shape}, 2);
}

void DenoiserDiscriminativeHqOnnxRunner::infer(gigaedge::denoiser::Task* denoise_task) {
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

DenoiserDiscriminativeNbOnnxRunner::DenoiserDiscriminativeNbOnnxRunner(
    const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config, uint32_t inference_engine_threads, bool on_gpu) {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  load_model_io_metadata();
  warm_up<float>();
}

void DenoiserDiscriminativeNbOnnxRunner::init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t out_shape) {
  resize_if_needed_in_place(data.new_spectrogram.real, {out_shape}, 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, {out_shape}, 2);
}

void DenoiserDiscriminativeNbOnnxRunner::prepare_input_in_place(std::vector<float>& mag,
                                                                std::vector<std::vector<float>*>& inputs,
                                                                gigaedge::denoiser::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }

  gigaedge::denoiser::DenoiseContextDiscriminativeNb<float>* denoise_context =
      dynamic_cast<gigaedge::denoiser::DenoiseContextDiscriminativeNb<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserDiscriminativeNbOnnxRunner");
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

void DenoiserDiscriminativeNbOnnxRunner::infer(gigaedge::denoiser::Task* denoise_task) {
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
DenoiserHybridOnnxRunner::DenoiserHybridOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                                   uint32_t inference_engine_threads, bool on_gpu) {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  load_model_io_metadata();
  if (model_input_shapes_.size() != 9 || model_output_shapes_.size() != 9 || model_input_shapes_[0].size() != 4 ||
      model_input_shapes_[0][0] != 1 || model_input_shapes_[0][1] <= 0 || model_input_shapes_[0][3] != 2 ||
      (time_axis_in_[0] != -1 && time_axis_in_[0] != 2)) {
    throw std::runtime_error("Invalid hybrid model: expected nine inputs/outputs and [1, F, T, 2] spectrum");
  }
  for (size_t i = 0; i < model_input_shapes_.size(); ++i) {
    if (session_ptr_->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetElementType() !=
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        session_ptr_->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetElementType() !=
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
      throw std::runtime_error("Hybrid models require FP32 tensors");
    }
  }
  for (size_t i = 1; i < model_input_shapes_.size(); ++i) {
    if (std::any_of(model_input_shapes_[i].begin(), model_input_shapes_[i].end(),
                    [](int64_t dimension) { return dimension <= 0; })) {
      throw std::runtime_error("Hybrid recurrent states must have fixed positive dimensions");
    }
  }
  warm_up<float>();
}

void DenoiserHybridOnnxRunner::prepare_input_in_place(std::vector<float>& noisy_ri,
                                                      std::vector<std::vector<float>*>& inputs,
                                                      gigaedge::denoiser::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }

  gigaedge::denoiser::DenoiseContextHybrid<float>* denoise_context =
      dynamic_cast<gigaedge::denoiser::DenoiseContextHybrid<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserHybridOnnxRunner");
  }
  inputs.clear();

  const uint32_t num_feats = static_cast<uint32_t>(model_input_shapes_[0][1]);
  if (num_feats == 0) {
    throw std::runtime_error("Invalid model input shape: num_feats is zero");
  }
  const size_t complex_frame_size = static_cast<size_t>(num_feats) * 2;
  if (noisy_ri.size() < complex_frame_size) {
    throw std::runtime_error("Noisy RI input size is smaller than expected");
  }
  if (noisy_ri.size() % complex_frame_size != 0) {
    throw std::runtime_error("Noisy RI input size is not divisible by num_feats * 2");
  }

  const uint32_t num_frames = static_cast<uint32_t>(noisy_ri.size() / complex_frame_size);
  std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], num_frames);

  denoise_context->noisy_ri.resize(noisy_ri.size());
  resize_if_needed_in_place(denoise_context->fullband_hidden, resolved_input_shapes[1]);
  resize_if_needed_in_place(denoise_context->fb_snr_noise_psd, resolved_input_shapes[2]);
  resize_if_needed_in_place(denoise_context->fb_snr_noise_pg, resolved_input_shapes[3]);
  resize_if_needed_in_place(denoise_context->sb_snr_noise_psd, resolved_input_shapes[4]);
  resize_if_needed_in_place(denoise_context->sb_snr_noise_pg, resolved_input_shapes[5]);
  resize_if_needed_in_place(denoise_context->fb_conv1_buf, resolved_input_shapes[6]);
  resize_if_needed_in_place(denoise_context->fb_conv3_buf, resolved_input_shapes[7]);
  resize_if_needed_in_place(denoise_context->stage2_hidden, resolved_input_shapes[8]);

  std::copy(noisy_ri.begin(), noisy_ri.end(), denoise_context->noisy_ri.begin());

  inputs.push_back(&denoise_context->noisy_ri);
  inputs.push_back(&denoise_context->fullband_hidden);
  inputs.push_back(&denoise_context->fb_snr_noise_psd);
  inputs.push_back(&denoise_context->fb_snr_noise_pg);
  inputs.push_back(&denoise_context->sb_snr_noise_psd);
  inputs.push_back(&denoise_context->sb_snr_noise_pg);
  inputs.push_back(&denoise_context->fb_conv1_buf);
  inputs.push_back(&denoise_context->fb_conv3_buf);
  inputs.push_back(&denoise_context->stage2_hidden);
}

void DenoiserHybridOnnxRunner::infer(gigaedge::denoiser::Task* denoise_task) {
  if (denoise_task == nullptr || denoise_task->data.context == nullptr) {
    throw std::runtime_error("Task data or context are not initialized!");
  }
  if (denoise_task->data.input_spectrogram.empty()) {
    throw std::runtime_error("Input spectrogram should be calculated before this step!");
  }

  auto& denoise_data = denoise_task->data;
  auto& denoise_context = denoise_data.context;
  const auto& specs = denoise_data.input_spectrogram;
  const size_t num_feats = specs.front().size();
  const size_t num_frames = specs.size();

  if (num_feats == 0 || specs.front().imag.size() != num_feats) {
    throw std::runtime_error("Input spectrogram has an invalid complex frame");
  }
  if (num_feats != static_cast<size_t>(model_input_shapes_[0][1])) {
    throw std::runtime_error("Input spectrogram feature count does not match the hybrid model");
  }
  for (const auto& spec : specs) {
    if (spec.size() != num_feats || spec.imag.size() != num_feats) {
      throw std::runtime_error("Input spectrogram frames have different sizes");
    }
  }

  if (denoise_context->prev_features.size() == model_params_.network_input_ctx_len + 1) {
    denoise_context->prev_features.pop();
  }

  denoise_task->state.require(gigaedge::denoiser::PipelineStage::input_spectrum_ready);

  std::vector<float> noisy_ri(num_feats * num_frames * 2);
  for (size_t t = 0; t < num_frames; ++t) {
    for (size_t f = 0; f < num_feats; ++f) {
      const size_t offset = (f * num_frames + t) * 2;
      noisy_ri[offset] = specs[t].real[f];
      noisy_ri[offset + 1] = specs[t].imag[f];
    }
  }

  std::vector<std::vector<float>*> inputs;
  std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], static_cast<int64_t>(num_frames));

  prepare_input_in_place(noisy_ri, inputs, denoise_context);
  std::vector<Ort::Value> outputs;
  run(inputs, outputs, resolved_input_shapes);
  parse_results<float>(outputs, denoise_data);

  if (denoise_context->prev_features.size() < model_params_.network_input_ctx_len) {
    denoise_task->state.set_outcome(gigaedge::denoiser::ChunkOutcome::warming_up);
  }

  denoise_task->state.advance_to(gigaedge::denoiser::PipelineStage::model_output_ready);
  denoise_context->prev_features.push(specs.front());
}

}  // namespace onnx
}  // namespace util
