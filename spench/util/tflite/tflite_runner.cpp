#include <functional>
#include <algorithm>
#include <string>
#include <memory>
#include <sstream>
#include "spench/util/tflite/tflite_runner.h"

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
#include "spench/submodules/modelencryption/lib/include/crypto.h"
#endif

namespace util {
namespace tf_lite {

BaseTfliteRunner::BaseTfliteRunner() {}

BaseTfliteRunner::~BaseTfliteRunner() {}

BaseTfliteRunner::BaseTfliteRunner(std::vector<std::string>& input_names,
                                   std::vector<std::string>& output_names,
                                   const std::string& model_path,
                                   const std::string& password,
                                   const std::string& checksum,
                                   const uint32_t num_threads, bool on_gpu) {
  (void)(on_gpu);
  InitModel(input_names, output_names,
            model_path, password,
            checksum, num_threads,
            on_gpu);
}

void BaseTfliteRunner::InitModel(std::vector<std::string>& input_names,
                                 std::vector<std::string>& output_names,
                                 const std::string& model_path,
                                 const std::string& password,
                                 const std::string& checksum,
                                 const uint32_t num_threads, bool on_gpu) {
  (void)(on_gpu);
  if (password.empty() && checksum.empty()) {
    m_model = tflite::FlatBufferModel::VerifyAndBuildFromFile(model_path.c_str());
  } else {
#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
    modelencryption::Decryptor decryptor;
    modelencryption::Hasher hasher;
    try {
      std::unique_ptr<modelencryption::Checksum> license_checksum = nullptr;
      if (!checksum.empty()) {
        license_checksum.reset(new modelencryption::Checksum(hasher.DeserializeChecksum(checksum.c_str())));
      }
      std::vector<uint8_t> modelData =
                              decryptor.DecryptFile(model_path, modelencryption::Key(password), license_checksum.get());
      m_model_data = std::make_unique<std::vector<char>>(modelData.data(), modelData.data()+modelData.size());
    } catch (const std::exception& ex) {
      std::string err_message = std::string("Model reading error. Detailes: ")
                                + ex.what();
      throw TfliteValidationException(err_message);
    }
    m_model = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(m_model_data->data(),
                                                                m_model_data->size());
#else
    throw TfliteValidationException("Password must be empty or library should be built with encryption support");
#endif
  }

  if (m_model == nullptr) {
    throw TfliteValidationException("Model reading error. There is problem with checkpoint file.");
  }

  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*m_model, resolver);

  if (builder.SetNumThreads(num_threads) != kTfLiteOk) {
    throw TfliteValidationException("Can not set number of threads");
  }
  if (builder(&m_interpreter) != kTfLiteOk) {
    throw TfliteValidationException("Interpreter creation error. Can not create interpreter");
  }
  if (m_interpreter->AllocateTensors() != kTfLiteOk) {
    throw TfliteValidationException("Interpreter tensor allocation error");
  }

  m_signature_runner = m_interpreter->GetSignatureRunner("serving_default");
  if (m_signature_runner == nullptr) {
    throw TfliteValidationException("Model does not has required 'serving_default' signature");
  }

  if (m_signature_runner->input_names().size() != input_names.size()) {
    throw TfliteValidationException("Model and config file have different number of inputs");
  }
  if (m_signature_runner->output_names().size() != output_names.size()) {
    throw TfliteValidationException("Model and config file have different number of outputs");
  }

  m_model_input_names.assign(input_names.begin(), input_names.end());
  m_model_output_names.assign(output_names.begin(), output_names.end());

  RefreshTensorMetadata();
}

template<typename T>
void BaseTfliteRunner::Run(std::vector<std::vector<T>*>& inputs,
                           const std::vector<std::vector<int32_t>>& input_shapes) {
  if (inputs.size() != m_model_inputs.size()) {
    throw TfliteValidationException("Model run failed. Number of inputs are differ.");
  }
  if (inputs.size() != input_shapes.size()) {
    throw TfliteValidationException("Model run failed. Number of provided shapes are differ.");
  }

  ResizeInputsIfNeeded(input_shapes);

  T* data_ptr = nullptr;
  for (size_t i = 0; i < inputs.size(); ++i) {
    uint64_t expected_size = 0;

    if (m_time_axis_in[i] >= 0) {
      expected_size = ShapeProduct(input_shapes[i]);
    } else {
      expected_size = m_model_input_size[i];
    }

    if (expected_size != inputs[i]->size()) {
      std::string err_msg = "Model run failed. Resolved dim size of provided and model input #";
      err_msg += std::to_string(i) + " are differ: ";
      err_msg += std::to_string(expected_size) + " vs " + std::to_string(inputs[i]->size());
      throw TfliteValidationException(err_msg);
    }

    data_ptr = tflite::GetTensorData<T>(m_model_inputs[i]);
    if (data_ptr == nullptr) {
      throw TfliteValidationException("Failed to get input tensor buffer");
    }
    std::memcpy(data_ptr, inputs[i]->data(), sizeof(T) * inputs[i]->size());
  }

  if (m_signature_runner->Invoke() != kTfLiteOk) {
    throw TfliteValidationException("Interpreter invoke failed");
  }

  RefreshTensorMetadata();
}

void BaseTfliteRunner::ResizeIfNeededInPlace(std::vector<float>& to_check,
                                             uint32_t shape) {
  if (to_check.size() != shape) {
    to_check.resize(shape, 0.0f);
  }
}

void BaseTfliteRunner::ResizeIfNeededInPlace(std::valarray<float>& to_check,
                                             uint32_t shape) {
  if (to_check.size() != shape) {
    to_check.resize(shape, 0.0f);
  }
}

void BaseTfliteRunner::InitOutputTensors(spench::SpenchTaskData& data, uint32_t size) {
  ResizeIfNeededInPlace(data.new_spectrogram.real, size / 2);
  ResizeIfNeededInPlace(data.new_spectrogram.imag, size / 2);
}

int32_t BaseTfliteRunner::GetSingleDynamicAxis(const TfLiteTensor* tensor,
                                               const char* tensor_debug_name) {
  if (tensor == nullptr) {
    throw TfliteValidationException(std::string("Null tensor: ") + tensor_debug_name);
  }

  const TfLiteIntArray* sig = tensor->dims_signature;
  if (sig == nullptr) {
    return -1;
  }

  int32_t found = -1;
  for (int i = 0; i < sig->size; ++i) {
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

void BaseTfliteRunner::SetTimeDimIfDynamic(std::vector<int32_t>& shape,
                                           int32_t time_axis,
                                           int32_t num_frames) {
  if (time_axis >= 0) {
    shape[static_cast<size_t>(time_axis)] = num_frames;
  }
}

uint64_t BaseTfliteRunner::ShapeProduct(const std::vector<int32_t>& shape) {
  return std::accumulate(shape.begin(), shape.end(),
                         uint64_t{1},
                         [](uint64_t acc, int32_t dim) {
                           if (dim <= 0) {
                             throw TfliteValidationException("Resolved tensor shape contains non-positive dimension");
                           }
                           return acc * static_cast<uint64_t>(dim);
                         });
}

void BaseTfliteRunner::RefreshTensorMetadata() {
  m_model_inputs.clear();
  m_model_outputs.clear();
  m_model_input_shapes.clear();
  m_model_output_shapes.clear();
  m_model_input_size.clear();
  m_model_output_size.clear();
  m_time_axis_in.clear();

  for (const auto& name : m_model_input_names) {
    TfLiteTensor* t = m_signature_runner->input_tensor(name.c_str());
    if (t == nullptr) {
      throw TfliteValidationException("Failed to get input tensor: " + name);
    }
    if (t->dims == nullptr) {
      throw TfliteValidationException("Input tensor has no dims: " + name);
    }

    m_model_inputs.push_back(t);
    m_time_axis_in.push_back(GetSingleDynamicAxis(t, name.c_str()));

    std::vector<int32_t> concrete_shape;
    concrete_shape.reserve(static_cast<size_t>(t->dims->size));
    for (int i = 0; i < t->dims->size; ++i) {
      concrete_shape.push_back(t->dims->data[i]);
    }

    m_model_input_shapes.push_back(concrete_shape);
    m_model_input_size.push_back(ShapeProduct(concrete_shape));
  }

  for (const auto& name : m_model_output_names) {
    const TfLiteTensor* t = m_signature_runner->output_tensor(name.c_str());
    if (t == nullptr) {
      throw TfliteValidationException("Failed to get output tensor: " + name);
    }
    if (t->dims == nullptr) {
      throw TfliteValidationException("Output tensor has no dims: " + name);
    }

    m_model_outputs.push_back(t);

    std::vector<int32_t> concrete_shape;
    concrete_shape.reserve(static_cast<size_t>(t->dims->size));
    for (int i = 0; i < t->dims->size; ++i) {
      concrete_shape.push_back(t->dims->data[i]);
    }

    m_model_output_shapes.push_back(concrete_shape);
    m_model_output_size.push_back(ShapeProduct(concrete_shape));
  }
}

void BaseTfliteRunner::ResizeInputsIfNeeded(const std::vector<std::vector<int32_t>>& input_shapes) {
  if (input_shapes.size() != m_model_input_names.size()) {
    throw TfliteValidationException("Resolved input shapes do not match number of model inputs");
  }

  bool need_resize = false;

  for (size_t i = 0; i < input_shapes.size(); ++i) {
    if (m_time_axis_in[i] < 0) {
      continue;  // static input
    }

    TfLiteTensor* t = m_signature_runner->input_tensor(m_model_input_names[i].c_str());
    if (t == nullptr || t->dims == nullptr) {
      throw TfliteValidationException("Failed to inspect dynamic input tensor before resize");
    }

    if (static_cast<size_t>(t->dims->size) != input_shapes[i].size()) {
      need_resize = true;
      break;
    }

    for (int j = 0; j < t->dims->size; ++j) {
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
    if (m_time_axis_in[i] < 0) {
      continue;  // resize only dynamic inputs
    }

    if (m_signature_runner->ResizeInputTensor(m_model_input_names[i].c_str(),
                                              input_shapes[i]) != kTfLiteOk) {
      throw TfliteValidationException("Failed to resize input tensor: " + m_model_input_names[i]);
    }
  }

  if (m_signature_runner->AllocateTensors() != kTfLiteOk) {
    throw TfliteValidationException("AllocateTensors failed after input resize");
  }

  RefreshTensorMetadata();
}

SpenchFTfliteRunner::SpenchFTfliteRunner(const YAML::Node& yaml_config,
                                         const std::string& password,
                                         const std::string& checksum,
                                         uint32_t inference_engine_threads, bool on_gpu) :
                                         BaseTfliteRunner::BaseTfliteRunner() {
    ReadConfig(yaml_config);
    InitModel(m_model_params.m_model_inputs, m_model_params.m_model_outputs,
              m_model_params.m_model_path, password, checksum, inference_engine_threads,
              on_gpu);
    WarmUp<float>();
}

template<typename T>
void SpenchFTfliteRunner::WarmUp() {
  spench::SpenchTask dummy_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>());
  dummy_task.m_data.context = ctx.get();

  std::vector<T> dummy_mag(m_model_input_size[0], 0.0);
  std::vector<T> dummy_fb_h(m_model_input_size[1], 0.0);
  std::vector<T> dummy_fb_c(m_model_input_size[2], 0.0);
  std::vector<T> dummy_prev_fb_out(m_model_input_size[3], 0.0);

  std::vector<std::vector<T>*> inputs;
  inputs.push_back(&dummy_mag);
  inputs.push_back(&dummy_fb_h);
  inputs.push_back(&dummy_fb_c);
  inputs.push_back(&dummy_prev_fb_out);

  BaseTfliteRunner::WarmUp<T>(inputs);
}

void SpenchFTfliteRunner::PrepareInputInPlace(std::vector<float>& mag,
                                            std::vector<std::vector<float>*>& inputs,
                                            spench::SpenchContext<float>* context) {
  if (context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextF<float>* denoise_context =
                                     dynamic_cast<spench::DenoiseContextF<float>*>(context);

  uint32_t from, to;
  uint32_t num_feats = static_cast<uint32_t>(m_model_input_shapes[0][1]);
  uint32_t num_frames = static_cast<uint32_t>(m_model_input_shapes[0][2]);

  if (denoise_context->noisy_mag.size() == 0) {
    // for the first frame of wav only
    denoise_context->noisy_mag.resize(m_model_input_size[0]);
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

  ResizeIfNeededInPlace(denoise_context->noisy_mag, m_model_input_size[0]);  // noisy_mag_5
  ResizeIfNeededInPlace(denoise_context->fullband_hidden, m_model_input_size[1]);  // fb_h
  ResizeIfNeededInPlace(denoise_context->fullband_cell_state, m_model_input_size[2]);  // fb_c
  ResizeIfNeededInPlace(denoise_context->prev_fb_out, m_model_input_size[3]);  // new_fb_out_5

  inputs.push_back(&denoise_context->noisy_mag);
  inputs.push_back(&denoise_context->fullband_hidden);
  inputs.push_back(&denoise_context->fullband_cell_state);
  inputs.push_back(&denoise_context->prev_fb_out);
}

template<typename T>
void SpenchFTfliteRunner::ParseResults(spench::SpenchTaskData& data) {
  if (data.context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextF<T>* context = dynamic_cast<spench::DenoiseContextF<T>*>(data.context);

  const T* const_output_ptr;
  for (uint16_t output_idx = 0; output_idx < m_model_output_size.size(); output_idx++) {
    const_output_ptr = tflite::GetTensorData<T>(m_model_outputs[output_idx]);
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
        for (uint64_t i = 0; i < m_model_output_size[3]; i += 2) {
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

void SpenchFTfliteRunner::Infer(spench::SpenchTask* denoise_task) {
  if (denoise_task == nullptr || denoise_task->m_data.context == nullptr) {
    throw("Task data or context are not initialized!");
  }
  if (denoise_task->m_data.input_spectrogram.size() == 0) {
    throw("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->m_data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() > m_model_params.m_network_input_ctx_len) {
    denoise_context->prev_features.pop();
  }
  denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);

  if (denoise_task->m_stage == spench::PipelineStage::FOURIER_TRANSFORMED_SPEC) {
    std::vector<std::vector<float>*> inputs;
    std::vector<std::vector<float>> outputs;
    std::vector<float> mag = denoise_data.input_spectrogram[0].getMagnitude();
    SpenchFTfliteRunner::PrepareInputInPlace(mag, inputs, denoise_context);
    SpenchFTfliteRunner::InitOutputTensors(denoise_data, m_model_output_size[3]);

    {
      std::lock_guard<std::mutex> lock(m_model_run_mutex);
      Run(inputs, m_model_input_shapes);
      ParseResults<float>(denoise_data);
    }

    if (denoise_context->prev_spectrogram.real.size() == 0) {
      denoise_data.enhanced_wav.resize(denoise_data.waveform.size() / 2);
      std::fill_n(denoise_data.enhanced_wav.begin(), denoise_data.enhanced_wav.size(), 0.0);
      denoise_task->m_stage = spench::PipelineStage::PREDICTED_MASK;
    } else if (denoise_context->prev_features.size() > m_model_params.m_network_input_ctx_len + 1) {
      throw("Incorrect number of previous states!");
    } else {
      denoise_task->m_stage = spench::PipelineStage::PREDICTED_MASK;
    }
  } else {
    throw("State is not appropriate");
  }
}

SpenchQTfliteRunner::SpenchQTfliteRunner(const YAML::Node& yaml_config,
                                         const std::string& password,
                                         const std::string& checksum,
                                         uint32_t inference_engine_threads, bool on_gpu) :
                                         BaseTfliteRunner::BaseTfliteRunner() {
    ReadConfig(yaml_config);
    InitModel(m_model_params.m_model_inputs, m_model_params.m_model_outputs,
              m_model_params.m_model_path, password, checksum,
              inference_engine_threads, on_gpu);
    WarmUp<float>();
}

template<typename T>
void SpenchQTfliteRunner::WarmUp() {
  std::vector<std::vector<int32_t>> resolved_input_shapes = m_model_input_shapes;
  SetTimeDimIfDynamic(resolved_input_shapes[0], m_time_axis_in[0], 1);

  std::vector<T> dummy_noisy_mag(ShapeProduct(resolved_input_shapes[0]), 0.0f);
  std::vector<T> dummy_fb_hidden(m_model_input_size[1], 0.0f);
  std::vector<T> dummy_fb_snr_psd(m_model_input_size[2], -1.0f);
  std::vector<T> dummy_fb_snr_pg(m_model_input_size[3], -1.0f);
  std::vector<T> dummy_sb_snr_psd(m_model_input_size[4], -1.0f);
  std::vector<T> dummy_sb_snr_pg(m_model_input_size[5], -1.0f);
  std::vector<T> dummy_conv1_buf(m_model_input_size[6], 0.0f);
  std::vector<T> dummy_conv3_buf(m_model_input_size[7], 0.0f);

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
    Run(inputs, resolved_input_shapes);
  }
}

void SpenchQTfliteRunner::PrepareInputInPlace(std::vector<float>& mag,
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

  const uint32_t num_feats = static_cast<uint32_t>(m_model_input_shapes[0][1]);
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

  std::vector<std::vector<int32_t>> resolved_input_shapes = m_model_input_shapes;
  SetTimeDimIfDynamic(resolved_input_shapes[0], m_time_axis_in[0], static_cast<int32_t>(num_frames));

  ResizeIfNeededInPlace(denoise_context->noisy_mag,
                        static_cast<uint32_t>(ShapeProduct(resolved_input_shapes[0])));
  ResizeIfNeededInPlace(denoise_context->fullband_hidden,  static_cast<uint32_t>(m_model_input_size[1]));
  ResizeIfNeededInPlace(denoise_context->fb_snr_noise_psd, static_cast<uint32_t>(m_model_input_size[2]));
  ResizeIfNeededInPlace(denoise_context->fb_snr_noise_pg,  static_cast<uint32_t>(m_model_input_size[3]));
  ResizeIfNeededInPlace(denoise_context->sb_snr_noise_psd, static_cast<uint32_t>(m_model_input_size[4]));
  ResizeIfNeededInPlace(denoise_context->sb_snr_noise_pg,  static_cast<uint32_t>(m_model_input_size[5]));
  ResizeIfNeededInPlace(denoise_context->fb_conv1_buf,     static_cast<uint32_t>(m_model_input_size[6]));
  ResizeIfNeededInPlace(denoise_context->fb_conv3_buf,     static_cast<uint32_t>(m_model_input_size[7]));

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
void SpenchQTfliteRunner::ParseResults(spench::SpenchTaskData& data) {
  if (data.context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextQ<T>* context = dynamic_cast<spench::DenoiseContextQ<T>*>(data.context);

  const T* const_output_ptr;
  for (uint16_t output_idx = 0; output_idx < m_model_output_size.size(); output_idx++) {
    const_output_ptr = tflite::GetTensorData<T>(m_model_outputs[output_idx]);
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
        const TfLiteTensor* t = m_model_outputs[output_idx];
        if (!t || !t->dims || t->dims->size < 3) {
          throw std::runtime_error("Invalid output tensor #7: missing dims");
        }

        const int last_dim = t->dims->data[t->dims->size - 1];
        if (last_dim != 2) {
          throw std::runtime_error("Invalid output shape: expected last dimension = 2 (complex)");
        }

        const size_t num_feats = static_cast<size_t>(t->dims->data[1]);
        const size_t num_frames = static_cast<size_t>(t->dims->data[2]);

        if (num_feats == 0 || num_frames == 0) {
          throw std::runtime_error("Invalid output shape: zero-sized dynamic output");
        }

        UnpackComplexMask(const_output_ptr, num_feats, num_frames, data.new_spectrogram);
        break;
      }
      default:
        std::string err_msg = "Error in parsing results. Unknown output_idx: ";
        err_msg += std::to_string(output_idx);
        throw(err_msg);
    }
  }
}

void SpenchQTfliteRunner::Infer(spench::SpenchTask* denoise_task) {
  if (denoise_task == nullptr || denoise_task->m_data.context == nullptr) {
    throw("Task data or context are not initialized!");
  }
  if (denoise_task->m_data.input_spectrogram.size() == 0) {
    throw("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->m_data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() > m_model_params.m_network_input_ctx_len) {
    denoise_context->prev_features.pop();
  }

  if (denoise_task->m_stage == spench::PipelineStage::FOURIER_TRANSFORMED_SPEC) {
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
      specs[t].writeMagnitudeInterleaved(mag, t, num_frames);
    }

    std::vector<std::vector<int32_t>> resolved_input_shapes = m_model_input_shapes;
    SetTimeDimIfDynamic(resolved_input_shapes[0], m_time_axis_in[0], static_cast<int32_t>(num_frames));

    SpenchQTfliteRunner::PrepareInputInPlace(mag, inputs, denoise_context);
    {
      std::lock_guard<std::mutex> lock(m_model_run_mutex);
      Run(inputs, resolved_input_shapes);
      SpenchQTfliteRunner::InitOutputTensors(denoise_data, static_cast<uint32_t>(m_model_output_size[7]));
      ParseResults<float>(denoise_data);
    }

    if (denoise_context->prev_features.size() < m_model_params.m_network_input_ctx_len) {
      denoise_data.enhanced_wav.resize(denoise_data.waveform.size() / 2);
      std::fill_n(denoise_data.enhanced_wav.begin(), denoise_data.enhanced_wav.size(), 0.0);
      denoise_task->m_stage = spench::PipelineStage::SKIPPED_CHUNK;
    } else if (denoise_context->prev_features.size() == m_model_params.m_network_input_ctx_len &&
               denoise_context->prev_spectrogram.real.size() == 0) {
      denoise_data.enhanced_wav.resize(denoise_data.waveform.size() / 2);
      std::fill_n(denoise_data.enhanced_wav.begin(), denoise_data.enhanced_wav.size(), 0.0);
      denoise_task->m_stage = spench::PipelineStage::PREDICTED_MASK;
    } else {
      denoise_task->m_stage = spench::PipelineStage::PREDICTED_MASK;
    }

    denoise_context->prev_features.push(denoise_data.input_spectrogram[0]);
  } else {
    throw("State is not appropriate");
  }
}

}  // namespace tf_lite
}  // namespace util
