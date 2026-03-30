#pragma once

#include <memory>
#include <string>
#include <vector>
#include <exception>
#include <numeric>
#include <functional>
#include "spench/util/onnx/base_onnx_runner.h"
#include "spench/util/onnx/onnx_env.h"
#include "spench/src/spench_task.h"
#include "spench/src/spench_context.h"

namespace util {
namespace onnx {
class SpenchQOnnxRunner: public BaseOnnxRunner {
  void InitOutputTensors(spench::SpenchTaskData& data, uint32_t num_frames = 1);

  template<typename T>
  void ParseResults(std::vector<Ort::Value>& network_output, spench::SpenchTaskData& data) {
    if (m_model_params.m_model_outputs.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    spench::DenoiseContextQ<T>* context = dynamic_cast<spench::DenoiseContextQ<T>*>(data.context);
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->fullband_hidden.assign(probabilities,
                                                  probabilities + context->fullband_hidden.size());
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fb_snr_noise_psd.assign(probabilities,
                                                  probabilities + context->fb_snr_noise_psd.size());
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->fb_snr_noise_pg.assign(probabilities,
                                                      probabilities + context->fb_snr_noise_pg.size());
    probabilities = network_output[3].GetTensorMutableData<T>();
    context->sb_snr_noise_psd.assign(probabilities,
                                                      probabilities + context->sb_snr_noise_psd.size());
    probabilities = network_output[4].GetTensorMutableData<T>();
    context->sb_snr_noise_pg.assign(probabilities,
                                                      probabilities + context->sb_snr_noise_pg.size());
    probabilities = network_output[5].GetTensorMutableData<T>();
    context->fb_conv1_buf.assign(probabilities,
                                                      probabilities + context->fb_conv1_buf.size());
    probabilities = network_output[6].GetTensorMutableData<T>();
    context->fb_conv3_buf.assign(probabilities,
                                                      probabilities + context->fb_conv3_buf.size());
    probabilities = network_output[7].GetTensorMutableData<T>();
    auto info = network_output[7].GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();

    if (shape.empty() || shape.back() != 2) {
      throw std::runtime_error("Invalid output shape: expected last dimension = 2 (complex)");
    }

    const size_t num_feats = static_cast<size_t>(static_cast<size_t>(shape.at(1)));
    const size_t total_elems = static_cast<size_t>(info.GetElementCount());

    if (total_elems % 2 != 0) {
      throw std::runtime_error("Invalid output buffer: element count is not divisible by 2");
    }

    const size_t complex_pairs = total_elems / 2;
    if (complex_pairs % num_feats != 0) {
      throw std::runtime_error("Invalid output buffer: complex pairs not divisible by num_feats");
    }

    const size_t num_frames = complex_pairs / num_feats;
    UnpackComplexMask(probabilities, num_feats, num_frames, data.new_spectrogram);
  }

 public:
  SpenchQOnnxRunner() = delete;
  virtual ~SpenchQOnnxRunner() = default;
  explicit SpenchQOnnxRunner(const std::shared_ptr<spench::NetworkConfig> config,
                             const std::string& password,
                             const std::string& checksum,
                             uint32_t inference_engine_threads = 1,
                             bool on_gpu = false);
  void PrepareInputInPlace(std::vector<float>& mag,
                           std::vector<std::vector<float>*>& inputs,
                           spench::SpenchContext<float>* denoise_context) override;

  template<typename T>
  void WarmUp() {
    spench::SpenchTask dummy_task;
    std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>());
    dummy_task.m_data.context = ctx.get();
    std::vector<std::vector<T>*> inputs;
    InitOutputTensors(dummy_task.m_data);

    uint64_t multi = std::abs(std::accumulate(m_model_input_shapes[0].begin(),
                                              m_model_input_shapes[0].end(),
                                              int32_t{1},
                                              std::multiplies<int32_t>()));
    std::vector<T> mag(multi, 1e-4);
    PrepareInputInPlace(mag, inputs, dummy_task.m_data.context);

    BaseOnnxRunner::WarmUp<T>(inputs);
  }

  void Infer(spench::SpenchTask* denoise_task) override;
};

class SpenchFOnnxRunner: public BaseOnnxRunner {
  void InitOutputTensors(spench::SpenchTaskData& data);

  template<typename T>
  void ParseResults(std::vector<Ort::Value>& network_output, spench::SpenchTaskData& data) {
    if (m_model_params.m_model_outputs.size() != network_output.size()) {
      throw("Model output error");
    }
    if (data.context == nullptr) {
      throw("Context is not allocated!");
    }

    spench::DenoiseContextF<T>* context = dynamic_cast<spench::DenoiseContextF<T>*>(data.context);
    // fb_h
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->fullband_hidden.assign(probabilities,
                                             probabilities + context->fullband_hidden.size());

    // fb_c
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fullband_cell_state.assign(probabilities,
                                                 probabilities + context->fullband_cell_state.size());

    // new_fb_out_5
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->prev_fb_out.assign(probabilities,
                                  probabilities + context->prev_fb_out.size());

    // network_output
    probabilities = network_output[3].GetTensorMutableData<T>();
    for (uint64_t i = 0; i < 2 * data.new_spectrogram.real.size(); i += 2) {
      data.new_spectrogram.real[i / 2] = probabilities[i];
      data.new_spectrogram.imag[i / 2] = probabilities[i + 1];
    }
  }

 public:
  SpenchFOnnxRunner() = delete;
  virtual ~SpenchFOnnxRunner() = default;
  explicit SpenchFOnnxRunner(const std::shared_ptr<spench::NetworkConfig> config,
                             const std::string& password,
                             const std::string& checksum,
                             uint32_t inference_engine_threads = 1,
                             bool on_gpu = false);

  // change two last arguments
  void PrepareInputInPlace(std::vector<float>& mag,
                           std::vector<std::vector<float>*>& inputs,
                           spench::SpenchContext<float>* denoise_context) override;
  void Infer(spench::SpenchTask* denoise_task) override;

  template<typename T>
  void WarmUp() {
    spench::SpenchTask dummy_task;
    std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>());
    dummy_task.m_data.context = ctx.get();

    std::vector<std::vector<T>*> inputs;
    // batch_size * num_features (for example 1 * 385)
    uint64_t multi = m_model_input_shapes[0][0] * m_model_input_shapes[0][1];

    std::vector<T> dummy_mag(multi, 1e-4);
    PrepareInputInPlace(dummy_mag, inputs, dummy_task.m_data.context);
    InitOutputTensors(dummy_task.m_data);
    BaseOnnxRunner::WarmUp<T>(inputs);
  }
};

class SpenchSFOnnxRunner : public BaseOnnxRunner {
  void InitOutputTensors(spench::SpenchTaskData& data, uint32_t out_shape = 1);

  template<typename T>
  void ParseResults(std::vector<Ort::Value>& network_output, spench::SpenchTaskData& data) {
    if (m_model_params.m_model_outputs.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    spench::DenoiseContextSF<T>* context =
        dynamic_cast<spench::DenoiseContextSF<T>*>(data.context);
    if (!context) {
      throw std::runtime_error("Invalid context type for SpenchSFOnnxRunner");
    }

    // fb_h
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->fullband_hidden_state.assign(
        probabilities,
        probabilities + context->fullband_hidden_state.size());

    // fb_c
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fullband_cell_state.assign(
        probabilities,
        probabilities + context->fullband_cell_state.size());

    // fb_conv1_buf
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->fb_conv1_buf.assign(
        probabilities,
        probabilities + context->fb_conv1_buf.size());

    // fb_conv3_buf
    probabilities = network_output[3].GetTensorMutableData<T>();
    context->fb_conv3_buf.assign(
        probabilities,
        probabilities + context->fb_conv3_buf.size());

    // complex mask output with dynamic time dimension
    probabilities = network_output[4].GetTensorMutableData<T>();
    auto info = network_output[4].GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();

    if (shape.empty() || shape.back() != 2) {
      throw std::runtime_error(
          "Invalid output shape: expected last dimension = 2 (complex)");
    }

    const size_t num_feats = static_cast<size_t>(shape.at(1));
    const size_t total_elems = static_cast<size_t>(info.GetElementCount());

    if (total_elems % 2 != 0) {
      throw std::runtime_error(
          "Invalid output buffer: element count is not divisible by 2");
    }

    const size_t complex_pairs = total_elems / 2;
    if (complex_pairs % num_feats != 0) {
      throw std::runtime_error(
          "Invalid output buffer: complex pairs not divisible by num_feats");
    }

    const size_t num_frames = complex_pairs / num_feats;
    UnpackComplexMask(probabilities, num_feats, num_frames, data.new_spectrogram);
  }

 public:
  SpenchSFOnnxRunner() = delete;
  virtual ~SpenchSFOnnxRunner() = default;
  explicit SpenchSFOnnxRunner(const std::shared_ptr<spench::NetworkConfig> config,
                              const std::string& password,
                              const std::string& checksum,
                              uint32_t inference_engine_threads = 1,
                              bool on_gpu = false);

  void PrepareInputInPlace(std::vector<float>& mag,
                           std::vector<std::vector<float>*>& inputs,
                           spench::SpenchContext<float>* denoise_context) override;

  void Infer(spench::SpenchTask* denoise_task) override;

  template<typename T>
  void WarmUp() {
    spench::SpenchTask dummy_task;
    std::unique_ptr<spench::DenoiseContextSF<float>> ctx(new spench::DenoiseContextSF<float>());
    dummy_task.m_data.context = ctx.get();

    std::vector<std::vector<T>*> inputs;
    InitOutputTensors(dummy_task.m_data);

    std::vector<std::vector<int64_t>> resolved_input_shapes = m_model_input_shapes;
    SetTimeDimIfDynamic(resolved_input_shapes[0], m_time_axis_in[0], 1);

    uint64_t multi = static_cast<uint64_t>(std::abs(std::accumulate(
        resolved_input_shapes[0].begin(),
        resolved_input_shapes[0].end(),
        int32_t{1},
        std::multiplies<int32_t>())));

    std::vector<T> dummy_mag(multi, static_cast<T>(1e-4));
    PrepareInputInPlace(dummy_mag, inputs, dummy_task.m_data.context);

    BaseOnnxRunner::WarmUp<T>(inputs);
  }
};

}  // namespace onnx
}  // namespace util
