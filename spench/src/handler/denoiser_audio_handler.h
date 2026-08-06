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

  SpenchContext<float>* create_context(uint32_t input_sample_rate = 0,
                                      uint32_t output_sample_rate = 0) override;

  void destroy_context(SpenchContext<float>** ctx) override {
    if (*ctx != nullptr) {
      destroy_audio_streams(*ctx);
      delete *ctx;
    }
    *ctx = nullptr;
  }

  ~DenoiserAudioHandler() override;

  SpenchRetCode process_frame(struct spench::SpenchContext<float>* ctx,
                             const std::vector<float>& input_audio_frame,
                             std::vector<float>& enhanced_audio_frame) override;

  SpenchRetCode finalize_inference(struct spench::SpenchContext<float>* ctx,
                                  std::vector<float>& enhanced_audio_frame) override;

  StatInfo get_stat_info(const struct spench::SpenchContext<float>* ctx) override;

  spench::DenoiserModelAdviser<float>*       adviser()       { return adviser_.get(); }
  const spench::DenoiserModelAdviser<float>* adviser() const { return adviser_.get(); }

 private:
  // 30 min restriction
  const uint64_t max_number_of_time_frames_for_single_input = 120000;
  //
  std::unique_ptr<DenoiserModelAdviser<float>> adviser_;
  SpenchRetCode init_audio_streams(SpenchContext<float>* ctx,
                                 uint32_t input_sample_rate,
                                 uint32_t output_sample_rate);
  SpenchRetCode destroy_audio_streams(SpenchContext<float>* ctx);
  SpenchRetCode infer_frame(std::unique_ptr<spench::SpenchTask>& task_ptr) override;
  void          fill_in_data_for_processing(std::unique_ptr<spench::SpenchTask>& task_ptr,
                                        std::vector<float>& input) override;
};
}  // namespace spench
