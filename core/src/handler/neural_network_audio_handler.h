#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/feature_extractor_runner.h"
#include "src/handler/base_audio_handler.h"
#include "src/inference_runner.h"

namespace gigaedge::denoiser {

class NeuralNetworkAudioHandler : public BaseAudioHandler {
 protected:
  std::unique_ptr<gigaedge::denoiser::FeatureExtractorRunner> feature_extractor_runner_ptr_;
  std::unique_ptr<gigaedge::denoiser::InferenceRunner> inference_runner_ptr_;
  bool skip_zero_chunk_ = false;

 public:
  NeuralNetworkAudioHandler() = delete;
  NeuralNetworkAudioHandler& operator=(const NeuralNetworkAudioHandler&) = delete;
  NeuralNetworkAudioHandler(const NeuralNetworkAudioHandler&) = delete;

  explicit NeuralNetworkAudioHandler(std::shared_ptr<NetworkConfig> config);
  NeuralNetworkAudioHandler(NeuralNetworkAudioHandler&& orig) noexcept;

  ~NeuralNetworkAudioHandler() override;

  uint32_t get_engine_threads_num();
  uint64_t get_model_sample_rate();
  uint64_t get_input_frame_size();

  virtual Context<float>* create_context(uint32_t input_sample_rate = 0, uint32_t output_sample_rate = 0) = 0;
  virtual void destroy_context(Context<float>** ctx) = 0;

  virtual GigaDenoiserRetCode finalize_inference(struct gigaedge::denoiser::Context<float>* ctx,
                                                 std::vector<float>& enhanced_audio_frame) = 0;

  virtual StatInfo get_stat_info(const struct gigaedge::denoiser::Context<float>* ctx) = 0;

 protected:
  // Startup/shutdown for NN resources.
  GigaDenoiserRetCode do_start() override;
  void do_stop() override;

  // Runners moved from Base.
  GigaDenoiserRetCode launch_feature_extractor_runner();
  GigaDenoiserRetCode launch_inference_runner();

  // fill_in_data_for_processing moved here; still abstract for final handlers.
  virtual void fill_in_data_for_processing(std::unique_ptr<gigaedge::denoiser::Task>& task_ptr,
                                           std::vector<float>& input) = 0;

  // Still abstract here; concrete handlers implement the algorithm.
  virtual GigaDenoiserRetCode infer_frame(std::unique_ptr<gigaedge::denoiser::Task>& task_ptr) = 0;

  bool is_zeroed_vector(const std::vector<float>& v, float epsilon = 0.0f) noexcept;
};

namespace tools {
void prepare_first_input_frame(const std::vector<float>& current_chunk, std::vector<float>& frame, uint32_t frame_size);
}  // namespace tools

}  // namespace gigaedge::denoiser
