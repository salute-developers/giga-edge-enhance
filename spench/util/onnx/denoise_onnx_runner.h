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
  void InitOutputTensors(spench::SpenchTaskData& data);

  template<typename T>
  void ParseResults(std::vector<Ort::Value>& network_output, spench::SpenchTaskData& data) {
    if (m_model_params.m_model_outputs.size() != network_output.size()) {
      throw("Model output error");
    }
    if (data.context == nullptr) {
      throw("Context is not allocated!");
    }

    T* probabilities = network_output[0].GetTensorMutableData<T>();
    spench::DenoiseContextQ<T>* context = dynamic_cast<spench::DenoiseContextQ<T>*>(data.context);
    context->fullband_hidden.assign(probabilities,
                                                  probabilities + context->fullband_hidden.size());
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fullband_cell_state.assign(probabilities,
                                                      probabilities + context->fullband_cell_state.size());
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->subband_hidden.assign(probabilities,
                                                  probabilities + context->subband_hidden.size());
    probabilities = network_output[3].GetTensorMutableData<T>();
    context->subband_cell_state.assign(probabilities,
                                                      probabilities + context->subband_cell_state.size());
    probabilities = network_output[4].GetTensorMutableData<T>();

    for (uint64_t i = 0; i < 2 * data.new_spectrogram.real.size(); i += 2) {
      data.new_spectrogram.real[i / 2] = probabilities[i];
      data.new_spectrogram.imag[i / 2] = probabilities[i + 1];
    }
  }

 public:
  SpenchQOnnxRunner() = delete;
  virtual ~SpenchQOnnxRunner() = default;
  explicit SpenchQOnnxRunner(const YAML::Node& yaml_config,
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

    uint64_t multi = std::accumulate(m_model_input_shapes[0].begin(),
                                     m_model_input_shapes[0].end(),
                                     1,
                                     std::multiplies<uint64_t>());
    std::vector<T> mag(multi, 0.0);
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
  explicit SpenchFOnnxRunner(const YAML::Node& yaml_config,
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

    std::vector<T> dummy_mag(multi, 0.0);
    PrepareInputInPlace(dummy_mag, inputs, dummy_task.m_data.context);
    InitOutputTensors(dummy_task.m_data);
    BaseOnnxRunner::WarmUp<T>(inputs);
  }
};
}  // namespace onnx
}  // namespace util
