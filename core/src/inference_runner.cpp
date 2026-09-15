#include "src/inference_runner.h"

namespace gigaedge::denoiser {

InferenceRunner::InferenceRunner(std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                 uint32_t inference_engine_threads, bool gpu) {
  model_type_ = string_to_model_type_enum(config->params()["inference"]["model_type"].as<std::string>(""));
#ifdef BUILD_TFLITE_ENGINE
  if (model_type_ == ModelType::denoiser_hybrid_web_model) {
    engine_runner_ptr_ =
        std::make_unique<util::tf_lite::DenoiserHybridTfliteRunner>(config, inference_engine_threads, gpu);
  } else if (model_type_ == ModelType::denoiser_discr_web_model) {
    engine_runner_ptr_ =
        std::make_unique<util::tf_lite::DenoiserDiscriminativeHqTfliteRunner>(config, inference_engine_threads, gpu);
  }
#else
  if (model_type_ == ModelType::denoiser_hybrid_web_model) {
    engine_runner_ptr_ = std::make_unique<util::onnx::DenoiserHybridOnnxRunner>(config, inference_engine_threads, gpu);
  } else if (model_type_ == ModelType::denoiser_discr_hq_model) {
    engine_runner_ptr_ =
        std::make_unique<util::onnx::DenoiserDiscriminativeNbOnnxRunner>(config, inference_engine_threads, gpu);
  } else if (model_type_ == ModelType::denoiser_discr_web_model) {
    engine_runner_ptr_ =
        std::make_unique<util::onnx::DenoiserDiscriminativeHqOnnxRunner>(config, inference_engine_threads, gpu);
  }
#endif
  if (engine_runner_ptr_ == nullptr) {
    throw std::invalid_argument("Model type is not supported by the selected inference engine.");
  }
}

InferenceRunner::~InferenceRunner() { engine_runner_ptr_.reset(); }

void InferenceRunner::infer(gigaedge::denoiser::Task* denoise_task) {
  if (denoise_task == nullptr) {
    throw std::invalid_argument("Task data is not initialized.");
  }
  engine_runner_ptr_->infer(denoise_task);
}

}  // namespace gigaedge::denoiser
