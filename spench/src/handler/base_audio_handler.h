#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "spench/src/spench_context.h"
#include "spench/src/spench_task.h"
#include "spench/src/spench_config_reader.h"
#include "spench/errors.h"
#include "spench/stream_stat.h"

namespace spench {

class BaseAudioHandler {
 protected:
  std::unique_ptr<spench::ConfigReader> m_configReader;

  // ToDo(artsokol): remove and use reader directly
  const YAML::Node& params() { return m_configReader->params(); }

 public:
  BaseAudioHandler() = delete;
  BaseAudioHandler& operator=(const BaseAudioHandler&) = delete;
  BaseAudioHandler(const BaseAudioHandler&) = delete;

  explicit BaseAudioHandler(std::unique_ptr<ConfigReader> reader);
  BaseAudioHandler(BaseAudioHandler&& orig_handler) noexcept;

  virtual ~BaseAudioHandler();

  SpenchRetCode ReloadConfig() { return Restart(); }

  virtual SpenchRetCode ProcessFrame(struct spench::SpenchContext<float>* ctx,
                                     const std::vector<float>& input_audio_frame,
                                     std::vector<float>& enhanced_audio_frame);

  virtual SpenchRetCode ProcessFrame(struct spench::SpenchContext<float>* ctx,
                                     const std::vector<float>& mic_frame,
                                     const std::vector<float>& lbk_frame,
                                     std::vector<float>& out);

 protected:
  // Lifecycle owned by Base, implemented by subclasses.
  SpenchRetCode Start();
  void Stop();
  SpenchRetCode Restart();

  // Subclass hooks.
  virtual SpenchRetCode DoStart();   // default no-op
  virtual void DoStop();             // default no-op
};

}  // namespace spench
