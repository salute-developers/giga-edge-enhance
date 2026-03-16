#include "spench/src/inference_runner.h"

namespace spench {

InferenceRunner::InferenceRunner(std::shared_ptr<spench::NetworkConfig> config,
                                const std::string& password,
                                const std::string& checksum,
                                uint32_t inference_engine_threads,
                                bool gpu) {
  m_model_type = StringToModelTypeEnum(config->params()["inference"]["model_type"].as<std::string>(""));
#ifdef BUILD_TFLITE_ENGINE
  auto model_config = config->params()["model"];

  if (m_model_type == ModelType::DENOISER_BEST_QUALITY) {
    m_engine_runner_ptr =
                    std::make_unique<util::tf_lite::SpenchQTfliteRunner>(model_config,
                                                                         password,
                                                                         checksum,
                                                                         inference_engine_threads,
                                                                         gpu);
  } else if (m_model_type == ModelType::DENOISER_BEST_SPEED) {
    m_engine_runner_ptr =
                    std::make_unique<util::tf_lite::SpenchFTfliteRunner>(model_config,
                                                                         password,
                                                                         checksum,
                                                                         inference_engine_threads,
                                                                         gpu);
  }
#else
  if (m_model_type == ModelType::DENOISER_BEST_QUALITY) {
    m_engine_runner_ptr = std::make_unique<util::onnx::SpenchQOnnxRunner>(config,
                                                                          password,
                                                                          checksum,
                                                                          inference_engine_threads,
                                                                          gpu);
  } else if (m_model_type == ModelType::DENOISER_BEST_SPEED) {
    m_engine_runner_ptr = std::make_unique<util::onnx::SpenchFOnnxRunner>(config,
                                                                          password,
                                                                          checksum,
                                                                          inference_engine_threads,
                                                                          gpu);
  } else if (m_model_type == ModelType::DENOISER_BEST_SPEED_NEW) {
    m_engine_runner_ptr = std::make_unique<util::onnx::SpenchSFOnnxRunner>(config,
                                                                           password,
                                                                           checksum,
                                                                           inference_engine_threads,
                                                                           gpu);
  } else {
    m_engine_runner_ptr =
                    std::make_unique<util::onnx::SpenchDereverbOnnxRunner>(config,
                                                                           password,
                                                                           checksum,
                                                                           inference_engine_threads,
                                                                           gpu);
  }
#endif
}

InferenceRunner::~InferenceRunner() {
  m_engine_runner_ptr.reset();
}

void InferenceRunner::Infer(spench::SpenchTask* denoise_task) {
  if (denoise_task) {
    m_engine_runner_ptr->Infer(denoise_task);
  } else {
    throw("Task data or context are not initialized!");
  }
}

}  // namespace spench
