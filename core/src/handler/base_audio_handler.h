#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "enhance/errors.h"
#include "enhance/stream_stat.h"
#include "src/config_reader.h"
#include "src/context.h"
#include "src/task.h"

namespace giga_edge::enhance {

class BaseAudioHandler {
 protected:
  std::shared_ptr<giga_edge::enhance::PipelineConfig> config_;

  // ToDo(artsokol): remove and use config directly
  const YAML::Node& params() { return config_->params(); }

 public:
  BaseAudioHandler() = delete;
  BaseAudioHandler& operator=(const BaseAudioHandler&) = delete;
  BaseAudioHandler(const BaseAudioHandler&) = delete;

  explicit BaseAudioHandler(std::shared_ptr<PipelineConfig> config);
  BaseAudioHandler(BaseAudioHandler&& orig_handler) noexcept;

  virtual ~BaseAudioHandler();

  GigaEdgeEnhanceRetCode reload_config() { return restart(); }

  virtual GigaEdgeEnhanceRetCode process_frame(struct giga_edge::enhance::Context<float>* ctx,
                                               const std::vector<float>& input_audio_frame,
                                               std::vector<float>& enhanced_audio_frame);

  virtual GigaEdgeEnhanceRetCode process_frame(struct giga_edge::enhance::Context<float>* ctx,
                                               const std::vector<float>& mic_frame, const std::vector<float>& lbk_frame,
                                               std::vector<float>& out);

 protected:
  // Lifecycle owned by Base, implemented by subclasses.
  GigaEdgeEnhanceRetCode start();
  void stop();
  GigaEdgeEnhanceRetCode restart();

  // Subclass hooks.
  virtual GigaEdgeEnhanceRetCode do_start();  // default no-op
  virtual void do_stop();                  // default no-op
};

}  // namespace giga_edge::enhance
