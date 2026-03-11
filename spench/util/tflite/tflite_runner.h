#pragma once

#include "contrib/tflite/tensorflow/lite/interpreter.h"
#include "contrib/tflite/tensorflow/lite/kernels/register.h"
#include "contrib/tflite/tensorflow/lite/model.h"
#include "contrib/tflite/tensorflow/lite/signature_runner.h"
#include "contrib/tflite/tensorflow/lite/kernels/internal/tensor_ctypes.h"

#include <vector>
#include <numeric>
#include <exception>
#include <string>
#include <memory>
#include <mutex> // NOLINT

#include "spench/src/spench_task.h"
#include "spench/src/spench_context.h"


namespace util {
namespace tf_lite {

class TfliteValidationException: public std::runtime_error {
 public:
  explicit TfliteValidationException(const std::string& what)
    : std::runtime_error(what) {}
};

class BaseTfliteRunner {
 public:
  BaseTfliteRunner();
  BaseTfliteRunner& operator=(const BaseTfliteRunner&) = delete;
  BaseTfliteRunner(const BaseTfliteRunner&) = delete;

  BaseTfliteRunner(std::vector<std::string>& input_names,
                  std::vector<std::string>& output_names,
                  const std::string& model_path,
                  const std::string& password,
                  const std::string& checksum,
                  const uint32_t num_threads,
                  bool on_gpu = false);
  virtual ~BaseTfliteRunner();

  virtual void Infer(spench::SpenchTask* denoise_task) = 0;
  // change two last arguments
  virtual void PrepareInputInPlace(std::vector<float>& mag,
                                   std::vector<std::vector<float>*>& inputs,
                                   spench::SpenchContext<float>* denoise_context) = 0;
  template<typename T>
  void _TestRun(std::vector<std::vector<T>*>& inputs) {Run(inputs);}
  virtual uint32_t GetFutureContextLen() {
    return m_model_params.m_network_input_ctx_len;
  }

 protected:
  void ReadConfig(const YAML::Node& yaml_config) {
    m_model_params.ReadModelNode(yaml_config);
  }

  void InitModel(std::vector<std::string>& input_names,
                 std::vector<std::string>& output_names,
                 const std::string& model_path,
                 const std::string& password,
                 const std::string& checksum,
                 const uint32_t num_threads, bool on_gpu);

  template<typename T>
  void Run(std::vector<std::vector<T>*>& inputs,
           const std::vector<std::vector<int32_t>>& input_shapes);

  template<typename T>
  void Run(std::vector<std::vector<T>*>& inputs) {
    Run(inputs, m_model_input_shapes);
  }

  void ResizeIfNeededInPlace(std::vector<float>& to_check, uint32_t shape);
  void ResizeIfNeededInPlace(std::valarray<float>& to_check, uint32_t shape);
  void InitOutputTensors(spench::SpenchTaskData& data, uint32_t size);

  static int32_t GetSingleDynamicAxis(const TfLiteTensor* tensor, const char* tensor_debug_name);
  static void SetTimeDimIfDynamic(std::vector<int32_t>& shape, int32_t time_axis, int32_t num_frames);
  static uint64_t ShapeProduct(const std::vector<int32_t>& shape);

  void RefreshTensorMetadata();
  void ResizeInputsIfNeeded(const std::vector<std::vector<int32_t>>& input_shapes);

  template<typename T>
  void WarmUp(std::vector<std::vector<T>*>& inputs) {
    for (int16_t i = 0; i < 3; ++i) {
      Run(inputs);
    }
  }

 public:
  std::vector<uint64_t> m_model_input_size;
  std::vector<uint64_t> m_model_output_size;

 protected:
  std::mutex m_model_run_mutex;
  std::unique_ptr<tflite::FlatBufferModel> m_model = nullptr;
  std::unique_ptr<tflite::Interpreter> m_interpreter = nullptr;
  std::unique_ptr<std::vector<char> > m_model_data = nullptr;
  std::vector<TfLiteTensor*> m_model_inputs;
  std::vector<const TfLiteTensor*> m_model_outputs;
  tflite::SignatureRunner* m_signature_runner;
  spench::DenoiseModelParams m_model_params;
  std::vector<std::vector<int32_t>> m_model_input_shapes;
  std::vector<std::vector<int32_t>> m_model_output_shapes;
  std::vector<int32_t> m_time_axis_in;
  std::vector<std::string> m_model_input_names;
  std::vector<std::string> m_model_output_names;
};


class SpenchFTfliteRunner: public BaseTfliteRunner {
 public:
  SpenchFTfliteRunner() = delete;
  virtual ~SpenchFTfliteRunner() = default;
  explicit SpenchFTfliteRunner(const YAML::Node& yaml_config,
                               const std::string& password,
                               const std::string& checksum,
                               uint32_t inference_engine_threads = 1,
                               bool on_gpu = false);
  void Infer(spench::SpenchTask* denoise_task);
 protected:
  template<typename T> void WarmUp();
  void PrepareInputInPlace(std::vector<float>& mag,
                           std::vector<std::vector<float>*>& inputs,
                           spench::SpenchContext<float>* context);
  template<typename T>
  void ParseResults(spench::SpenchTaskData& data);
};

class SpenchQTfliteRunner: public BaseTfliteRunner {
 public:
  SpenchQTfliteRunner() = delete;
  virtual ~SpenchQTfliteRunner() = default;
  explicit SpenchQTfliteRunner(const YAML::Node& yaml_config,
                               const std::string& password,
                               const std::string& checksum,
                               uint32_t inference_engine_threads = 1,
                               bool on_gpu = false);
  void Infer(spench::SpenchTask* denoise_task);
 protected:
  template<typename T> void WarmUp();
  void PrepareInputInPlace(std::vector<float>& mag,
                           std::vector<std::vector<float>*>& inputs,
                           spench::SpenchContext<float>* context);
  template<typename T>
  void ParseResults(spench::SpenchTaskData& data);
};

}  // namespace tf_lite
}  // namespace util
