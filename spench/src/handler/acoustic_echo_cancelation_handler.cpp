#include <algorithm>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include "spench/src/handler/acoustic_echo_cancelation_handler.h"
#include "spench/src/spench_logger.h"

namespace spench {

AudioEchoCancelationHandler::AudioEchoCancelationHandler()
    : BaseAudioHandler(std::make_shared<AecConfig>()) {
  auto ret_code = Restart();  // run DoStart() after derived members are live
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Failed to initialize EchoCanceler!  ret_code: {}",
                                        std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Failed to initialize EchoCanceler!  ret_code: " +
                                        std::to_string(static_cast<int32_t>(ret_code)));
  }
}

AudioEchoCancelationHandler::AudioEchoCancelationHandler(std::shared_ptr<AecConfig> config)
    : BaseAudioHandler(std::move(config)) {
  auto ret_code = Restart();
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Failed to initialize EchoCanceler!  ret_code: {}",
                                        std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Failed to initialize EchoCanceler!  ret_code: " +
                                        std::to_string(static_cast<int32_t>(ret_code)));
  }
}

SpenchRetCode AudioEchoCancelationHandler::DoStart() {
  try {
    auto* aec = dynamic_cast<spench::AecConfig*>(m_config.get());
    if (aec == nullptr) {
      spdlog::error("AEC config reader type mismatch");
      return SpenchRetCode::ERR_CONF_FILE_PROBLEM;
    }

    m_canceler = util::echo::EchoCancelerBuilder()
                     .SetSampleRate(aec->sampleRate())
                     .SetFilterLen(aec->filterLen())
                     .SetFrameLen(aec->frameLen())
                     .SetType(aec->type())
                     .SetResidualEchoSuppression(aec->residualEchoSuppressionEnabled())
                     .SetDelayCompensator(aec->delayCompensatorEnabled())
                     .Create();

    if (!m_canceler) {
      spdlog::error("Failed to create EchoCanceler");
      return SpenchRetCode::ERR_CONF_FILE_PROBLEM;
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return SpenchRetCode::ERR_CONF_FILE_PROBLEM;
  } catch (...) {
    return SpenchRetCode::ERR_CONF_FILE_PROBLEM;
  }

  return SpenchRetCode::SUCCESS;
}

void AudioEchoCancelationHandler::DoStop() {
  m_canceler.reset();
}

const uint32_t AudioEchoCancelationHandler::GetSampleRate() {
  auto* aec = dynamic_cast<AecConfig*>(m_config.get());
  return aec ? aec->sampleRate() : 0;
}

const uint16_t AudioEchoCancelationHandler::GetFilterLength() {
  auto* aec = dynamic_cast<AecConfig*>(m_config.get());
  return aec ? aec->filterLen() : 0;
}

const uint16_t AudioEchoCancelationHandler::GetFrameLength() {
  auto* aec = dynamic_cast<AecConfig*>(m_config.get());
  return aec ? aec->frameLen() : 0;
}

void AudioEchoCancelationHandler::Process(const std::vector<float>& mic,
                                          const std::vector<float>& lbk,
                                          std::vector<float>& out) {
  if (!m_canceler) {
    throw std::runtime_error("EchoCanceler is not initialized!");
  }
  m_canceler->Process(mic, lbk, out);
}

SpenchRetCode AudioEchoCancelationHandler::ProcessFrame(struct spench::SpenchContext<float>*,
                                                        const std::vector<float>& mic_frame,
                                                        const std::vector<float>& lbk_frame,
                                                        std::vector<float>& out) {
  try {
    if (!m_canceler) {
      spdlog::error("EchoCanceler is not initialized!");
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    m_canceler->Process(mic_frame, lbk_frame, out);
    return SpenchRetCode::SUCCESS;
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
}

}  // namespace spench
