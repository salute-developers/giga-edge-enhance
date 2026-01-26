#pragma once

#include <memory>
#include <vector>

#include "util/sound/rnnoise.h"
#include "util/sound/voice_activity.h"

namespace util::sound {

class RNNoiseVad : public IVad {
 public:
  explicit RNNoiseVad(const VadConfig& cfg);

  virtual ~RNNoiseVad() = default;

  IVad::State process(const std::vector<float>& x) final;

  [[nodiscard]] IVad::State state() const noexcept final;

  [[nodiscard]] uint32_t frame_len() const noexcept final;

  [[nodiscard]] uint32_t sample_rate() const noexcept final;

  [[nodiscard]] uint32_t delay() const noexcept final;

 private:
  State state_;
  std::shared_ptr<::util::sound::RNNoise> rnnoise_;
  float threshold_{0.5};
};

}  // namespace util::sound
