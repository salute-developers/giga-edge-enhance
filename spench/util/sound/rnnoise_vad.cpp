#include "util/sound/rnnoise_vad.h"

#include <memory>
#include <vector>

namespace util::sound {

RNNoiseVad::RNNoiseVad(const VadConfig& cfg) : threshold_{cfg.threshold} {
  rnnoise_ = std::make_shared<::util::sound::RNNoise>(cfg.sample_rate, true);
}

IVad::State RNNoiseVad::process(const std::vector<float>& x) {
  auto [_, vad] = rnnoise_->process(x);
  state_.score = vad;
  state_.speech = (vad > threshold_);
  return this->state();
}

IVad::State RNNoiseVad::state() const noexcept { return state_; }

uint32_t RNNoiseVad::frame_len() const noexcept { return rnnoise_->stride(); }

uint32_t RNNoiseVad::sample_rate() const noexcept { return rnnoise_->sample_rate(); }

uint32_t RNNoiseVad::delay() const noexcept { return this->sample_rate() * 10 / 1000; }  // 10 ms

}  // namespace util::sound
