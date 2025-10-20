#pragma once

#ifdef BUILD_TFLITE_ENGINE
#include "spench/util/tflite/tflite_runner.h"
#else
#include "spench/util/onnx/denoise_onnx_runner.h"
#include "spench/util/onnx/dereverb_onnx_runner.h"
#endif
#include <vector>
#include <string>
#include <memory>

#include "spench/src/spench_task.h"
#include "spench/src/spench_context.h"
#include "spench/src/spench_config_reader.h"

namespace spench {

class InferenceRunner {
 public:
  typedef enum class ModelType {
    DENOISER_BEST_QUALITY = 0,
    DENOISER_BEST_SPEED = 1,
    DEREVERBERATOR_BASE = 2
  } ModelTypeEnum;
  InferenceRunner() = delete;
  InferenceRunner& operator=(const InferenceRunner&) = delete;
  InferenceRunner(const InferenceRunner&) = delete;

  InferenceRunner(std::shared_ptr<spench::NetworkConfig> config,
                  const std::string& password,
                  const std::string& checksum,
                  uint32_t inference_engine_threads,
                  bool gpu);
  ~InferenceRunner();

  void Infer(spench::SpenchTask* denoise_task);

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
      return ModelType::DENOISER_BEST_QUALITY;
    } else if (type == "best_speed") {
      return ModelType::DENOISER_BEST_SPEED;
    } else if (type == "remove_reverb") {
      return ModelType::DEREVERBERATOR_BASE;
    }
    throw std::invalid_argument("Model type must be in [\"best_quality\", \"best_speed\", \"remove_reverb\"]");
  }
};

}  // namespace spench
