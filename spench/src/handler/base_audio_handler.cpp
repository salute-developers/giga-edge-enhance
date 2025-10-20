#include "spench/src/handler/base_audio_handler.h"

#include <utility>
#include <spdlog/spdlog.h>

namespace spench {

SpenchRetCode BaseAudioHandler::ProcessFrame(struct spench::SpenchContext<float>*,
                                             const std::vector<float>&,
                                             std::vector<float>&) {
  return SpenchRetCode::ERR_NOT_IMPLEMENTED;
}

SpenchRetCode BaseAudioHandler::ProcessFrame(struct spench::SpenchContext<float>* ctx,
                                             const std::vector<float>& mic_frame,
                                             const std::vector<float>& lbk_frame,
                                             std::vector<float>& out) {
  // Default: ignore lbk and call single-input version.
  // Derived classes that need lbk (AEC) will override this.
  (void)lbk_frame;
  return ProcessFrame(ctx, mic_frame, out);
}

BaseAudioHandler::BaseAudioHandler(std::shared_ptr<PipelineConfig> config)
    : m_config(std::move(config)) {
  SpenchRetCode ret_code = Start();
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Handler could not be created! {}",
                  std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Handler creation failed. ret_code=" +
                             std::to_string(static_cast<int32_t>(ret_code)));
  }
}

BaseAudioHandler::BaseAudioHandler(BaseAudioHandler&& orig_handler) noexcept
    : m_config(std::move(orig_handler.m_config)) {}

BaseAudioHandler::~BaseAudioHandler() { Stop(); }

SpenchRetCode BaseAudioHandler::Start() {
  // Common pre-start steps can go here; leave config loading to subclasses if needed.
  return DoStart();
}

void BaseAudioHandler::Stop() { DoStop(); }

SpenchRetCode BaseAudioHandler::Restart() {
  Stop();
  return Start();
}

SpenchRetCode BaseAudioHandler::DoStart() { return SpenchRetCode::SUCCESS; }

void BaseAudioHandler::DoStop() {}

}  // namespace spench
