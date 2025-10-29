#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "spench/src/handler/base_audio_handler.h"
#include "spench/src/feature_extractor_runner.h"
#include "spench/src/inference_runner.h"

namespace spench {

class NeuralNetworkAudioHandler : public BaseAudioHandler {
 protected:
  std::unique_ptr<spench::FeatureExtractorRunner> m_feature_extractor_runner_ptr;
  std::unique_ptr<spench::InferenceRunner> m_inference_runner_ptr;
  std::string m_enc_password;
  std::string m_lic_checksum;
  bool m_skip_zero_chunk = false;

 public:
  NeuralNetworkAudioHandler() = delete;
  NeuralNetworkAudioHandler& operator=(const NeuralNetworkAudioHandler&) = delete;
  NeuralNetworkAudioHandler(const NeuralNetworkAudioHandler&) = delete;

  explicit NeuralNetworkAudioHandler(std::shared_ptr<NetworkConfig> config);
  NeuralNetworkAudioHandler(std::shared_ptr<NetworkConfig> config,
                            const std::string& password,
                            const std::string& checksum);
  NeuralNetworkAudioHandler(NeuralNetworkAudioHandler&& orig) noexcept;

  ~NeuralNetworkAudioHandler() override;

  uint32_t GetEngineThreadsNum();
  uint64_t GetModelSampleRate();
  uint64_t GetInputFrameSize();

  virtual SpenchContext<float>* CreateContext(uint32_t input_sample_rate = 0,
                                              uint32_t output_sample_rate = 0) = 0;
  virtual void DestroyContext(SpenchContext<float>** ctx) = 0;

  virtual SpenchRetCode FinalizeInference(struct spench::SpenchContext<float>* ctx,
                                          std::vector<float>& enhanced_audio_frame) = 0;

  virtual StatInfo GetStatInfo(const struct spench::SpenchContext<float>* ctx) = 0;
 protected:
  // Startup/shutdown for NN resources.
  SpenchRetCode DoStart() override;
  void DoStop() override;

  // Runners moved from Base.
  SpenchRetCode LaunchFeatureExtractorRunner();
  SpenchRetCode LaunchInferenceRunner();

  // FillInDataForProcessing moved here; still abstract for final handlers.
  virtual void FillInDataForProcessing(std::unique_ptr<spench::SpenchTask>& task_ptr,
                                       std::vector<float>& input) = 0;

  // Still abstract here; concrete handlers implement the algorithm.
  virtual SpenchRetCode InferFrame(std::unique_ptr<spench::SpenchTask>& task_ptr) = 0;

  bool IsZeroedVector(const std::vector<float>& v, float epsilon = 0.0f) noexcept;
};

namespace tools {
void PrepareFirstInputFrame(const std::vector<float>& current_chunk,
                            std::vector<float>& frame);
}  // namespace tools

}  // namespace spench
