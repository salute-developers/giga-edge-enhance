#include "src/handler/acoustic_echo_cancelation_handler.h"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "src/logger.h"

namespace giga_edge::enhance {

AudioEchoCancelationHandler::AudioEchoCancelationHandler() : BaseAudioHandler(std::make_shared<AecConfig>()) {
  auto ret_code = restart();  // run do_start() after derived members are live
  if (ret_code != GigaEdgeEnhanceRetCode::SUCCESS) {
    spdlog::error("Failed to initialize EchoCanceler!  ret_code: {}", std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Failed to initialize EchoCanceler!  ret_code: " +
                             std::to_string(static_cast<int32_t>(ret_code)));
  }
}

AudioEchoCancelationHandler::AudioEchoCancelationHandler(std::shared_ptr<AecConfig> config)
    : BaseAudioHandler(std::move(config)) {
  auto ret_code = restart();
  if (ret_code != GigaEdgeEnhanceRetCode::SUCCESS) {
    spdlog::error("Failed to initialize EchoCanceler!  ret_code: {}", std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Failed to initialize EchoCanceler!  ret_code: " +
                             std::to_string(static_cast<int32_t>(ret_code)));
  }
}

GigaEdgeEnhanceRetCode AudioEchoCancelationHandler::do_start() {
  try {
    auto* aec = dynamic_cast<giga_edge::enhance::AecConfig*>(config_.get());
    if (aec == nullptr) {
      spdlog::error("AEC config reader type mismatch");
      return GigaEdgeEnhanceRetCode::ERR_CONF_FILE_PROBLEM;
    }

    canceler_ = util::echo::EchoCancelerBuilder()
                    .set_sample_rate(aec->sample_rate())
                    .set_filter_len(aec->filter_len())
                    .set_frame_len(aec->frame_len())
                    .set_type(aec->type())
                    .set_residual_echo_suppression(aec->residual_echo_suppression_enabled())
                    .set_delay_compensator(aec->delay_compensator_enabled())
                    .set_echo_premute(static_cast<float>(aec->echo_premute()) / 1000.0F)
                    .create();

    if (!canceler_) {
      spdlog::error("Failed to create EchoCanceler");
      return GigaEdgeEnhanceRetCode::ERR_CONF_FILE_PROBLEM;
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return GigaEdgeEnhanceRetCode::ERR_CONF_FILE_PROBLEM;
  } catch (...) {
    return GigaEdgeEnhanceRetCode::ERR_CONF_FILE_PROBLEM;
  }

  return GigaEdgeEnhanceRetCode::SUCCESS;
}

void AudioEchoCancelationHandler::do_stop() { canceler_.reset(); }

const uint32_t AudioEchoCancelationHandler::get_sample_rate() {
  auto* aec = dynamic_cast<AecConfig*>(config_.get());
  return aec ? aec->sample_rate() : 0;
}

const uint16_t AudioEchoCancelationHandler::get_filter_length() {
  auto* aec = dynamic_cast<AecConfig*>(config_.get());
  return aec ? aec->filter_len() : 0;
}

const uint16_t AudioEchoCancelationHandler::get_frame_length() {
  auto* aec = dynamic_cast<AecConfig*>(config_.get());
  return aec ? aec->frame_len() : 0;
}

void AudioEchoCancelationHandler::process(const std::vector<float>& mic, const std::vector<float>& lbk,
                                          std::vector<float>& out) {
  if (!canceler_) {
    throw std::runtime_error("EchoCanceler is not initialized!");
  }
  canceler_->process(mic, lbk, out);
}

GigaEdgeEnhanceRetCode AudioEchoCancelationHandler::process_frame(struct giga_edge::enhance::Context<float>*,
                                                                  const std::vector<float>& mic_frame,
                                                                  const std::vector<float>& lbk_frame,
                                                                  std::vector<float>& out) {
  try {
    if (!canceler_) {
      spdlog::error("EchoCanceler is not initialized!");
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    canceler_->process(mic_frame, lbk_frame, out);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
}

}  // namespace giga_edge::enhance
