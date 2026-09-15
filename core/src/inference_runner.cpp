#include "src/inference_runner.h"

namespace gigaedge::denoiser {

InferenceRunner::InferenceRunner(std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                 uint32_t inference_engine_threads, bool gpu) {
  model_type_ = string_to_model_type_enum(config->params()["inference"]["model_type"].as<std::string>(""));
#ifdef BUILD_TFLITE_ENGINE
  if (model_type_ == ModelType::denoiser_best_quality) {
    engine_runner_ptr_ = std::make_unique<util::tf_lite::DenoiserQTfliteRunner>(config, inference_engine_threads, gpu);
  } else if (model_type_ == ModelType::denoiser_best_speed) {
    engine_runner_ptr_ = std::make_unique<util::tf_lite::DenoiserFTfliteRunner>(config, inference_engine_threads, gpu);
  }
#else
  if (model_type_ == ModelType::denoiser_best_quality) {
    engine_runner_ptr_ = std::make_unique<util::onnx::DenoiserQOnnxRunner>(config, inference_engine_threads, gpu);
  } else if (model_type_ == ModelType::denoiser_best_speed) {
    engine_runner_ptr_ = std::make_unique<util::onnx::DenoiserFOnnxRunner>(config, inference_engine_threads, gpu);
  } else if (model_type_ == ModelType::denoiser_best_speed_new) {
    engine_runner_ptr_ = std::make_unique<util::onnx::DenoiserSFOnnxRunner>(config, inference_engine_threads, gpu);
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
