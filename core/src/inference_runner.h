#pragma once

#ifdef BUILD_TFLITE_ENGINE
#include "util/tflite/tflite_runner.h"
#else
#include "util/onnx/denoise_onnx_runner.h"
#endif
#include <memory>
#include <string>
#include <vector>

#include "src/config_reader.h"
#include "src/context.h"
#include "src/task.h"

namespace gigaedge::denoiser {

class InferenceRunner {
 public:
  typedef enum class ModelType {
    denoiser_best_quality = 0,
    denoiser_best_speed = 1,
    denoiser_best_speed_new = 2
  } ModelTypeEnum;
  InferenceRunner() = delete;
  InferenceRunner& operator=(const InferenceRunner&) = delete;
  InferenceRunner(const InferenceRunner&) = delete;

  InferenceRunner(std::shared_ptr<gigaedge::denoiser::NetworkConfig> config, uint32_t inference_engine_threads,
                  bool gpu);
  ~InferenceRunner();

  void infer(gigaedge::denoiser::Task* denoise_task);

  ModelType get_model_type() const { return model_type_; }
  uint32_t get_model_future_context_len() const { return engine_runner_ptr_->get_future_context_len(); }

 protected:
#ifdef BUILD_TFLITE_ENGINE
  std::unique_ptr<::util::tf_lite::BaseTfliteRunner> engine_runner_ptr_ = nullptr;
#else
  std::unique_ptr<::util::onnx::BaseOnnxRunner> engine_runner_ptr_ = nullptr;
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
    }
    throw std::invalid_argument("Model type must be in [\"best_quality\", \"best_speed\", \"best_speed_new\"]");
  }
};

}  // namespace gigaedge::denoiser
