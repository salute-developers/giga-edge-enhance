#include "spench/src/inference_runner.h"

namespace spench {

InferenceRunner::InferenceRunner(const YAML::Node& model_config,
                                const std::string& model_type,
                                const std::string& password,
                                const std::string& checksum,
                                uint32_t inference_engine_threads,
                                bool gpu) {
  m_model_type = StringToModelTypeEnum(model_type);
#ifdef BUILD_TFLITE_ENGINE
  m_engine_runner_ptr =
                (m_model_type == ModelType::BEST_QUALITY) ?
                  std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                  new util::tf_lite::SpenchQTfliteRunner(model_config,
                                                                         password,
                                                                         checksum,
                                                                         inference_engine_threads,
                                                                         gpu)):
                  std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                  new util::tf_lite::SpenchFTfliteRunner(model_config,
                                                                         password,
                                                                         checksum,
                                                                         inference_engine_threads,
                                                                         gpu));
#else
  m_engine_runner_ptr =
                (m_model_type == ModelType::BEST_QUALITY) ?
                  std::unique_ptr<util::onnx::BaseOnnxRunner>(
                                  new util::onnx::SpenchQOnnxRunner(model_config,
                                                                    password,
                                                                    checksum,
                                                                    inference_engine_threads,
                                                                    gpu)):
                  std::unique_ptr<util::onnx::BaseOnnxRunner>(
                                  new util::onnx::SpenchFOnnxRunner(model_config,
                                                                    password,
                                                                    checksum,
                                                                    inference_engine_threads,
                                                                    gpu));
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
