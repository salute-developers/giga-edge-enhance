#pragma once

#include <memory>
#include <string>
#include <vector>
#include <exception>
#include <numeric>
#include <functional>
#include "spench/util/onnx/base_onnx_runner.h"
#include "spench/util/onnx/onnx_env.h"
#include "spench/src/task.h"
#include "spench/src/denoise_context.h"

namespace util {
namespace onnx {
class SpenchQOnnxRunner: public BaseOnnxRunner {
  void InitOutputTensors(spench::DenoiseTaskData& data);

  template<typename T>
  void ParseResults(std::vector<Ort::Value>& network_output, spench::DenoiseTaskData& data) {
    if (m_model_params.m_model_outputs.size() != network_output.size()) {
      throw("Model output error");
    }
    if (data.m_context == nullptr) {
      throw("Context is not allocated!");
    }

    T* probabilities = network_output[0].GetTensorMutableData<T>();
    spench::DenoiseContextQ<T>* context = dynamic_cast<spench::DenoiseContextQ<T>*>(data.m_context);
    context->m_fullband_hidden.assign(probabilities,
                                                  probabilities + context->m_fullband_hidden.size());
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->m_fullband_cell_state.assign(probabilities,
                                                      probabilities + context->m_fullband_cell_state.size());
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->m_subband_hidden.assign(probabilities,
                                                  probabilities + context->m_subband_hidden.size());
    probabilities = network_output[3].GetTensorMutableData<T>();
    context->m_subband_cell_state.assign(probabilities,
                                                      probabilities + context->m_subband_cell_state.size());
    probabilities = network_output[4].GetTensorMutableData<T>();

    for (uint64_t i = 0; i < 2 * data.m_network_output.real.size(); i += 2) {
      data.m_network_output.real[i / 2] = probabilities[i];
      data.m_network_output.imag[i / 2] = probabilities[i + 1];
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
                           spench::DenoiseContext<float>* denoise_context) override;

  template<typename T>
  void WarmUp() {
    spench::DenoiseTask dummy_task;
    std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>());
    dummy_task.m_data.m_context = ctx.get();
    std::vector<std::vector<T>*> inputs;
    InitOutputTensors(dummy_task.m_data);

    uint64_t multi = std::accumulate(m_model_input_shapes[0].begin(),
                                     m_model_input_shapes[0].end(),
                                     1,
                                     std::multiplies<uint64_t>());
    std::vector<T> mag(multi, 0.0);
    PrepareInputInPlace(mag, inputs, dummy_task.m_data.m_context);

    BaseOnnxRunner::WarmUp<T>(inputs);
  }

  void Infer(spench::DenoiseTask* denoise_task) override;
};

class SpenchFOnnxRunner: public BaseOnnxRunner {
  void InitOutputTensors(spench::DenoiseTaskData& data);

  template<typename T>
  void ParseResults(std::vector<Ort::Value>& network_output, spench::DenoiseTaskData& data) {
    if (m_model_params.m_model_outputs.size() != network_output.size()) {
      throw("Model output error");
    }
    if (data.m_context == nullptr) {
      throw("Context is not allocated!");
    }

    spench::DenoiseContextF<T>* context = dynamic_cast<spench::DenoiseContextF<T>*>(data.m_context);
    // fb_h
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->m_fullband_hidden.assign(probabilities,
                                             probabilities + context->m_fullband_hidden.size());

    // fb_c
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->m_fullband_cell_state.assign(probabilities,
                                                 probabilities + context->m_fullband_cell_state.size());

    // new_fb_out_5
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->m_prev_fb_out.assign(probabilities,
                                  probabilities + context->m_prev_fb_out.size());

    // network_output
    probabilities = network_output[3].GetTensorMutableData<T>();
    for (uint64_t i = 0; i < 2 * data.m_network_output.real.size(); i += 2) {
      data.m_network_output.real[i / 2] = probabilities[i];
      data.m_network_output.imag[i / 2] = probabilities[i + 1];
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
                           spench::DenoiseContext<float>* denoise_context) override;
  void Infer(spench::DenoiseTask* denoise_task) override;

  template<typename T>
  void WarmUp() {
    spench::DenoiseTask dummy_task;
    std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>());
    dummy_task.m_data.m_context = ctx.get();

    std::vector<std::vector<T>*> inputs;
    // batch_size * num_features (for example 1 * 385)
    uint64_t multi = m_model_input_shapes[0][0] * m_model_input_shapes[0][1];

    std::vector<T> dummy_mag(multi, 0.0);
    PrepareInputInPlace(dummy_mag, inputs, dummy_task.m_data.m_context);
    InitOutputTensors(dummy_task.m_data);
    BaseOnnxRunner::WarmUp<T>(inputs);
  }
};
}  // namespace onnx
}  // namespace util
