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

  virtual void infer(spench::SpenchTask* denoise_task) = 0;
  // change two last arguments
  virtual void prepare_input_in_place(std::vector<float>& mag,
                                   std::vector<std::vector<float>*>& inputs,
                                   spench::SpenchContext<float>* denoise_context) = 0;

  virtual uint32_t get_future_context_len() {
    return model_params_.network_input_ctx_len;
  }

 protected:
  void resize_if_needed_in_place(std::vector<float>& to_check,
                             const std::vector<int64_t>& shape,
                             uint32_t num_parts = 1);

  void resize_if_needed_in_place(std::valarray<float>& to_check,
                             const std::vector<int64_t>& shape,
                             uint32_t num_parts = 1);

  // Returns axis index of the only dynamic dim (-1). If none -> -1.
  // If there are multiple dynamic dims -> throws (contract violation).
  static int32_t get_single_dynamic_axis(const std::vector<int64_t>& shape, const char* tensor_debug_name);

  static inline void set_time_dim_if_dynamic(std::vector<int64_t>& shape, int32_t time_axis, int64_t num_frames) {
    if (time_axis >= 0) { shape[static_cast<size_t>(time_axis)] = num_frames; }
  }

  void validate_model(const std::vector<std::string>& input_names,
                     const std::vector<std::string>& output_names);
  void init_model(const std::shared_ptr<spench::NetworkConfig> config,
                 const std::string& password,
                 const std::string& checksum,
                 const uint32_t num_threads,
                 bool gpu);

  template<typename T>
  void run(const std::vector<std::vector<T>*>& inputs,
           std::vector<Ort::Value>& outputs,
           const std::vector<std::vector<int64_t>>& input_shapes) {
    if (inputs.size() != input_shapes.size()) {
      throw std::runtime_error("Provided inputs do not match provided shapes!");
    }
    if (inputs.size() != model_input_names_.size()) {
      throw std::runtime_error("Provided inputs do not match model inputs!");
    }

    std::vector<Ort::Value> ort_inputs;
    ort_inputs.reserve(inputs.size());

    for (size_t i = 0; i < inputs.size(); ++i) {
      ort_inputs.emplace_back(
        Ort::Value::CreateTensor<T>(
            *memory_info_ptr_,
            const_cast<T*>(inputs[i]->data()),
            inputs[i]->size(),
            input_shapes[i].data(),
            input_shapes[i].size()));
    }

    outputs = session_ptr_->Run(*run_options_ptr_,
                                 model_input_names_.data(), ort_inputs.data(), ort_inputs.size(),
                                 model_output_names_.data(), model_output_names_.size());
  }

  // keep old API for static models
  template<typename T>
  void run(const std::vector<std::vector<T>*>& inputs, std::vector<Ort::Value>& outputs) {
    run(inputs, outputs, model_input_shapes_);
  }

  template<typename T>
  void warm_up(const std::vector<std::vector<T>*>& inputs) {
    std::vector<Ort::Value> outputs;
    std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
    set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], static_cast<uint64_t>(1));
    for (int16_t i = 0; i < 3; ++i) {
      run(inputs, outputs, resolved_input_shapes);
    }
  }

  void read_config(const YAML::Node& yaml_config) {
    model_params_.read_model_node(yaml_config);
  }

 protected:
  std::unique_ptr<Ort::Session> session_ptr_ = nullptr;
  Ort::MemoryInfo* memory_info_ptr_ = nullptr;
  std::unique_ptr<Ort::RunOptions> run_options_ptr_ = nullptr;
  Ort::AllocatorWithDefaultOptions default_allocator_;

  std::vector<char*> model_input_names_;
  std::vector<char*> model_output_names_;
  std::vector<std::vector<int64_t>> model_input_shapes_;
  std::vector<std::vector<int64_t>> model_output_shapes_;
  // For each tensor: index of the only dynamic dim (-1). If tensor is static -> -1.
  // This is a readability optimization based on contract: only time is dynamic.
  std::vector<int32_t> time_axis_in_;

  spench::DenoiseModelParams model_params_;
};

}  // namespace onnx
}  // namespace util
