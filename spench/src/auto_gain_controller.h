#pragma once

#include <memory>
#include <string>
#include <vector>
#include <utility>

// #include "spench/contrib/ctpl/ctpl_stl.h"
#include <dsplib.h>
#include "spench/util/sound/range_control.h"
#include "spench/include/errors.h"

namespace spench {

class AutoGainController {
 private:
  std::unique_ptr<dsplib::Agc> m_auto_gain_ctrl_ptr;
  std::unique_ptr<util::sound::RangeControl> m_range_ctrl_ptr;

 public:
  AutoGainController(bool range_control = true,
                     // uint32_t range_average_len = 1000,
                     // float range_max_level = 0.75,
                     // float range_step_size = 1e-3,
                     bool gain_control = true,
                     float gain_target_level = 1,
                     float max_gain = 60.0,
                     uint32_t average_len = 100,
                     float t_rise = 0.01,
                     float t_fall = 0.01):
                     m_auto_gain_ctrl_ptr(nullptr)
                     m_range_ctrl_ptr(nullptr) {
    if (range_control) {
      m_auto_gain_ctrl_ptr = std::make_unique<dsplib::Agc>(1, 60, 100, 0.01, 0.01);
    }

    if (gain_control) {
      m_range_ctrl_ptr = std::make_unique<util::sound::RangeControl>(1000, 0.75, 1e-3);
    }
  }

  AutoGainController& operator=(const AutoGainController&) = delete;
  AutoGainController(const AutoGainController&) = delete;
  // explicit AutoGainController(const FeatureParams& params);
  // ~FeatureExtractor() = default;
  resetRangeCtrlParameters(uint32_t range_average_len,
                           float range_max_level,
                           float range_step_size);

  resetAutoGainCtrlParameters(float gain_target_level = 1,
                              float max_gain = 60.0,
                              uint32_t average_len = 100,
                              float t_rise = 0.01,
                              float t_fall = 0.01);
}
}  // namespace spench
