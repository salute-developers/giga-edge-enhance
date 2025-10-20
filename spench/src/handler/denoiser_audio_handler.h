#pragma once

#include "spench/src/handler/neural_network_audio_handler.h"

#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "spench/src/denoiser_model_adviser.h"
namespace spench {

class DenoiserAudioHandler : public NeuralNetworkAudioHandler {
 public:
  DenoiserAudioHandler() = delete;
  DenoiserAudioHandler& operator=(const DenoiserAudioHandler&) = delete;
  DenoiserAudioHandler(const DenoiserAudioHandler&) = delete;

  explicit DenoiserAudioHandler(std::shared_ptr<DenoiserConfig> config);

  DenoiserAudioHandler(std::shared_ptr<DenoiserConfig> config,
                       const std::string& password,
                       const std::string& checksum);

  SpenchContext<float>* CreateContext(uint32_t input_sample_rate = 0,
                                      uint32_t output_sample_rate = 0) override;

  void DestroyContext(SpenchContext<float>** ctx) override {
    if (*ctx != nullptr) {
      DestroyAudioStreams(*ctx);
      delete *ctx;
    }
    *ctx = nullptr;
  }

  ~DenoiserAudioHandler() override;

  SpenchRetCode ProcessFrame(struct spench::SpenchContext<float>* ctx,
                             const std::vector<float>& input_audio_frame,
                             std::vector<float>& enhanced_audio_frame) override;

  SpenchRetCode FinalizeInference(struct spench::SpenchContext<float>* ctx,
                                  std::vector<float>& enhanced_audio_frame) override;

  StatInfo GetStatInfo(const struct spench::SpenchContext<float>* ctx) override;

  spench::DenoiserModelAdviser<float>*       adviser()       { return m_adviser.get(); }
  const spench::DenoiserModelAdviser<float>* adviser() const { return m_adviser.get(); }

 private:
  std::unique_ptr<DenoiserModelAdviser<float>> m_adviser;
  SpenchRetCode InitAudioStreams(SpenchContext<float>* ctx,
                                 uint32_t input_sample_rate,
                                 uint32_t output_sample_rate);
  SpenchRetCode DestroyAudioStreams(SpenchContext<float>* ctx);
  SpenchRetCode InferFrame(std::unique_ptr<spench::SpenchTask>& task_ptr) override;
  void          FillInDataForProcessing(std::unique_ptr<spench::SpenchTask>& task_ptr,
                                        std::vector<float>& input) override;
};
}  // namespace spench
