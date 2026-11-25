#include "util/tflite/tflite_runner.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace util {
namespace tf_lite {

BaseTfliteRunner::BaseTfliteRunner() {}

BaseTfliteRunner::~BaseTfliteRunner() {}

BaseTfliteRunner::BaseTfliteRunner(const std::shared_ptr<giga_edge::enhance::NetworkConfig> config,
                                   const uint32_t num_threads, bool on_gpu) {
  (void)(on_gpu);
  init_model(config, num_threads, on_gpu);
}

void BaseTfliteRunner::init_model(const std::shared_ptr<giga_edge::enhance::NetworkConfig> config,
                                  const uint32_t num_threads, bool on_gpu) {
  (void)(on_gpu);
  if (config->is_model_bundled()) {
    const auto& model_data = config->get_model();
    model_data_ = std::make_unique<std::vector<char>>(model_data.begin(), model_data.end());
    model_ = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(model_data_->data(), model_data_->size());
  } else {
    model_ = tflite::FlatBufferModel::VerifyAndBuildFromFile(config->model_path().c_str());
  }

  if (model_ == nullptr) {
    throw TfliteValidationException("Model reading error. There is problem with checkpoint file.");
  }

  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*model_, resolver);

  if (builder.SetNumThreads(num_threads) != kTfLiteOk) {
    throw TfliteValidationException("Can not set number of threads");
  }
  if (builder(&interpreter_) != kTfLiteOk) {
    throw TfliteValidationException("Interpreter creation error. Can not create interpreter");
  }
  if (interpreter_->AllocateTensors() != kTfLiteOk) {
    throw TfliteValidationException("Interpreter tensor allocation error");
  }

  signature_runner_ = interpreter_->GetSignatureRunner("serving_default");
  if (signature_runner_ == nullptr) {
    throw TfliteValidationException("Model does not has required 'serving_default' signature");
  }

  model_input_names_.assign(signature_runner_->input_names().begin(), signature_runner_->input_names().end());
  model_output_names_.assign(signature_runner_->output_names().begin(), signature_runner_->output_names().end());

  refresh_tensor_metadata();
}

void BaseTfliteRunner::set_model_io_order(const std::vector<std::string>& input_names,
                                          const std::vector<std::string>& output_names) {
  const auto validate_names = [](const std::vector<std::string>& discovered, const std::vector<std::string>& ordered,
                                 const char* kind) {
    if (discovered.size() != ordered.size()) {
      throw TfliteValidationException(std::string("Unexpected number of model ") + kind);
    }
    for (const auto& name : ordered) {
      if (std::find(discovered.begin(), discovered.end(), name) == discovered.end()) {
        throw TfliteValidationException(std::string("Required model ") + kind + " is missing: " + name);
      }
    }
  };

  validate_names(model_input_names_, input_names, "inputs");
  validate_names(model_output_names_, output_names, "outputs");
  model_input_names_ = input_names;
  model_output_names_ = output_names;
  refresh_tensor_metadata();
}

template <typename T>
void BaseTfliteRunner::run(std::vector<std::vector<T>*>& inputs,
                           const std::vector<std::vector<int32_t>>& input_shapes) {
  if (inputs.size() != model_inputs_.size()) {
    throw TfliteValidationException("Model run failed. Number of inputs are differ.");
  }
  if (inputs.size() != input_shapes.size()) {
    throw TfliteValidationException("Model run failed. Number of provided shapes are differ.");
  }

  resize_inputs_if_needed(input_shapes);

  T* data_ptr = nullptr;
  for (size_t i = 0; i < inputs.size(); ++i) {
    uint64_t expected_size = 0;

    if (time_axis_in_[i] >= 0) {
      expected_size = shape_product(input_shapes[i]);
    } else {
      expected_size = input_tensor_sizes[i];
    }

    if (expected_size != inputs[i]->size()) {
      std::string err_msg = "Model run failed. Resolved dim size of provided and model input #";
      err_msg += std::to_string(i) + " are differ: ";
      err_msg += std::to_string(expected_size) + " vs " + std::to_string(inputs[i]->size());
      throw TfliteValidationException(err_msg);
    }

    data_ptr = tflite::GetTensorData<T>(model_inputs_[i]);
    if (data_ptr == nullptr) {
      throw TfliteValidationException("Failed to get input tensor buffer");
    }
    std::memcpy(data_ptr, inputs[i]->data(), sizeof(T) * inputs[i]->size());
  }

  if (signature_runner_->Invoke() != kTfLiteOk) {
    throw TfliteValidationException("Interpreter invoke failed");
  }

  refresh_tensor_metadata();
}

void BaseTfliteRunner::resize_if_needed_in_place(std::vector<float>& to_check, uint32_t shape) {
  if (to_check.size() != shape) {
    to_check.resize(shape, 0.0f);
  }
}

void BaseTfliteRunner::resize_if_needed_in_place(std::valarray<float>& to_check, uint32_t shape) {
  if (to_check.size() != shape) {
    to_check.resize(shape, 0.0f);
  }
}

void BaseTfliteRunner::init_output_tensors(giga_edge::enhance::TaskData& data, uint32_t size) {
  resize_if_needed_in_place(data.new_spectrogram.real, size / 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, size / 2);
}

int32_t BaseTfliteRunner::get_single_dynamic_axis(const TfLiteTensor* tensor, const char* tensor_debug_name) {
  if (tensor == nullptr) {
    throw TfliteValidationException(std::string("Null tensor: ") + tensor_debug_name);
  }

  const TfLiteIntArray* sig = tensor->dims_signature;
  if (sig == nullptr) {
    return -1;
  }

  int32_t found = -1;
  for (int32_t i = 0; i < sig->size; ++i) {
    if (sig->data[i] < 0) {
      if (found >= 0) {
        std::stringstream ss;
        ss << "Tensor has more than one dynamic dim (-1): " << tensor_debug_name;
        throw TfliteValidationException(ss.str());
      }
      found = i;
    }
  }
  return found;
}

void BaseTfliteRunner::set_time_dim_if_dynamic(std::vector<int32_t>& shape, int32_t time_axis, int32_t num_frames) {
  if (time_axis >= 0) {
    shape[static_cast<size_t>(time_axis)] = num_frames;
  }
}

uint64_t BaseTfliteRunner::shape_product(const std::vector<int32_t>& shape) {
  return std::accumulate(shape.begin(), shape.end(), uint64_t{1}, [](uint64_t acc, int32_t dim) {
    if (dim <= 0) {
      throw TfliteValidationException("Resolved tensor shape contains non-positive dimension");
    }
    if (static_cast<uint64_t>(dim) > std::numeric_limits<uint64_t>::max() / acc) {
      throw TfliteValidationException("Resolved tensor shape is too large");
    }
    return acc * static_cast<uint64_t>(dim);
  });
}

void BaseTfliteRunner::refresh_tensor_metadata() {
  model_inputs_.clear();
  model_outputs_.clear();
  model_input_shapes_.clear();
  model_output_shapes_.clear();
  input_tensor_sizes.clear();
  output_tensor_sizes.clear();
  time_axis_in_.clear();

  for (const auto& name : model_input_names_) {
    TfLiteTensor* t = signature_runner_->input_tensor(name.c_str());
    if (t == nullptr) {
      throw TfliteValidationException("Failed to get input tensor: " + name);
    }
    if (t->dims == nullptr) {
      throw TfliteValidationException("Input tensor has no dims: " + name);
    }

    model_inputs_.push_back(t);
    time_axis_in_.push_back(get_single_dynamic_axis(t, name.c_str()));

    std::vector<int32_t> concrete_shape;
    concrete_shape.reserve(static_cast<size_t>(t->dims->size));
    for (int32_t i = 0; i < t->dims->size; ++i) {
      concrete_shape.push_back(t->dims->data[i]);
    }

    model_input_shapes_.push_back(concrete_shape);
    input_tensor_sizes.push_back(shape_product(concrete_shape));
  }

  for (const auto& name : model_output_names_) {
    const TfLiteTensor* t = signature_runner_->output_tensor(name.c_str());
    if (t == nullptr) {
      throw TfliteValidationException("Failed to get output tensor: " + name);
    }
    if (t->dims == nullptr) {
      throw TfliteValidationException("Output tensor has no dims: " + name);
    }

    model_outputs_.push_back(t);

    std::vector<int32_t> concrete_shape;
    concrete_shape.reserve(static_cast<size_t>(t->dims->size));
    for (int32_t i = 0; i < t->dims->size; ++i) {
      concrete_shape.push_back(t->dims->data[i]);
    }

    model_output_shapes_.push_back(concrete_shape);
    output_tensor_sizes.push_back(shape_product(concrete_shape));
  }
}

void BaseTfliteRunner::resize_inputs_if_needed(const std::vector<std::vector<int32_t>>& input_shapes) {
  if (input_shapes.size() != model_input_names_.size()) {
    throw TfliteValidationException("Resolved input shapes do not match number of model inputs");
  }

  bool need_resize = false;

  for (size_t i = 0; i < input_shapes.size(); ++i) {
    if (time_axis_in_[i] < 0) {
      continue;  // static input
    }

    TfLiteTensor* t = signature_runner_->input_tensor(model_input_names_[i].c_str());
    if (t == nullptr || t->dims == nullptr) {
      throw TfliteValidationException("Failed to inspect dynamic input tensor before resize");
    }

    if (static_cast<size_t>(t->dims->size) != input_shapes[i].size()) {
      need_resize = true;
      break;
    }

    for (int32_t j = 0; j < t->dims->size; ++j) {
      if (t->dims->data[j] != input_shapes[i][static_cast<size_t>(j)]) {
        need_resize = true;
        break;
      }
    }

    if (need_resize) {
      break;
    }
  }

  if (!need_resize) {
    return;
  }

  for (size_t i = 0; i < input_shapes.size(); ++i) {
    if (time_axis_in_[i] < 0) {
      continue;  // resize only dynamic inputs
    }

    if (signature_runner_->ResizeInputTensor(model_input_names_[i].c_str(), input_shapes[i]) != kTfLiteOk) {
      throw TfliteValidationException("Failed to resize input tensor: " + model_input_names_[i]);
    }
  }

  if (signature_runner_->AllocateTensors() != kTfLiteOk) {
    throw TfliteValidationException("AllocateTensors failed after input resize");
  }

  refresh_tensor_metadata();
}

DenoiserDiscriminativeHqTfliteRunner::DenoiserDiscriminativeHqTfliteRunner(
    const std::shared_ptr<giga_edge::enhance::NetworkConfig> config, uint32_t inference_engine_threads, bool on_gpu)
    : BaseTfliteRunner::BaseTfliteRunner() {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  set_model_io_order(
      {"noisy_mag_in", "prev_fb_hidden_in", "prev_fb_snr_noise_psd_in", "prev_fb_snr_noise_pg_in",
       "prev_sb_snr_noise_psd_in", "prev_sb_snr_noise_pg_in", "prev_fb_conv1_buf_in", "prev_fb_conv3_buf_in"},
      {"cur_fb_hidden_out", "cur_fb_snr_noise_psd_out", "cur_fb_snr_noise_pg_out", "cur_sb_snr_noise_psd_out",
       "cur_sb_snr_noise_pg_out", "cur_fb_conv1_buf_out", "cur_fb_conv3_buf_out", "predicted_mask_out"});
  warm_up<float>();
}

template <typename T>
void DenoiserDiscriminativeHqTfliteRunner::warm_up() {
  std::vector<std::vector<int32_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], 1);

  std::vector<T> dummy_noisy_mag(shape_product(resolved_input_shapes[0]), 0.0f);
  std::vector<T> dummy_fb_hidden(input_tensor_sizes[1], 0.0f);
  std::vector<T> dummy_fb_snr_psd(input_tensor_sizes[2], -1.0f);
  std::vector<T> dummy_fb_snr_pg(input_tensor_sizes[3], -1.0f);
  std::vector<T> dummy_sb_snr_psd(input_tensor_sizes[4], -1.0f);
  std::vector<T> dummy_sb_snr_pg(input_tensor_sizes[5], -1.0f);
  std::vector<T> dummy_conv1_buf(input_tensor_sizes[6], 0.0f);
  std::vector<T> dummy_conv3_buf(input_tensor_sizes[7], 0.0f);

  std::vector<std::vector<T>*> inputs;
  inputs.push_back(&dummy_noisy_mag);
  inputs.push_back(&dummy_fb_hidden);
  inputs.push_back(&dummy_fb_snr_psd);
  inputs.push_back(&dummy_fb_snr_pg);
  inputs.push_back(&dummy_sb_snr_psd);
  inputs.push_back(&dummy_sb_snr_pg);
  inputs.push_back(&dummy_conv1_buf);
  inputs.push_back(&dummy_conv3_buf);

  for (int16_t i = 0; i < 3; ++i) {
    run(inputs, resolved_input_shapes);
  }
}

void DenoiserDiscriminativeHqTfliteRunner::prepare_input_in_place(std::vector<float>& mag,
                                                                  std::vector<std::vector<float>*>& inputs,
                                                                  giga_edge::enhance::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  giga_edge::enhance::DenoiseContextDiscriminativeHq<float>* denoise_context =
      dynamic_cast<giga_edge::enhance::DenoiseContextDiscriminativeHq<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserDiscriminativeHqTfliteRunner");
  }

  inputs.clear();

  const uint32_t num_feats = static_cast<uint32_t>(model_input_shapes_[0][1]);
  if (num_feats == 0) {
    throw std::runtime_error("Invalid model input shape: num_feats is zero");
  }
  if (mag.size() < num_feats) {
    throw std::runtime_error("mag size is smaller than expected");
  }
  if (mag.size() % num_feats != 0) {
    throw std::runtime_error("mag size is not divisible by num_feats");
  }

  const uint32_t num_frames = static_cast<uint32_t>(mag.size() / num_feats);

  std::vector<std::vector<int32_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], static_cast<int32_t>(num_frames));

  resize_if_needed_in_place(denoise_context->noisy_mag, static_cast<uint32_t>(shape_product(resolved_input_shapes[0])));
  resize_if_needed_in_place(denoise_context->fullband_hidden, static_cast<uint32_t>(input_tensor_sizes[1]));
  resize_if_needed_in_place(denoise_context->fb_snr_noise_psd, static_cast<uint32_t>(input_tensor_sizes[2]));
  resize_if_needed_in_place(denoise_context->fb_snr_noise_pg, static_cast<uint32_t>(input_tensor_sizes[3]));
  resize_if_needed_in_place(denoise_context->sb_snr_noise_psd, static_cast<uint32_t>(input_tensor_sizes[4]));
  resize_if_needed_in_place(denoise_context->sb_snr_noise_pg, static_cast<uint32_t>(input_tensor_sizes[5]));
  resize_if_needed_in_place(denoise_context->fb_conv1_buf, static_cast<uint32_t>(input_tensor_sizes[6]));
  resize_if_needed_in_place(denoise_context->fb_conv3_buf, static_cast<uint32_t>(input_tensor_sizes[7]));

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

template <typename T>
void DenoiserDiscriminativeHqTfliteRunner::parse_results(giga_edge::enhance::TaskData& data) {
  if (data.context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  giga_edge::enhance::DenoiseContextDiscriminativeHq<T>* context =
      dynamic_cast<giga_edge::enhance::DenoiseContextDiscriminativeHq<T>*>(data.context);

  const T* const_output_ptr;
  for (uint16_t output_idx = 0; output_idx < output_tensor_sizes.size(); output_idx++) {
    const_output_ptr = tflite::GetTensorData<T>(model_outputs_[output_idx]);
    switch (output_idx) {
      case 0:
        context->fullband_hidden.assign(const_output_ptr, const_output_ptr + context->fullband_hidden.size());
        break;
      case 1:
        context->fb_snr_noise_psd.assign(const_output_ptr, const_output_ptr + context->fb_snr_noise_psd.size());
        break;
      case 2:
        context->fb_snr_noise_pg.assign(const_output_ptr, const_output_ptr + context->fb_snr_noise_pg.size());
        break;
      case 3:
        context->sb_snr_noise_psd.assign(const_output_ptr, const_output_ptr + context->sb_snr_noise_psd.size());
        break;
      case 4:
        context->sb_snr_noise_pg.assign(const_output_ptr, const_output_ptr + context->sb_snr_noise_pg.size());
        break;
      case 5:
        context->fb_conv1_buf.assign(const_output_ptr, const_output_ptr + context->fb_conv1_buf.size());
        break;
      case 6:
        context->fb_conv3_buf.assign(const_output_ptr, const_output_ptr + context->fb_conv3_buf.size());
        break;
      case 7: {
        const TfLiteTensor* t = model_outputs_[output_idx];
        if (!t || !t->dims || t->dims->size < 3) {
          throw std::runtime_error("Invalid output tensor #7: missing dims");
        }

        const int32_t last_dim = t->dims->data[t->dims->size - 1];
        if (last_dim != 2) {
          throw std::runtime_error("Invalid output shape: expected last dimension = 2 (complex)");
        }

        const size_t num_feats = static_cast<size_t>(t->dims->data[1]);
        const size_t num_frames = static_cast<size_t>(t->dims->data[2]);

        if (num_feats == 0 || num_frames == 0) {
          throw std::runtime_error("Invalid output shape: zero-sized dynamic output");
        }

        unpack_complex_mask(const_output_ptr, num_feats, num_frames, data.new_spectrogram);
        break;
      }
      default:
        std::string err_msg = "Error in parsing results. Unknown output_idx: ";
        err_msg += std::to_string(output_idx);
        throw std::runtime_error(err_msg);
    }
  }
}

void DenoiserDiscriminativeHqTfliteRunner::infer(giga_edge::enhance::Task* denoise_task) {
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

  denoise_task->state.require(giga_edge::enhance::PipelineStage::input_spectrum_ready);

  std::vector<std::vector<float>*> inputs;
  std::vector<float> mag;
  const auto& specs = denoise_data.input_spectrogram;
  const std::size_t num_feats = specs.front().size();
  const std::size_t num_frames = specs.size();

  mag.resize(num_feats * num_frames);
  for (std::size_t t = 0; t < num_frames; ++t) {
    specs[t].write_magnitude_interleaved(mag, t, num_frames);
  }

  {
    // Tensor resizing rebuilds metadata. Keep every metadata read and the
    // corresponding run in one critical section when a runner is shared.
    std::lock_guard<std::mutex> lock(model_run_mutex_);
    std::vector<std::vector<int32_t>> resolved_input_shapes = model_input_shapes_;
    set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], static_cast<int32_t>(num_frames));
    DenoiserDiscriminativeHqTfliteRunner::prepare_input_in_place(mag, inputs, denoise_context);
    run(inputs, resolved_input_shapes);
    DenoiserDiscriminativeHqTfliteRunner::init_output_tensors(denoise_data,
                                                              static_cast<uint32_t>(output_tensor_sizes[7]));
    parse_results<float>(denoise_data);
  }

  if (denoise_context->prev_features.size() < model_params_.network_input_ctx_len) {
    denoise_task->state.set_outcome(giga_edge::enhance::ChunkOutcome::warming_up);
  }
  denoise_task->state.advance_to(giga_edge::enhance::PipelineStage::model_output_ready);
  denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);
}

DenoiserHybridTfliteRunner::DenoiserHybridTfliteRunner(const std::shared_ptr<giga_edge::enhance::NetworkConfig> config,
                                                       uint32_t inference_engine_threads, bool on_gpu)
    : BaseTfliteRunner::BaseTfliteRunner() {
  read_config(config->params()["model"]);
  init_model(config, inference_engine_threads, on_gpu);
  set_model_io_order({"noisy_ri_in", "prev_fb_hidden_in", "prev_fb_snr_noise_psd_in", "prev_fb_snr_noise_pg_in",
                      "prev_sb_snr_noise_psd_in", "prev_sb_snr_noise_pg_in", "prev_fb_conv1_buf_in",
                      "prev_fb_conv3_buf_in", "prev_stage2_hidden_in"},
                     {"cur_fb_hidden_out", "cur_fb_snr_noise_psd_out", "cur_fb_snr_noise_pg_out",
                      "cur_sb_snr_noise_psd_out", "cur_sb_snr_noise_pg_out", "cur_fb_conv1_buf_out",
                      "cur_fb_conv3_buf_out", "cur_stage2_hidden_out", "predicted_ri_out"});
  if (model_input_shapes_[0].size() != 4 || model_input_shapes_[0][0] != 1 || model_input_shapes_[0][1] <= 0 ||
      model_input_shapes_[0][3] != 2 || (time_axis_in_[0] != -1 && time_axis_in_[0] != 2)) {
    throw std::runtime_error("Invalid hybrid input spectrum shape: expected [1, F, T, 2]");
  }
  for (size_t i = 0; i < model_inputs_.size(); ++i) {
    if (model_inputs_[i]->type != kTfLiteFloat32 || model_outputs_[i]->type != kTfLiteFloat32 ||
        input_tensor_sizes[i] == 0 || input_tensor_sizes[i] > std::numeric_limits<uint32_t>::max() ||
        (i > 0 && time_axis_in_[i] != -1)) {
      throw std::runtime_error("Hybrid models require FP32 tensors and fixed recurrent states");
    }
  }
  warm_up<float>();
}

template <typename T>
void DenoiserHybridTfliteRunner::warm_up() {
  std::vector<std::vector<int32_t>> resolved_input_shapes = model_input_shapes_;
  set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], 1);

  std::vector<T> dummy_noisy_ri(shape_product(resolved_input_shapes[0]), 0.0f);
  std::vector<T> dummy_fb_hidden(input_tensor_sizes[1], 0.0f);
  std::vector<T> dummy_fb_snr_psd(input_tensor_sizes[2], -1.0f);
  std::vector<T> dummy_fb_snr_pg(input_tensor_sizes[3], -1.0f);
  std::vector<T> dummy_sb_snr_psd(input_tensor_sizes[4], -1.0f);
  std::vector<T> dummy_sb_snr_pg(input_tensor_sizes[5], -1.0f);
  std::vector<T> dummy_conv1_buf(input_tensor_sizes[6], 0.0f);
  std::vector<T> dummy_conv3_buf(input_tensor_sizes[7], 0.0f);
  std::vector<T> dummy_stage2_hidden(input_tensor_sizes[8], 0.0f);

  std::vector<std::vector<T>*> inputs;
  inputs.push_back(&dummy_noisy_ri);
  inputs.push_back(&dummy_fb_hidden);
  inputs.push_back(&dummy_fb_snr_psd);
  inputs.push_back(&dummy_fb_snr_pg);
  inputs.push_back(&dummy_sb_snr_psd);
  inputs.push_back(&dummy_sb_snr_pg);
  inputs.push_back(&dummy_conv1_buf);
  inputs.push_back(&dummy_conv3_buf);
  inputs.push_back(&dummy_stage2_hidden);

  for (int16_t i = 0; i < 3; ++i) {
    run(inputs, resolved_input_shapes);
  }
}

void DenoiserHybridTfliteRunner::prepare_input_in_place(std::vector<float>& noisy_ri,
                                                        std::vector<std::vector<float>*>& inputs,
                                                        giga_edge::enhance::Context<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  giga_edge::enhance::DenoiseContextHybrid<float>* denoise_context =
      dynamic_cast<giga_edge::enhance::DenoiseContextHybrid<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for DenoiserHybridTfliteRunner");
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

  denoise_context->noisy_ri.resize(noisy_ri.size());
  resize_if_needed_in_place(denoise_context->fullband_hidden, static_cast<uint32_t>(input_tensor_sizes[1]));
  resize_if_needed_in_place(denoise_context->fb_snr_noise_psd, static_cast<uint32_t>(input_tensor_sizes[2]));
  resize_if_needed_in_place(denoise_context->fb_snr_noise_pg, static_cast<uint32_t>(input_tensor_sizes[3]));
  resize_if_needed_in_place(denoise_context->sb_snr_noise_psd, static_cast<uint32_t>(input_tensor_sizes[4]));
  resize_if_needed_in_place(denoise_context->sb_snr_noise_pg, static_cast<uint32_t>(input_tensor_sizes[5]));
  resize_if_needed_in_place(denoise_context->fb_conv1_buf, static_cast<uint32_t>(input_tensor_sizes[6]));
  resize_if_needed_in_place(denoise_context->fb_conv3_buf, static_cast<uint32_t>(input_tensor_sizes[7]));
  resize_if_needed_in_place(denoise_context->stage2_hidden, static_cast<uint32_t>(input_tensor_sizes[8]));

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

template <typename T>
void DenoiserHybridTfliteRunner::parse_results(giga_edge::enhance::TaskData& data) {
  if (data.context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  giga_edge::enhance::DenoiseContextHybrid<T>* context =
      dynamic_cast<giga_edge::enhance::DenoiseContextHybrid<T>*>(data.context);

  if (context == nullptr || model_outputs_.size() != 9) {
    throw std::runtime_error("Invalid hybrid context or output count");
  }
  std::vector<std::vector<T>*> states = {
      &context->fullband_hidden, &context->fb_snr_noise_psd, &context->fb_snr_noise_pg, &context->sb_snr_noise_psd,
      &context->sb_snr_noise_pg, &context->fb_conv1_buf,     &context->fb_conv3_buf,    &context->stage2_hidden};
  for (size_t i = 0; i < states.size(); ++i) {
    if (output_tensor_sizes[i] != states[i]->size()) {
      throw std::runtime_error("Hybrid recurrent output size mismatch");
    }
    const T* values = tflite::GetTensorData<T>(model_outputs_[i]);
    states[i]->assign(values, values + states[i]->size());
  }
  const auto& shape = model_output_shapes_[8];
  if (shape.size() != 4 || shape[0] != 1 || shape[1] != model_input_shapes_[0][1] || shape[2] <= 0 ||
      static_cast<size_t>(shape[2]) != data.input_spectrogram.size() || shape[3] != 2) {
    throw std::runtime_error("Invalid hybrid spectrum shape: expected [1, F, T, 2]");
  }
  unpack_complex_mask(tflite::GetTensorData<T>(model_outputs_[8]), static_cast<size_t>(shape[1]),
                      static_cast<size_t>(shape[2]), data.new_spectrogram);
}

void DenoiserHybridTfliteRunner::infer(giga_edge::enhance::Task* denoise_task) {
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

  const auto& specs = denoise_data.input_spectrogram;
  const size_t num_feats = specs.front().size();
  const size_t num_frames = specs.size();
  if (num_frames > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    throw std::runtime_error("Too many input spectrogram frames");
  }
  for (const auto& spec : specs) {
    if (num_feats == 0 || spec.size() != num_feats || spec.imag.size() != num_feats) {
      throw std::runtime_error("Input spectrogram frames have different or invalid sizes");
    }
  }

  denoise_task->state.require(giga_edge::enhance::PipelineStage::input_spectrum_ready);
  std::vector<float> noisy_ri(num_feats * num_frames * 2);
  for (size_t t = 0; t < num_frames; ++t) {
    for (size_t f = 0; f < num_feats; ++f) {
      const size_t offset = (f * num_frames + t) * 2;
      noisy_ri[offset] = specs[t].real[f];
      noisy_ri[offset + 1] = specs[t].imag[f];
    }
  }

  {
    std::lock_guard<std::mutex> lock(model_run_mutex_);
    if (num_feats != static_cast<size_t>(model_input_shapes_[0][1])) {
      throw std::runtime_error("Input spectrogram feature count does not match the hybrid model");
    }
    std::vector<std::vector<float>*> inputs;
    std::vector<std::vector<int32_t>> resolved_input_shapes = model_input_shapes_;
    set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], static_cast<int32_t>(num_frames));

    DenoiserHybridTfliteRunner::prepare_input_in_place(noisy_ri, inputs, denoise_context);
    run(inputs, resolved_input_shapes);
    parse_results<float>(denoise_data);
  }

  if (denoise_context->prev_features.size() < model_params_.network_input_ctx_len) {
    denoise_task->state.set_outcome(giga_edge::enhance::ChunkOutcome::warming_up);
  }
  denoise_task->state.advance_to(giga_edge::enhance::PipelineStage::model_output_ready);
  denoise_context->prev_features.push(specs.front());
}

}  // namespace tf_lite
}  // namespace util
