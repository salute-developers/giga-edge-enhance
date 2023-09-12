#pragma once

#ifdef BUILD_TFLITE_ENGINE
#include "spench/util/tflite/tflite_runner.h"
#else
#include "spench/util/onnx/onnx_runner.h"
#endif
#include <vector>
#include <string>
#include <memory>

#include "spench/src/task.h"
#include "spench/src/denoise_context.h"

namespace spench {

class InferenceRunner {
 public:
  enum class ModelType {
    BEST_QUALITY = 0,
    BEST_SPEED = 1,
  };
  InferenceRunner() = delete;
  InferenceRunner& operator=(const InferenceRunner&) = delete;
  InferenceRunner(const InferenceRunner&) = delete;

  InferenceRunner(const YAML::Node& yaml_config,
                  const std::string& model_type,
                  const std::string& password,
                  const std::string& checksum,
                  uint32_t inference_engine_threads,
                  bool gpu);
  ~InferenceRunner();

  void Infer(spench::DenoiseTask* denoise_task);

  const ModelType GetModelType() {return m_model_type;}
  uint32_t GetModelFutureContextLen() {
    return m_engine_runner_ptr->GetFutureContextLen();
  }

 protected:
#ifdef BUILD_TFLITE_ENGINE
  std::unique_ptr<util::tf_lite::BaseTfliteRunner> m_engine_runner_ptr = nullptr;
#else
  std::unique_ptr<util::onnx::BaseOnnxRunner> m_engine_runner_ptr = nullptr;
#endif
  ModelType m_model_type;

 private:
  ModelType StringToModelTypeEnum(const std::string& type) {
    if (type == "best_quality") {
      return ModelType::BEST_QUALITY;
    }
    if (type == "best_speed") {
      return ModelType::BEST_SPEED;
    }
    throw std::invalid_argument("Model type must be in [\"best_quality\", \"best_speed\"]");
  }
};

}  // namespace spench
