#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/denoiser_model_adviser.h"
#include "src/handler/neural_network_audio_handler.h"
namespace gigaedge::denoiser {

class DenoiserAudioHandler : public NeuralNetworkAudioHandler {
 public:
  DenoiserAudioHandler() = delete;
  DenoiserAudioHandler& operator=(const DenoiserAudioHandler&) = delete;
  DenoiserAudioHandler(const DenoiserAudioHandler&) = delete;

  explicit DenoiserAudioHandler(std::shared_ptr<DenoiserConfig> config);

  Context<float>* create_context(uint32_t input_sample_rate = 0, uint32_t output_sample_rate = 0) override;

  void destroy_context(Context<float>** ctx) override {
    if (*ctx != nullptr) {
      destroy_audio_streams(*ctx);
      delete *ctx;
    }
    *ctx = nullptr;
  }

  ~DenoiserAudioHandler() override;

  GigaDenoiserRetCode process_frame(struct gigaedge::denoiser::Context<float>* ctx,
                                    const std::vector<float>& input_audio_frame,
                                    std::vector<float>& enhanced_audio_frame) override;

  GigaDenoiserRetCode finalize_inference(struct gigaedge::denoiser::Context<float>* ctx,
                                         std::vector<float>& enhanced_audio_frame) override;

  StatInfo get_stat_info(const struct gigaedge::denoiser::Context<float>* ctx) override;

  gigaedge::denoiser::DenoiserModelAdviser<float>* adviser() { return adviser_.get(); }
  const gigaedge::denoiser::DenoiserModelAdviser<float>* adviser() const { return adviser_.get(); }

 private:
  // 30 min restriction
  const uint64_t max_number_of_time_frames_for_single_input = 120000;
  //
  std::unique_ptr<DenoiserModelAdviser<float>> adviser_;
  GigaDenoiserRetCode init_audio_streams(Context<float>* ctx, uint32_t input_sample_rate, uint32_t output_sample_rate);
  GigaDenoiserRetCode destroy_audio_streams(Context<float>* ctx);
  GigaDenoiserRetCode infer_frame(std::unique_ptr<gigaedge::denoiser::Task>& task_ptr) override;
  GigaDenoiserRetCode process_frame_internal(Context<float>* ctx, const std::vector<float>& input_audio_frame,
                                             std::vector<float>& enhanced_audio_frame, bool allow_zero_shortcut);
  void fill_in_data_for_processing(std::unique_ptr<gigaedge::denoiser::Task>& task_ptr,
                                   std::vector<float>& input) override;
};
}  // namespace gigaedge::denoiser
