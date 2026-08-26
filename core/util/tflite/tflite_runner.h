#pragma once

#include <exception>
#include <memory>
#include <mutex>  // NOLINT
#include <numeric>
#include <string>
#include <vector>

#include "src/config_reader.h"
#include "src/context.h"
#include "src/task.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/signature_runner.h"


namespace util {
namespace tf_lite {

class TfliteValidationException : public std::runtime_error {
 public:
  explicit TfliteValidationException(const std::string& what) : std::runtime_error(what) {}
};

class BaseTfliteRunner {
 public:
  BaseTfliteRunner();
  BaseTfliteRunner& operator=(const BaseTfliteRunner&) = delete;
  BaseTfliteRunner(const BaseTfliteRunner&) = delete;

  BaseTfliteRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config, const uint32_t num_threads,
                   bool on_gpu = false);
  virtual ~BaseTfliteRunner();

  virtual void infer(gigaedge::denoiser::Task* denoise_task) = 0;
  // change two last arguments
  virtual void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                                      gigaedge::denoiser::Context<float>* denoise_context) = 0;
  template <typename T>
  void test_run(std::vector<std::vector<T>*>& inputs) {
    std::lock_guard<std::mutex> lock(model_run_mutex_);
    run(inputs);
  }
  virtual uint32_t get_future_context_len() { return model_params_.network_input_ctx_len; }

 protected:
  void read_config(const YAML::Node& yaml_config) { model_params_.read_model_node(yaml_config); }

  void init_model(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config, const uint32_t num_threads,
                  bool on_gpu);

  template <typename T>
  void run(std::vector<std::vector<T>*>& inputs, const std::vector<std::vector<int32_t>>& input_shapes);

  template <typename T>
  void run(std::vector<std::vector<T>*>& inputs) {
    run(inputs, model_input_shapes_);
  }

  void resize_if_needed_in_place(std::vector<float>& to_check, uint32_t shape);
  void resize_if_needed_in_place(std::valarray<float>& to_check, uint32_t shape);
  void init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t size);

  static int32_t get_single_dynamic_axis(const TfLiteTensor* tensor, const char* tensor_debug_name);
  static void set_time_dim_if_dynamic(std::vector<int32_t>& shape, int32_t time_axis, int32_t num_frames);
  static uint64_t shape_product(const std::vector<int32_t>& shape);

  void refresh_tensor_metadata();
  void resize_inputs_if_needed(const std::vector<std::vector<int32_t>>& input_shapes);

  template <typename T>
  void warm_up(std::vector<std::vector<T>*>& inputs) {
    for (int16_t i = 0; i < 3; ++i) {
      run(inputs);
    }
  }

 public:
  std::vector<uint64_t> input_tensor_sizes;
  std::vector<uint64_t> output_tensor_sizes;

 protected:
  std::mutex model_run_mutex_;
  std::unique_ptr<std::vector<char>> model_data_ = nullptr;
  std::unique_ptr<tflite::FlatBufferModel> model_ = nullptr;
  std::unique_ptr<tflite::Interpreter> interpreter_ = nullptr;
  std::vector<TfLiteTensor*> model_inputs_;
  std::vector<const TfLiteTensor*> model_outputs_;
  tflite::SignatureRunner* signature_runner_;
  gigaedge::denoiser::DenoiseModelParams model_params_;
  std::vector<std::vector<int32_t>> model_input_shapes_;
  std::vector<std::vector<int32_t>> model_output_shapes_;
  std::vector<int32_t> time_axis_in_;
  std::vector<std::string> model_input_names_;
  std::vector<std::string> model_output_names_;
};


class DenoiserFTfliteRunner : public BaseTfliteRunner {
 public:
  DenoiserFTfliteRunner() = delete;
  virtual ~DenoiserFTfliteRunner() = default;
  explicit DenoiserFTfliteRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                 uint32_t inference_engine_threads = 1, bool on_gpu = false);
  void infer(gigaedge::denoiser::Task* denoise_task);

 protected:
  template <typename T>
  void warm_up();
  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* context);
  template <typename T>
  void parse_results(gigaedge::denoiser::TaskData& data);
};

class DenoiserQTfliteRunner : public BaseTfliteRunner {
 public:
  DenoiserQTfliteRunner() = delete;
  virtual ~DenoiserQTfliteRunner() = default;
  explicit DenoiserQTfliteRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                 uint32_t inference_engine_threads = 1, bool on_gpu = false);
  void infer(gigaedge::denoiser::Task* denoise_task);

 protected:
  template <typename T>
  void warm_up();
  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* context);
  template <typename T>
  void parse_results(gigaedge::denoiser::TaskData& data);
};

}  // namespace tf_lite
}  // namespace util
