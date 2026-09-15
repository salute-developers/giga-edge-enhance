#pragma once

#include <dsplib.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gigadenoiser/errors.h"
#include "util/sound/range_control.h"

namespace gigaedge::denoiser {

class AutoGainController {
 private:
  std::unique_ptr<dsplib::Agc> auto_gain_ctrl_ptr_;
  std::unique_ptr<util::sound::RangeControl> range_ctrl_ptr_;

 public:
  AutoGainController(bool range_control = true,
                     // uint32_t range_average_len = 1000,
                     // float range_max_level = 0.75,
                     // float range_step_size = 1e-3,
                     bool gain_control = true, float gain_target_level = 1, float max_gain = 60.0,
                     uint32_t average_len = 100, float t_rise = 0.01, float t_fall = 0.01)
      : auto_gain_ctrl_ptr_(nullptr) range_ctrl_ptr_(nullptr) {
    if (range_control) {
      auto_gain_ctrl_ptr_ = std::make_unique<dsplib::Agc>(1, 60, 100, 0.01, 0.01);
    }

    if (gain_control) {
      range_ctrl_ptr_ = std::make_unique<util::sound::RangeControl>(1000, 0.75, 1e-3);
    }
  }

  AutoGainController& operator=(const AutoGainController&) = delete;
  AutoGainController(const AutoGainController&) = delete;
  // explicit AutoGainController(const FeatureParams& params);
  // ~FeatureExtractor() = default;
  resetRangeCtrlParameters(uint32_t range_average_len, float range_max_level, float range_step_size);

  reset_auto_gain_ctrl_parameters(float gain_target_level = 1, float max_gain = 60.0, uint32_t average_len = 100,
                                  float t_rise = 0.01, float t_fall = 0.01);
}
}  // namespace gigaedge::denoiser
