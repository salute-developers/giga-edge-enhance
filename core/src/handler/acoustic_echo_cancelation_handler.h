#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "enhance/errors.h"
#include "src/config_reader.h"
#include "src/handler/base_audio_handler.h"
#include "util/echo/echo_canceler.h"

namespace gigaedge::enhance {

class AudioEchoCancelationHandler : public BaseAudioHandler {
 public:
  AudioEchoCancelationHandler();
  AudioEchoCancelationHandler& operator=(const AudioEchoCancelationHandler&) = delete;
  AudioEchoCancelationHandler(const AudioEchoCancelationHandler&) = delete;
  AudioEchoCancelationHandler(AudioEchoCancelationHandler&& orig) noexcept
      : BaseAudioHandler(std::move(orig)), canceler_(std::move(orig.canceler_)) {}

  explicit AudioEchoCancelationHandler(std::shared_ptr<AecConfig> config);

  // Convenience API preserved
  void process(const std::vector<float>& mic, const std::vector<float>& lbk, std::vector<float>& out);
  std::vector<float> process(const std::vector<float>& mic, const std::vector<float>& lbk) {
    std::vector<float> out;
    process(mic, lbk, out);
    return out;
  }
  std::vector<float> operator()(const std::vector<float>& mic, const std::vector<float>& lbk) {
    return process(mic, lbk);
  }


  // BaseAudioHandler integration
  GigaEdgeEnhanceRetCode process_frame(struct gigaedge::enhance::Context<float>* ctx,
                                       const std::vector<float>& mic_frame, const std::vector<float>& lbk_frame,
                                       std::vector<float>& out) override;

  // Explicitly mark single-input as unsupported for AEC
  GigaEdgeEnhanceRetCode process_frame(struct gigaedge::enhance::Context<float>*, const std::vector<float>&,
                                       std::vector<float>&) override {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  const uint32_t get_sample_rate();
  const uint16_t get_filter_length();
  const uint16_t get_frame_length();

 protected:
  GigaEdgeEnhanceRetCode do_start() override;
  void do_stop() override;

 private:
  std::unique_ptr<util::echo::IEchoCanceler> canceler_;
  GigaEdgeEnhanceRetCode load_configuration(const std::string& config_path);
};
}  // namespace gigaedge::enhance
