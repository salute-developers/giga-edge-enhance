#include "spench/src/inference_runner.h"

namespace spench {

InferenceRunner::InferenceRunner(std::shared_ptr<spench::NetworkConfig> config,
                                 uint32_t inference_engine_threads,
                                bool gpu) {
  model_type_ = string_to_model_type_enum(config->params()["inference"]["model_type"].as<std::string>(""));
#ifdef BUILD_TFLITE_ENGINE
  if (model_type_ == ModelType::denoiser_best_quality) {
      engine_runner_ptr_ =
                    std::make_unique<util::tf_lite::SpenchQTfliteRunner>(config,
                                                                           inference_engine_threads,
                                                                          gpu);
  } else if (model_type_ == ModelType::denoiser_best_speed) {
      engine_runner_ptr_ =
                    std::make_unique<util::tf_lite::SpenchFTfliteRunner>(config,
                                                                           inference_engine_threads,
                                                                          gpu);
  }
#else
  if (model_type_ == ModelType::denoiser_best_quality) {
    engine_runner_ptr_ = std::make_unique<util::onnx::SpenchQOnnxRunner>(config,
                                                                           inference_engine_threads,
                                                                          gpu);
  } else if (model_type_ == ModelType::denoiser_best_speed) {
    engine_runner_ptr_ = std::make_unique<util::onnx::SpenchFOnnxRunner>(config,
                                                                           inference_engine_threads,
                                                                          gpu);
  } else if (model_type_ == ModelType::denoiser_best_speed_new) {
    engine_runner_ptr_ = std::make_unique<util::onnx::SpenchSFOnnxRunner>(config,
                                                                             inference_engine_threads,
                                                                            gpu);
  }
#endif
  if (engine_runner_ptr_ == nullptr) {
    throw std::invalid_argument("Model type is not supported by the selected inference engine.");
  }
}

InferenceRunner::~InferenceRunner() {
  engine_runner_ptr_.reset();
}

void InferenceRunner::infer(spench::SpenchTask* denoise_task) {
  if (denoise_task == nullptr) {
    throw std::invalid_argument("Task data is not initialized.");
  }
  engine_runner_ptr_->infer(denoise_task);
}

}  // namespace spench
