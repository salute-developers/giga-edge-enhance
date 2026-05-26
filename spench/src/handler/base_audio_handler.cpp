#include "spench/src/handler/base_audio_handler.h"

#include <utility>
#include <spdlog/spdlog.h>

namespace spench {

SpenchRetCode BaseAudioHandler::process_frame(struct spench::SpenchContext<float>*,
                                             const std::vector<float>&,
                                             std::vector<float>&) {
  return SpenchRetCode::ERR_NOT_IMPLEMENTED;
}

SpenchRetCode BaseAudioHandler::process_frame(struct spench::SpenchContext<float>* ctx,
                                             const std::vector<float>& mic_frame,
                                             const std::vector<float>& lbk_frame,
                                             std::vector<float>& out) {
  // Default: ignore lbk and call single-input version.
  // Derived classes that need lbk (AEC) will override this.
  (void)lbk_frame;
  return process_frame(ctx, mic_frame, out);
}

BaseAudioHandler::BaseAudioHandler(std::shared_ptr<PipelineConfig> config)
    : config_(std::move(config)) {
  SpenchRetCode ret_code = start();
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Handler could not be created! {}",
                  std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Handler creation failed. ret_code=" +
                             std::to_string(static_cast<int32_t>(ret_code)));
  }
}

BaseAudioHandler::BaseAudioHandler(BaseAudioHandler&& orig_handler) noexcept
    : config_(std::move(orig_handler.config_)) {}

BaseAudioHandler::~BaseAudioHandler() { stop(); }

SpenchRetCode BaseAudioHandler::start() {
  // Common pre-start steps can go here; leave config loading to subclasses if needed.
  return do_start();
}

void BaseAudioHandler::stop() { do_stop(); }

SpenchRetCode BaseAudioHandler::restart() {
  stop();
  return start();
}

SpenchRetCode BaseAudioHandler::do_start() { return SpenchRetCode::SUCCESS; }

void BaseAudioHandler::do_stop() {}

}  // namespace spench
