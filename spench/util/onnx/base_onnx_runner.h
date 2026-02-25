#pragma once

#include <memory>
#include <string>
#include <vector>
#include <exception>
#include <numeric>
#include <functional>
#include <cstdint>
#include "spench/util/onnx/onnx_env.h"
#include "spench/src/spench_task.h"
#include "spench/src/spench_context.h"
#include "spench/src/spench_config_reader.h"

namespace util {
namespace onnx {

class OnnxValidationException: public std::runtime_error {
 public:
  explicit OnnxValidationException(const std::string& what)
    : std::runtime_error(what) {}
};

class BaseOnnxRunner {
 public:
  BaseOnnxRunner();
  BaseOnnxRunner& operator=(const BaseOnnxRunner&) = delete;
  BaseOnnxRunner(const BaseOnnxRunner&) = delete;

  BaseOnnxRunner(const std::vector<std::string>& inputs,
                 const std::vector<std::string>& outputs,
                 const std::shared_ptr<spench::NetworkConfig> config,
                 const std::string& password,
                 const std::string& checksum,
                 const uint32_t num_threads,
                 bool gpu);
  virtual ~BaseOnnxRunner();

  virtual void Infer(spench::SpenchTask* denoise_task) = 0;
  // change two last arguments
  virtual void PrepareInputInPlace(std::vector<float>& mag,
                                   std::vector<std::vector<float>*>& inputs,
                                   spench::SpenchContext<float>* denoise_context) = 0;

  virtual uint32_t GetFutureContextLen() {
    return m_model_params.m_network_input_ctx_len;
  }

 protected:
  void ResizeIfNeededInPlace(std::vector<float>& to_check,
                             const std::vector<int64_t>& shape,
                             uint32_t num_parts = 1);

  void ResizeIfNeededInPlace(std::valarray<float>& to_check,
                             const std::vector<int64_t>& shape,
                             uint32_t num_parts = 1);

  // Returns axis index of the only dynamic dim (-1). If none -> -1.
  // If there are multiple dynamic dims -> throws (contract violation).
  static int32_t GetSingleDynamicAxis(const std::vector<int64_t>& shape, const char* tensor_debug_name);

  static inline void SetTimeDimIfDynamic(std::vector<int64_t>& shape, int32_t time_axis, int64_t num_frames) {
    if (time_axis >= 0) { shape[static_cast<size_t>(time_axis)] = num_frames; }
  }

  void ValidateModel(const std::vector<std::string>& input_names,
                     const std::vector<std::string>& output_names);
  void InitModel(const std::shared_ptr<spench::NetworkConfig> config,
                 const std::string& password,
                 const std::string& checksum,
                 const uint32_t num_threads,
                 bool gpu);

  template<typename T>
  void Run(const std::vector<std::vector<T>*>& inputs,
           std::vector<Ort::Value>& outputs,
           const std::vector<std::vector<int64_t>>& input_shapes) {
    if (inputs.size() != input_shapes.size()) {
      throw std::runtime_error("Provided inputs do not match provided shapes!");
    }
    if (inputs.size() != m_model_input_names.size()) {
      throw std::runtime_error("Provided inputs do not match model inputs!");
    }

    std::vector<Ort::Value> ort_inputs;
    ort_inputs.reserve(inputs.size());

    for (size_t i = 0; i < inputs.size(); ++i) {
      ort_inputs.emplace_back(
        Ort::Value::CreateTensor<T>(
            *m_memory_info_ptr,
            const_cast<T*>(inputs[i]->data()),
            inputs[i]->size(),
            input_shapes[i].data(),
            input_shapes[i].size()));
    }

    outputs = m_session_ptr->Run(*m_run_options_ptr,
                                 m_model_input_names.data(), ort_inputs.data(), ort_inputs.size(),
                                 m_model_output_names.data(), m_model_output_names.size());
  }

  // keep old API for static models
  template<typename T>
  void Run(const std::vector<std::vector<T>*>& inputs, std::vector<Ort::Value>& outputs) {
    Run(inputs, outputs, m_model_input_shapes);
  }

  template<typename T>
  void WarmUp(const std::vector<std::vector<T>*>& inputs) {
    std::vector<Ort::Value> outputs;
    std::vector<std::vector<int64_t>> resolved_input_shapes = m_model_input_shapes;
    SetTimeDimIfDynamic(resolved_input_shapes[0], m_time_axis_in[0], static_cast<uint64_t>(1));
    for (int16_t i = 0; i < 3; ++i) {
      Run(inputs, outputs, resolved_input_shapes);
    }
  }

  void ReadConfig(const YAML::Node& yaml_config) {
    m_model_params.ReadModelNode(yaml_config);
  }

 protected:
  std::unique_ptr<Ort::Session> m_session_ptr = nullptr;
  Ort::MemoryInfo* m_memory_info_ptr = nullptr;
  std::unique_ptr<Ort::RunOptions> m_run_options_ptr = nullptr;
  Ort::AllocatorWithDefaultOptions m_default_allocator;

  std::vector<char*> m_model_input_names;
  std::vector<char*> m_model_output_names;
  std::vector<std::vector<int64_t>> m_model_input_shapes;
  std::vector<std::vector<int64_t>> m_model_output_shapes;
  // For each tensor: index of the only dynamic dim (-1). If tensor is static -> -1.
  // This is a readability optimization based on contract: only time is dynamic.
  std::vector<int32_t> m_time_axis_in;

  spench::DenoiseModelParams m_model_params;
};

}  // namespace onnx
}  // namespace util
