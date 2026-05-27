#include <algorithm>
#include <spdlog/spdlog.h>
#include "spench/src/handler/acoustic_echo_cancelation_handler.h"
#include "spench/src/spench_logger.h"

namespace spench {

AudioEchoCancelationHandler::AudioEchoCancelationHandler()
    : BaseAudioHandler(std::make_shared<AecConfig>()) {
  auto ret_code = restart();  // run do_start() after derived members are live
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Failed to initialize EchoCanceler!  ret_code: {}",
                                        std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Failed to initialize EchoCanceler!  ret_code: " +
                                        std::to_string(static_cast<int32_t>(ret_code)));
  }
}

AudioEchoCancelationHandler::AudioEchoCancelationHandler(std::shared_ptr<AecConfig> config)
    : BaseAudioHandler(std::move(config)) {
  auto ret_code = restart();
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Failed to initialize EchoCanceler!  ret_code: {}",
                                        std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Failed to initialize EchoCanceler!  ret_code: " +
                                        std::to_string(static_cast<int32_t>(ret_code)));
  }
}

SpenchRetCode AudioEchoCancelationHandler::do_start() {
  try {
    auto* aec = dynamic_cast<spench::AecConfig*>(config_.get());
    if (aec == nullptr) {
      spdlog::error("AEC config reader type mismatch");
      return SpenchRetCode::ERR_CONF_FILE_PROBLEM;
    }

    canceler_ = util::echo::EchoCancelerBuilder()
                     .set_sample_rate(aec->sample_rate())
                     .set_filter_len(aec->filter_len())
                     .set_frame_len(aec->frame_len())
                     .set_type(aec->type())
                     .set_residual_echo_suppression(aec->residual_echo_suppression_enabled())
                     .set_delay_compensator(aec->delay_compensator_enabled())
                     .set_echo_premute(aec->echo_premute())
                     .create();

    if (!canceler_) {
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

void AudioEchoCancelationHandler::do_stop() {
  canceler_.reset();
}

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

void AudioEchoCancelationHandler::process(const std::vector<float>& mic,
                                          const std::vector<float>& lbk,
                                          std::vector<float>& out) {
  if (!canceler_) {
    throw std::runtime_error("EchoCanceler is not initialized!");
  }
  canceler_->process(mic, lbk, out);
}

SpenchRetCode AudioEchoCancelationHandler::process_frame(struct spench::SpenchContext<float>*,
                                                        const std::vector<float>& mic_frame,
                                                        const std::vector<float>& lbk_frame,
                                                        std::vector<float>& out) {
  try {
    if (!canceler_) {
      spdlog::error("EchoCanceler is not initialized!");
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    canceler_->process(mic_frame, lbk_frame, out);
    return SpenchRetCode::SUCCESS;
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
}

}  // namespace spench
