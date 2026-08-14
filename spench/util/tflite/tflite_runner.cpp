#include <functional>
#include <algorithm>
#include <string>
#include <memory>
#include <sstream>
#include "spench/util/tflite/tflite_runner.h"

namespace util {
namespace tf_lite {

BaseTfliteRunner::BaseTfliteRunner() {}

BaseTfliteRunner::~BaseTfliteRunner() {}

BaseTfliteRunner::BaseTfliteRunner(std::vector<std::string>& input_names,
                                   std::vector<std::string>& output_names,
                                   const std::shared_ptr<spench::NetworkConfig> config,
                                   const uint32_t num_threads, bool on_gpu) {
  (void)(on_gpu);
  init_model(input_names, output_names, config, num_threads, on_gpu);
}

void BaseTfliteRunner::init_model(std::vector<std::string>& input_names,
                                  std::vector<std::string>& output_names,
                                   const std::shared_ptr<spench::NetworkConfig> config,
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

  if (signature_runner_->input_names().size() != input_names.size()) {
    throw TfliteValidationException("Model and config file have different number of inputs");
  }
  if (signature_runner_->output_names().size() != output_names.size()) {
    throw TfliteValidationException("Model and config file have different number of outputs");
  }

  model_input_names_.assign(input_names.begin(), input_names.end());
  model_output_names_.assign(output_names.begin(), output_names.end());

  refresh_tensor_metadata();
}

template<typename T>
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

void BaseTfliteRunner::resize_if_needed_in_place(std::vector<float>& to_check,
                                             uint32_t shape) {
  if (to_check.size() != shape) {
    to_check.resize(shape, 0.0f);
  }
}

void BaseTfliteRunner::resize_if_needed_in_place(std::valarray<float>& to_check,
                                             uint32_t shape) {
  if (to_check.size() != shape) {
    to_check.resize(shape, 0.0f);
  }
}

void BaseTfliteRunner::init_output_tensors(spench::SpenchTaskData& data, uint32_t size) {
  resize_if_needed_in_place(data.new_spectrogram.real, size / 2);
  resize_if_needed_in_place(data.new_spectrogram.imag, size / 2);
}

int32_t BaseTfliteRunner::get_single_dynamic_axis(const TfLiteTensor* tensor,
                                               const char* tensor_debug_name) {
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

void BaseTfliteRunner::set_time_dim_if_dynamic(std::vector<int32_t>& shape,
                                           int32_t time_axis,
                                           int32_t num_frames) {
  if (time_axis >= 0) {
    shape[static_cast<size_t>(time_axis)] = num_frames;
  }
}

uint64_t BaseTfliteRunner::shape_product(const std::vector<int32_t>& shape) {
  return std::accumulate(shape.begin(), shape.end(),
                         uint64_t{1},
                         [](uint64_t acc, int32_t dim) {
                           if (dim <= 0) {
                             throw TfliteValidationException("Resolved tensor shape contains non-positive dimension");
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

    if (signature_runner_->ResizeInputTensor(model_input_names_[i].c_str(),
                                              input_shapes[i]) != kTfLiteOk) {
      throw TfliteValidationException("Failed to resize input tensor: " + model_input_names_[i]);
    }
  }

  if (signature_runner_->AllocateTensors() != kTfLiteOk) {
    throw TfliteValidationException("AllocateTensors failed after input resize");
  }

  refresh_tensor_metadata();
}

SpenchFTfliteRunner::SpenchFTfliteRunner(const std::shared_ptr<spench::NetworkConfig> config,
                                         uint32_t inference_engine_threads, bool on_gpu) :
                                         BaseTfliteRunner::BaseTfliteRunner() {
    read_config(config->params()["model"]);
    init_model(model_params_.model_inputs, model_params_.model_outputs,
                config, inference_engine_threads, on_gpu);
    warm_up<float>();
}

template<typename T>
void SpenchFTfliteRunner::warm_up() {
  spench::SpenchTask dummy_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>());
  dummy_task.data.context = ctx.get();

  std::vector<T> dummy_mag(input_tensor_sizes[0], 0.0);
  std::vector<T> dummy_fb_h(input_tensor_sizes[1], 0.0);
  std::vector<T> dummy_fb_c(input_tensor_sizes[2], 0.0);
  std::vector<T> dummy_prev_fb_out(input_tensor_sizes[3], 0.0);

  std::vector<std::vector<T>*> inputs;
  inputs.push_back(&dummy_mag);
  inputs.push_back(&dummy_fb_h);
  inputs.push_back(&dummy_fb_c);
  inputs.push_back(&dummy_prev_fb_out);

  BaseTfliteRunner::warm_up<T>(inputs);
}

void SpenchFTfliteRunner::prepare_input_in_place(std::vector<float>& mag,
                                            std::vector<std::vector<float>*>& inputs,
                                            spench::SpenchContext<float>* context) {
  if (context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextF<float>* denoise_context =
                                     dynamic_cast<spench::DenoiseContextF<float>*>(context);

  uint32_t from, to;
  uint32_t num_feats = static_cast<uint32_t>(model_input_shapes_[0][1]);
  uint32_t num_frames = static_cast<uint32_t>(model_input_shapes_[0][2]);

  if (denoise_context->noisy_mag.size() == 0) {
    // for the first frame of wav only
    denoise_context->noisy_mag.resize(input_tensor_sizes[0]);
  } else {
    // for the next frames forget 1 frame out of num_frames (5) and move remaining to the left
    for (uint32_t frame_i = 1; frame_i < num_frames; ++frame_i) {
      for (uint32_t feat_i = 0; feat_i < num_feats; ++feat_i) {
        from = frame_i + feat_i * num_frames;
        to = (frame_i - 1) + feat_i * num_frames;
        denoise_context->noisy_mag[to] = denoise_context->noisy_mag[from];
      }
    }
  }

  // add new noisy_mag to the input
  for (uint32_t feat_i = 0; feat_i < num_feats; ++feat_i) {
    to = num_frames - 1 + feat_i * num_frames;
    denoise_context->noisy_mag[to] = mag[feat_i];
  }

  resize_if_needed_in_place(denoise_context->noisy_mag, input_tensor_sizes[0]);  // noisy_mag_5
  resize_if_needed_in_place(denoise_context->fullband_hidden, input_tensor_sizes[1]);  // fb_h
  resize_if_needed_in_place(denoise_context->fullband_cell_state, input_tensor_sizes[2]);  // fb_c
  resize_if_needed_in_place(denoise_context->prev_fb_out, input_tensor_sizes[3]);  // new_fb_out_5

  inputs.push_back(&denoise_context->noisy_mag);
  inputs.push_back(&denoise_context->fullband_hidden);
  inputs.push_back(&denoise_context->fullband_cell_state);
  inputs.push_back(&denoise_context->prev_fb_out);
}

template<typename T>
void SpenchFTfliteRunner::parse_results(spench::SpenchTaskData& data) {
  if (data.context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextF<T>* context = dynamic_cast<spench::DenoiseContextF<T>*>(data.context);

  const T* const_output_ptr;
  for (uint16_t output_idx = 0; output_idx < output_tensor_sizes.size(); output_idx++) {
    const_output_ptr = tflite::GetTensorData<T>(model_outputs_[output_idx]);
    switch (output_idx) {
      case 0:
        context->fullband_hidden.assign(const_output_ptr,
                                          const_output_ptr + context->fullband_hidden.size());
        break;
      case 1:
        context->fullband_cell_state.assign(const_output_ptr,
                                              const_output_ptr + context->fullband_cell_state.size());
        break;
      case 2:
        context->prev_fb_out.assign(const_output_ptr,
                                      const_output_ptr + context->prev_fb_out.size());
        break;
      case 3:
        for (uint64_t i = 0; i < output_tensor_sizes[3]; i += 2) {
          data.new_spectrogram.real[i / 2] = const_output_ptr[i];
          data.new_spectrogram.imag[i / 2] = const_output_ptr[i + 1];
        }
        break;
      default:
        std::string err_msg = "Error in parsing results. Unknown output_idx: ";
        err_msg += std::to_string(output_idx);
        throw(err_msg);
    }
  }
}

void SpenchFTfliteRunner::infer(spench::SpenchTask* denoise_task) {
  if (denoise_task == nullptr || denoise_task->data.context == nullptr) {
    throw("Task data or context are not initialized!");
  }
  if (denoise_task->data.input_spectrogram.size() == 0) {
    throw("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len) {
    denoise_context->prev_features.pop();
  }
  denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);

  if (denoise_task->stage == spench::PipelineStage::fourier_transformed_spec) {
    std::vector<std::vector<float>*> inputs;
    std::vector<std::vector<float>> outputs;
    std::vector<float> mag = denoise_data.input_spectrogram[0].get_magnitude();
    SpenchFTfliteRunner::prepare_input_in_place(mag, inputs, denoise_context);
    SpenchFTfliteRunner::init_output_tensors(denoise_data, output_tensor_sizes[3]);

    {
      std::lock_guard<std::mutex> lock(model_run_mutex_);
      run(inputs, model_input_shapes_);
      parse_results<float>(denoise_data);
    }

    if (denoise_context->prev_spectrogram.real.size() == 0) {
      denoise_data.enhanced_wav.resize(denoise_data.waveform.size() / 2);
      std::fill_n(denoise_data.enhanced_wav.begin(), denoise_data.enhanced_wav.size(), 0.0);
      denoise_task->stage = spench::PipelineStage::predicted_mask;
    } else if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len + 1) {
      throw("Incorrect number of previous states!");
    } else {
      denoise_task->stage = spench::PipelineStage::predicted_mask;
    }
  } else {
    throw("State is not appropriate");
  }
}

SpenchQTfliteRunner::SpenchQTfliteRunner(const std::shared_ptr<spench::NetworkConfig> config,
                                         uint32_t inference_engine_threads, bool on_gpu) :
                                         BaseTfliteRunner::BaseTfliteRunner() {
    read_config(config->params()["model"]);
    init_model(model_params_.model_inputs, model_params_.model_outputs,
                config, inference_engine_threads, on_gpu);
    warm_up<float>();
}

template<typename T>
void SpenchQTfliteRunner::warm_up() {
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

void SpenchQTfliteRunner::prepare_input_in_place(std::vector<float>& mag,
                                              std::vector<std::vector<float>*>& inputs,
                                              spench::SpenchContext<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  spench::DenoiseContextQ<float>* denoise_context =
      dynamic_cast<spench::DenoiseContextQ<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for SpenchQTfliteRunner");
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

  resize_if_needed_in_place(denoise_context->noisy_mag,
                        static_cast<uint32_t>(shape_product(resolved_input_shapes[0])));
  resize_if_needed_in_place(denoise_context->fullband_hidden,  static_cast<uint32_t>(input_tensor_sizes[1]));
  resize_if_needed_in_place(denoise_context->fb_snr_noise_psd, static_cast<uint32_t>(input_tensor_sizes[2]));
  resize_if_needed_in_place(denoise_context->fb_snr_noise_pg,  static_cast<uint32_t>(input_tensor_sizes[3]));
  resize_if_needed_in_place(denoise_context->sb_snr_noise_psd, static_cast<uint32_t>(input_tensor_sizes[4]));
  resize_if_needed_in_place(denoise_context->sb_snr_noise_pg,  static_cast<uint32_t>(input_tensor_sizes[5]));
  resize_if_needed_in_place(denoise_context->fb_conv1_buf,     static_cast<uint32_t>(input_tensor_sizes[6]));
  resize_if_needed_in_place(denoise_context->fb_conv3_buf,     static_cast<uint32_t>(input_tensor_sizes[7]));

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

template<typename T>
void SpenchQTfliteRunner::parse_results(spench::SpenchTaskData& data) {
  if (data.context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextQ<T>* context = dynamic_cast<spench::DenoiseContextQ<T>*>(data.context);

  const T* const_output_ptr;
  for (uint16_t output_idx = 0; output_idx < output_tensor_sizes.size(); output_idx++) {
    const_output_ptr = tflite::GetTensorData<T>(model_outputs_[output_idx]);
    switch (output_idx) {
      case 0:
        context->fullband_hidden.assign(const_output_ptr,
                                              const_output_ptr + context->fullband_hidden.size());
        break;
      case 1:
        context->fb_snr_noise_psd.assign(const_output_ptr,
                                         const_output_ptr + context->fb_snr_noise_psd.size());
        break;
      case 2:
        context->fb_snr_noise_pg.assign(const_output_ptr,
                                         const_output_ptr + context->fb_snr_noise_pg.size());
        break;
      case 3:
        context->sb_snr_noise_psd.assign(const_output_ptr,
                                             const_output_ptr + context->sb_snr_noise_psd.size());
        break;
      case 4:
        context->sb_snr_noise_pg.assign(const_output_ptr,
                                             const_output_ptr + context->sb_snr_noise_pg.size());
        break;
      case 5:
        context->fb_conv1_buf.assign(const_output_ptr,
                                             const_output_ptr + context->fb_conv1_buf.size());
        break;
      case 6:
        context->fb_conv3_buf.assign(const_output_ptr,
                                             const_output_ptr + context->fb_conv3_buf.size());
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
        throw(err_msg);
    }
  }
}

void SpenchQTfliteRunner::infer(spench::SpenchTask* denoise_task) {
  if (denoise_task == nullptr || denoise_task->data.context == nullptr) {
    throw("Task data or context are not initialized!");
  }
  if (denoise_task->data.input_spectrogram.size() == 0) {
    throw("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() > model_params_.network_input_ctx_len) {
    denoise_context->prev_features.pop();
  }

  if (denoise_task->stage == spench::PipelineStage::fourier_transformed_spec) {
    std::vector<std::vector<float>*> inputs;
    std::vector<std::vector<float>> outputs;
    std::vector<float> mag;
    const auto& specs = denoise_data.input_spectrogram;

    const std::size_t num_feats  = specs.front().size();
    const std::size_t num_frames = specs.size();

    // Pre-size output buffer for (freq, time) layout: [F * T]
    mag.resize(num_feats * num_frames);
    // Fill mag so that time is the fastest axis: mag[f * T + t]
    for (std::size_t t = 0; t < num_frames; ++t) {
      specs[t].write_magnitude_interleaved(mag, t, num_frames);
    }

    std::vector<std::vector<int32_t>> resolved_input_shapes = model_input_shapes_;
    set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], static_cast<int32_t>(num_frames));

    SpenchQTfliteRunner::prepare_input_in_place(mag, inputs, denoise_context);
    {
      std::lock_guard<std::mutex> lock(model_run_mutex_);
      run(inputs, resolved_input_shapes);
      SpenchQTfliteRunner::init_output_tensors(denoise_data, static_cast<uint32_t>(output_tensor_sizes[7]));
      parse_results<float>(denoise_data);
    }

    if (denoise_context->prev_features.size() < model_params_.network_input_ctx_len) {
      denoise_data.enhanced_wav.resize(denoise_data.waveform.size() / 2);
      std::fill_n(denoise_data.enhanced_wav.begin(), denoise_data.enhanced_wav.size(), 0.0);
      denoise_task->stage = spench::PipelineStage::skipped_chunk;
    } else if (denoise_context->prev_features.size() == model_params_.network_input_ctx_len &&
               denoise_context->prev_spectrogram.real.size() == 0) {
      denoise_data.enhanced_wav.resize(denoise_data.waveform.size() / 2);
      std::fill_n(denoise_data.enhanced_wav.begin(), denoise_data.enhanced_wav.size(), 0.0);
      denoise_task->stage = spench::PipelineStage::predicted_mask;
    } else {
      denoise_task->stage = spench::PipelineStage::predicted_mask;
    }

    denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);
  } else {
    throw("State is not appropriate");
  }
}

}  // namespace tf_lite
}  // namespace util
