#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "spench/errors.h"
#include "spench/stream_stat.h"
#include "src/config_reader.h"
#include "src/context.h"
#include "src/task.h"

namespace gigaedge::spench {

class BaseAudioHandler {
 protected:
  std::shared_ptr<gigaedge::spench::PipelineConfig> config_;

  // ToDo(artsokol): remove and use config directly
  const YAML::Node& params() { return config_->params(); }

 public:
  BaseAudioHandler() = delete;
  BaseAudioHandler& operator=(const BaseAudioHandler&) = delete;
  BaseAudioHandler(const BaseAudioHandler&) = delete;

  explicit BaseAudioHandler(std::shared_ptr<PipelineConfig> config);
  BaseAudioHandler(BaseAudioHandler&& orig_handler) noexcept;

  virtual ~BaseAudioHandler();

  GigaEdgeSpenchRetCode reload_config() { return restart(); }

  virtual GigaEdgeSpenchRetCode process_frame(struct gigaedge::spench::Context<float>* ctx,
                                              const std::vector<float>& input_audio_frame,
                                              std::vector<float>& enhanced_audio_frame);

  virtual GigaEdgeSpenchRetCode process_frame(struct gigaedge::spench::Context<float>* ctx,
                                              const std::vector<float>& mic_frame, const std::vector<float>& lbk_frame,
                                              std::vector<float>& out);

 protected:
  // Lifecycle owned by Base, implemented by subclasses.
  GigaEdgeSpenchRetCode start();
  void stop();
  GigaEdgeSpenchRetCode restart();

  // Subclass hooks.
  virtual GigaEdgeSpenchRetCode do_start();  // default no-op
  virtual void do_stop();                  // default no-op
};

}  // namespace gigaedge::spench
