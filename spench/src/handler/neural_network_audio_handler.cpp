#include "spench/src/handler/neural_network_audio_handler.h"

#include <algorithm>
#include <exception>
#include <spdlog/spdlog.h>
#include <spench/src/spench_config_reader.h>
#include "spench/src/spench_logger.h"

namespace spench {

NeuralNetworkAudioHandler::NeuralNetworkAudioHandler(std::shared_ptr<NetworkConfig> config)
    : NeuralNetworkAudioHandler(std::move(config), "", "") {}

NeuralNetworkAudioHandler::NeuralNetworkAudioHandler(std::shared_ptr<NetworkConfig> config,
                                                     const std::string& password,
                                                     const std::string& checksum)
    : BaseAudioHandler(std::move(config)),
      m_feature_extractor_runner_ptr(nullptr),
      m_inference_runner_ptr(nullptr),
      m_enc_password(password),
      m_lic_checksum(checksum) {
  // Ensure DoStart() runs after derived members are initialized.
  SpenchRetCode ret_code = Restart();
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Due to errors a neural network handler module could not be created!  {}",
                                        std::to_string(static_cast<int32_t>(ret_code)));
    throw("Due to errors a neural network handler module could not be created! ret_code: " +
                                        std::to_string(static_cast<int32_t>(ret_code)));
    spdlog::error("A neural network handler could not be created! {}",
                  std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("A neural network handler module creation failed. ret_code=" +
                             std::to_string(static_cast<int32_t>(ret_code)));
  }
  m_skip_zero_chunk = dynamic_cast<spench::NetworkConfig*>(m_config.get())->skipZeroedChunks();
}

NeuralNetworkAudioHandler::NeuralNetworkAudioHandler(NeuralNetworkAudioHandler&& orig) noexcept
    : BaseAudioHandler(std::move(orig)),
      m_feature_extractor_runner_ptr(std::move(orig.m_feature_extractor_runner_ptr)),
      m_inference_runner_ptr(std::move(orig.m_inference_runner_ptr)),
      m_enc_password(std::move(orig.m_enc_password)),
      m_lic_checksum(std::move(orig.m_lic_checksum)) {}

NeuralNetworkAudioHandler::~NeuralNetworkAudioHandler() = default;

uint32_t NeuralNetworkAudioHandler::GetEngineThreadsNum() {
  return std::dynamic_pointer_cast<spench::NetworkConfig>(m_config)->numThreads();
}

uint64_t NeuralNetworkAudioHandler::GetModelSampleRate() {
  return m_feature_extractor_runner_ptr->GetSamplingFreq();
}

uint64_t NeuralNetworkAudioHandler::GetInputFrameSize() {
  return m_feature_extractor_runner_ptr->GetFrameHopSizeSamples();
}

SpenchRetCode NeuralNetworkAudioHandler::DoStart() {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;

  ret_code = LaunchFeatureExtractorRunner();
  if (ret_code != SpenchRetCode::SUCCESS) {
    return ret_code;
  }
  ret_code = LaunchInferenceRunner();
  return ret_code;
}

void NeuralNetworkAudioHandler::DoStop() {
  m_feature_extractor_runner_ptr.reset();
  m_inference_runner_ptr.reset();
}

SpenchRetCode NeuralNetworkAudioHandler::LaunchFeatureExtractorRunner() {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  try {
    if (!m_feature_extractor_runner_ptr) {
      m_feature_extractor_runner_ptr = std::unique_ptr<spench::FeatureExtractorRunner>(
          new spench::FeatureExtractorRunner(std::dynamic_pointer_cast<spench::NetworkConfig>(m_config)));
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    ret_code = SpenchRetCode::ERR_MODEL_START_PROBLEM;
  }
  return ret_code;
}

SpenchRetCode NeuralNetworkAudioHandler::LaunchInferenceRunner() {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  const uint32_t inference_engine_threads = GetEngineThreadsNum();

  try {
    if (!m_inference_runner_ptr) {
      m_inference_runner_ptr = std::unique_ptr<spench::InferenceRunner>(
        new spench::InferenceRunner(std::dynamic_pointer_cast<spench::NetworkConfig>(m_config),
                                    m_enc_password,
                                    m_lic_checksum,
                                    inference_engine_threads,
                                    false));
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    ret_code = SpenchRetCode::ERR_MODEL_START_PROBLEM;
  }
  return ret_code;
}

bool NeuralNetworkAudioHandler::IsZeroedVector(const std::vector<float>& v, float eps) noexcept {
  const size_t n = v.size();
  for (size_t i = 0; i < n; ++i) {
    float ax = std::fabs(v[i]);
    if (ax > eps || !std::isfinite(ax)) {
      return false;
    }
  }
  return true;
}

namespace tools {

void PrepareFirstInputFrame(const std::vector<float>& current_chunk,
                            std::vector<float>& frame,
                            uint32_t frame_size) {
  if (frame.size() != 0) {
    spdlog::error("Zero length frame should be provided as output argument");
    return;
  }
  frame.reserve(1 + frame_size + (current_chunk.size() - 1));
  frame.emplace_back(0.0f);

  const auto first_begin = current_chunk.begin();
  const auto first_end   = first_begin + static_cast<std::ptrdiff_t>(frame_size);

  std::reverse_copy(first_begin, first_end, std::back_inserter(frame));
  // keep the original "skip the very first sample" behavior to avoid boundary duplication
  frame.insert(frame.end(), current_chunk.begin() + 1, current_chunk.end());
}

}  // namespace tools

}  // namespace spench
