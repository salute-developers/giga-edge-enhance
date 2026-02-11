#include "spench/util/onnx/denoise_onnx_runner.h"

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
#include "spench/submodules/modelencryption/lib/include/crypto.h"
#endif

namespace util {
namespace onnx {

SpenchQOnnxRunner::SpenchQOnnxRunner(const std::shared_ptr<spench::NetworkConfig> config,
                                     const std::string& password,
                                     const std::string& checksum,
                                     uint32_t inference_engine_threads,
                                     bool on_gpu) {
    ReadConfig(config->params()["model"]);
    InitModel(config, password, checksum, inference_engine_threads, on_gpu);
    ValidateModel(m_model_params.m_model_inputs, m_model_params.m_model_outputs);
    WarmUp<float>();
}

void SpenchQOnnxRunner::PrepareInputInPlace(std::vector<float>& mag,
                                           std::vector<std::vector<float>*>& inputs,
                                           spench::SpenchContext<float>* context) {
  if (context == nullptr) {
    throw std::runtime_error("Context is not allocated!");
  }
  spench::DenoiseContextQ<float>* denoise_context =
                                      dynamic_cast<spench::DenoiseContextQ<float>*>(context);
  if (!denoise_context) {
    throw std::runtime_error("Invalid context type for SpenchQOnnxRunner");
  }
  inputs.clear();

  ResizeIfNeededInPlace(denoise_context->noisy_mag,        m_model_input_shapes[0]);   // noisy_mag_in [1,1,769,1]
  ResizeIfNeededInPlace(denoise_context->fullband_hidden,  m_model_input_shapes[1]);   // fb_h
  ResizeIfNeededInPlace(denoise_context->fb_snr_noise_psd, m_model_input_shapes[2]);
  ResizeIfNeededInPlace(denoise_context->fb_snr_noise_pg,  m_model_input_shapes[3]);
  ResizeIfNeededInPlace(denoise_context->sb_snr_noise_psd, m_model_input_shapes[4]);
  ResizeIfNeededInPlace(denoise_context->sb_snr_noise_pg,  m_model_input_shapes[5]);
  ResizeIfNeededInPlace(denoise_context->fb_conv1_buf,     m_model_input_shapes[6]);
  ResizeIfNeededInPlace(denoise_context->fb_conv3_buf,     m_model_input_shapes[7]);

  const uint32_t num_feats = static_cast<uint32_t>(m_model_input_shapes[0][2]);
  if (mag.size() < num_feats) {
    throw std::runtime_error("mag size is smaller than expected");
  }
  std::fill(denoise_context->noisy_mag.begin(),
            denoise_context->noisy_mag.end(),
            0.0f);
  for (uint32_t f = 0; f < num_feats; ++f) {
    denoise_context->noisy_mag[f] = mag[f];
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

void SpenchQOnnxRunner::InitOutputTensors(spench::SpenchTaskData& data) {
  ResizeIfNeededInPlace(data.new_spectrogram.real, m_model_output_shapes[4], 2);
  ResizeIfNeededInPlace(data.new_spectrogram.imag, m_model_output_shapes[4], 2);
}

void SpenchQOnnxRunner::Infer(spench::SpenchTask* denoise_task) {
  if (denoise_task == nullptr || denoise_task->m_data.context == nullptr) {
    throw std::runtime_error("Task data or context are not initialized!");
  }
  if (denoise_task->m_data.input_spectrogram.size() == 0) {
    throw std::runtime_error("Input spectrogram should be calculated before this step!");
  }
  auto& denoise_data = denoise_task->m_data;
  auto& denoise_context = denoise_data.context;
  if (denoise_context->prev_features.size() == m_model_params.m_network_input_ctx_len + 1) {
    denoise_context->prev_features.pop();
  }

  if (denoise_task->m_stage == spench::PipelineStage::FOURIER_TRANSFORMED_SPEC) {
    std::vector<std::vector<float>*> inputs;
    std::vector<float> mag = denoise_data.input_spectrogram[0].getMagnitude();
    PrepareInputInPlace(mag, inputs, denoise_context);
    InitOutputTensors(denoise_data);
    std::vector<Ort::Value> outputs;

    Run(inputs, outputs);
    ParseResults<float>(outputs, denoise_data);

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
    throw std::runtime_error("State is not appropriate");
  }
}

SpenchFOnnxRunner::SpenchFOnnxRunner(const std::shared_ptr<spench::NetworkConfig> config,
                                     const std::string& password,
                                     const std::string& checksum,
                                     uint32_t inference_engine_threads,
                                     bool on_gpu) {
    ReadConfig(config->params()["model"]);
    InitModel(config, password, checksum, inference_engine_threads, on_gpu);
    ValidateModel(m_model_params.m_model_inputs, m_model_params.m_model_outputs);
    WarmUp<float>();
}

void SpenchFOnnxRunner::InitOutputTensors(spench::SpenchTaskData& data) {
  // other outputs were resized in PrepareInputInPlace
  ResizeIfNeededInPlace(data.new_spectrogram.real, m_model_output_shapes[3], 2);
  ResizeIfNeededInPlace(data.new_spectrogram.imag, m_model_output_shapes[3], 2);
}

void SpenchFOnnxRunner::PrepareInputInPlace(std::vector<float>& mag,
                                            std::vector<std::vector<float>*>& inputs,
                                            spench::SpenchContext<float>* context) {
  if (context == nullptr) {
    throw("Context is not allocated!");
  }
  spench::DenoiseContextF<float>* denoise_context =
                                     dynamic_cast<spench::DenoiseContextF<float>*>(context);
  ResizeIfNeededInPlace(denoise_context->noisy_mag, m_model_input_shapes[0]);  // noisy_mag_5
  ResizeIfNeededInPlace(denoise_context->fullband_hidden, m_model_input_shapes[1]);  // fb_h
  ResizeIfNeededInPlace(denoise_context->fullband_cell_state, m_model_input_shapes[2]);  // fb_c
  ResizeIfNeededInPlace(denoise_context->prev_fb_out, m_model_input_shapes[3]);  // new_fb_out_5

  // forget 1 frame out of num_frames (5) and move remaining to the left
  uint32_t from, to;
  uint32_t num_feats = m_model_input_shapes[0][1];
  uint32_t num_frames = m_model_input_shapes[0][2];
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

void SpenchFOnnxRunner::Infer(spench::SpenchTask* denoise_task) {
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
    std::vector<float> mag = denoise_data.input_spectrogram[0].getMagnitude();
    SpenchFOnnxRunner::PrepareInputInPlace(mag, inputs, denoise_context);
    InitOutputTensors(denoise_data);
    std::vector<Ort::Value> outputs;

    Run(inputs, outputs);
    ParseResults<float>(outputs, denoise_data);

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

}  // namespace onnx
}  // namespace util
