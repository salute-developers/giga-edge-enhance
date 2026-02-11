#include <functional>
#include <string>
#include <memory>
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
  for (auto const & name : input_names) {
    m_model_inputs.push_back(m_signature_runner->input_tensor(name.c_str()));
    uint16_t dims_num = m_model_inputs.back()->dims->size;
    m_model_input_size.push_back(
      std::accumulate(&m_model_inputs.back()->dims->data[0],
                      &m_model_inputs.back()->dims->data[dims_num],
                      uint64_t{1}, std::multiplies<uint64_t>()));
  }
  for (auto const & name : output_names) {
    m_model_outputs.push_back(m_signature_runner->output_tensor(name.c_str()));
    uint16_t dims_num = m_model_outputs.back()->dims->size;
    m_model_output_size.push_back(
      std::accumulate(&m_model_outputs.back()->dims->data[0],
                      &m_model_outputs.back()->dims->data[dims_num],
                      uint64_t{1}, std::multiplies<uint64_t>()));
  }
}

template<typename T>
void BaseTfliteRunner::Run(std::vector<std::vector<T>*>& inputs) {
  if (inputs.size() != m_model_inputs.size()) {
    throw TfliteValidationException("Model run failed. Number of inputs are differ.");
  }

  T* data_ptr;
  for (uint16_t i = 0; i < inputs.size(); ++i) {
    if (m_model_input_size[i] != inputs[i]->size()) {
      std::string err_msg = "Model run failed. Dim size of provided and model input #";
      err_msg += std::to_string(i) + " are differ: ";
      err_msg += std::to_string(m_model_input_size[i]) + " vs " + std::to_string(inputs[i]->size()) + "\n";
      throw TfliteValidationException(err_msg);
    }
    data_ptr = tflite::GetTensorData<T>(m_model_inputs[i]);
    std::memcpy(data_ptr, inputs[i]->data(), sizeof(T)*inputs[i]->size());
  }

  m_interpreter->Invoke();
}

void BaseTfliteRunner::ResizeIfNeededInPlace(std::vector<float>& to_check,
                                             uint32_t shape) {
  if (to_check.size() == 0) {
    to_check.resize(shape, 0.0);
  }
}

void BaseTfliteRunner::ResizeIfNeededInPlace(std::valarray<float>& to_check,
                                             uint32_t shape) {
  if (to_check.size() == 0) {
    to_check.resize(shape, 0.0);
  }
}

void BaseTfliteRunner::InitOutputTensors(spench::SpenchTaskData& data, uint32_t size) {
  ResizeIfNeededInPlace(data.new_spectrogram.real, size / 2);
  ResizeIfNeededInPlace(data.new_spectrogram.imag, size / 2);
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
  uint32_t num_feats = m_model_inputs[0]->dims->data[1];
  uint32_t num_frames = m_model_inputs[0]->dims->data[2];

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
      Run(inputs);
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
  spench::SpenchTask dummy_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>());
  dummy_task.m_data.context = ctx.get();

  std::vector<T> dummy_mag(m_model_input_size[0], 0.0);
  std::vector<T> dummy_fb_h(m_model_input_size[1], 0.0);
  std::vector<T> dummy_fb_c(m_model_input_size[2], 0.0);
  std::vector<T> dummy_sb_h(m_model_input_size[3], 0.0);
  std::vector<T> dummy_sb_c(m_model_input_size[4], 0.0);

  std::vector<std::vector<T>*> inputs;
  inputs.push_back(&dummy_mag);
  inputs.push_back(&dummy_fb_h);
  inputs.push_back(&dummy_fb_c);
  inputs.push_back(&dummy_sb_h);
  inputs.push_back(&dummy_sb_c);

  BaseTfliteRunner::WarmUp<T>(inputs);
}

void SpenchQTfliteRunner::PrepareInputInPlace(std::vector<float>& mag,
                                              std::vector<std::vector<float>*>& inputs,
                                              spench::SpenchContext<float>* context) {
  if (context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextQ<float>* denoise_context =
                                      dynamic_cast<spench::DenoiseContextQ<float>*>(context);
  ResizeIfNeededInPlace(denoise_context->noisy_mag,        m_model_input_size[0]);   // noisy_mag_in [1,1,769,1]
  ResizeIfNeededInPlace(denoise_context->fullband_hidden,  m_model_input_size[1]);   // fb_h
  ResizeIfNeededInPlace(denoise_context->fb_snr_noise_psd, m_model_input_size[2]);
  ResizeIfNeededInPlace(denoise_context->fb_snr_noise_pg,  m_model_input_size[3]);
  ResizeIfNeededInPlace(denoise_context->sb_snr_noise_psd, m_model_input_size[4]);
  ResizeIfNeededInPlace(denoise_context->sb_snr_noise_pg,  m_model_input_size[5]);
  ResizeIfNeededInPlace(denoise_context->fb_conv1_buf,     m_model_input_size[6]);
  ResizeIfNeededInPlace(denoise_context->fb_conv3_buf,     m_model_input_size[7]);
  const TfLiteTensor* mag_tensor = m_model_inputs[0];
  const uint32_t num_feats  = mag_tensor->dims->data[2];  // 769
  const uint32_t num_frames = mag_tensor->dims->data[3];  // 5
  uint32_t from, to;
  for (uint32_t frame_i = 1; frame_i < num_frames; ++frame_i) {
    for (uint32_t feat_i = 0; feat_i < num_feats; ++feat_i) {
      from = frame_i + feat_i * num_frames;
      to   = (frame_i - 1) + feat_i * num_frames;
      denoise_context->noisy_mag[to] = denoise_context->noisy_mag[from];
    }
  }
  for (uint32_t feat_i = 0; feat_i < num_feats; ++feat_i) {
    to = num_frames - 1 + feat_i * num_frames;
    denoise_context->noisy_mag[to] = mag[feat_i];
  }
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
      case 7:
        for (uint64_t i = 0; i < m_model_output_size[output_idx]; i += 2) {
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
    std::vector<float> mag = denoise_data.input_spectrogram[0].getMagnitude();
    SpenchQTfliteRunner::PrepareInputInPlace(mag, inputs, denoise_context);
    SpenchQTfliteRunner::InitOutputTensors(denoise_data, m_model_output_size[4]);

    {
      std::lock_guard<std::mutex> lock(m_model_run_mutex);
      Run(inputs);
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
