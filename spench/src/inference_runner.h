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
    denoiser_best_quality = 0,
    denoiser_best_speed = 1,
    denoiser_best_speed_new = 2,
    dereverberator_base = 3
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

  void infer(spench::SpenchTask* denoise_task);

  const ModelType get_model_type() {return model_type_;}
  uint32_t get_model_future_context_len() {
    return engine_runner_ptr_->get_future_context_len();
  }

 protected:
#ifdef BUILD_TFLITE_ENGINE
  std::unique_ptr<util::tf_lite::BaseTfliteRunner> engine_runner_ptr_ = nullptr;
#else
  std::unique_ptr<util::onnx::BaseOnnxRunner> engine_runner_ptr_ = nullptr;
#endif
  ModelType model_type_;

 private:
  ModelType string_to_model_type_enum(const std::string& type) {
    if (type == "best_quality") {
      return ModelType::denoiser_best_quality;
    } else if (type == "best_speed") {
      return ModelType::denoiser_best_speed;
    } else if (type == "best_speed_new") {
      return ModelType::denoiser_best_speed_new;
    } else if (type == "remove_reverb") {
      return ModelType::dereverberator_base;
    }
    throw std::invalid_argument("Model type must be in [\"best_quality\", \"best_speed\", \"remove_reverb\"]");
  }
};

}  // namespace spench
