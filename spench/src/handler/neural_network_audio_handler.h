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
  std::unique_ptr<spench::FeatureExtractorRunner> feature_extractor_runner_ptr_;
  std::unique_ptr<spench::InferenceRunner> inference_runner_ptr_;
  std::string enc_password_;
  std::string lic_checksum_;
  bool skip_zero_chunk_ = false;

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

  uint32_t get_engine_threads_num();
  uint64_t get_model_sample_rate();
  uint64_t get_input_frame_size();

  virtual SpenchContext<float>* create_context(uint32_t input_sample_rate = 0,
                                              uint32_t output_sample_rate = 0) = 0;
  virtual void destroy_context(SpenchContext<float>** ctx) = 0;

  virtual SpenchRetCode finalize_inference(struct spench::SpenchContext<float>* ctx,
                                          std::vector<float>& enhanced_audio_frame) = 0;

  virtual StatInfo get_stat_info(const struct spench::SpenchContext<float>* ctx) = 0;
 protected:
  // Startup/shutdown for NN resources.
  SpenchRetCode do_start() override;
  void do_stop() override;

  // Runners moved from Base.
  SpenchRetCode launch_feature_extractor_runner();
  SpenchRetCode launch_inference_runner();

  // fill_in_data_for_processing moved here; still abstract for final handlers.
  virtual void fill_in_data_for_processing(std::unique_ptr<spench::SpenchTask>& task_ptr,
                                       std::vector<float>& input) = 0;

  // Still abstract here; concrete handlers implement the algorithm.
  virtual SpenchRetCode infer_frame(std::unique_ptr<spench::SpenchTask>& task_ptr) = 0;

  bool is_zeroed_vector(const std::vector<float>& v, float epsilon = 0.0f) noexcept;
};

namespace tools {
void PrepareFirstInputFrame(const std::vector<float>& current_chunk,
                            std::vector<float>& frame, uint32_t frame_size);
}  // namespace tools

}  // namespace spench
