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

namespace gigaedge::spench {

class InferenceRunner {
 public:
  typedef enum class ModelType {
    // Half-band quantized discriminative model.
    denoiser_discr_hq_model = 0,
    // 48 kHz quality discriminative model.
    denoiser_discr_web_model = 1,
    denoiser_hybrid_web_model = 2
  } ModelTypeEnum;
  InferenceRunner() = delete;
  InferenceRunner& operator=(const InferenceRunner&) = delete;
  InferenceRunner(const InferenceRunner&) = delete;

  InferenceRunner(std::shared_ptr<gigaedge::spench::NetworkConfig> config, uint32_t inference_engine_threads, bool gpu);
  ~InferenceRunner();

  void infer(gigaedge::spench::Task* denoise_task);

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
    if (type == "denoiser_discr_hq_model") {
      return ModelType::denoiser_discr_hq_model;
    } else if (type == "denoiser_discr_web_model") {
      return ModelType::denoiser_discr_web_model;
    } else if (type == "denoiser_hybrid_web_model") {
      return ModelType::denoiser_hybrid_web_model;
    }
    throw std::invalid_argument(
        "Model type must be in [\"denoiser_discr_hq_model\", "
        "\"denoiser_discr_web_model\", \"denoiser_hybrid_web_model\"]");
  }
};

}  // namespace gigaedge::spench
