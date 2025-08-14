#include "util/sound/loudness_control.h"

#include <dsplib.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "util/sound/loudness_meter.h"

namespace util::sound {

// simple stat collector for short-term power
// TODO(vayulis): add hold time logic for peak values (or auto-reset)
class LoudnessStat {
 public:
  LoudnessStat() = default;

  void update(float power) noexcept {
    if (!min_power_) {
      min_power_ = power;
    }
    if (!max_power_) {
      max_power_ = power;
    }

    max_power_ = std::max(*max_power_, power);
    min_power_ = std::min(*min_power_, power);
  }

  float max_power() const noexcept { return max_power_.value_or(0.0f); }
  float min_power() const noexcept { return min_power_.value_or(-80.0f); }

  void reset() noexcept {
    min_power_.reset();
    max_power_.reset();
  }

 private:
  std::optional<float> min_power_;
  std::optional<float> max_power_;
};


LoudnessControl::LoudnessControl(uint32_t sample_rate, float target_level, float bypass_time, float max_gain)
    : m_target_db{target_level}, m_max_gain_db{max_gain} {
  m_mtr = std::make_shared<LoudnessMeter>(sample_rate);
  m_stat = std::make_shared<LoudnessStat>();
  m_bypass_counter = static_cast<int64_t>(bypass_time * static_cast<float>(sample_rate));
}

std::vector<float> LoudnessControl::process(const std::vector<float>& x) {
  dsplib::arr_real out(x);

  const auto mres = m_mtr->update(x);
  if (m_mtr->is_ready()) {
    m_last_short_term = mres.short_term;
    m_stat->update(mres.short_term);
  } else {
    m_last_short_term.reset();
  }

  // limit max gain
  float db = m_target_db - m_stat->max_power();
  db = std::min(db, m_max_gain_db);
  db = std::max(db, -m_max_gain_db);

  // smooth gain value
  m_gain_db = static_cast<float>((m_gain_db * 0.9f) + (db * 0.1f));
  m_gain = dsplib::db2mag(m_gain_db);

  // ignore some first data
  if (m_bypass_counter > 0) {
    m_bypass_counter -= static_cast<int64_t>(x.size());
    m_gain = 1.0f;
  }

  out *= m_gain;
  return out.to_vec();
}

void LoudnessControl::reset() { m_stat->reset(); }

LoudnessControl::State LoudnessControl::state() const noexcept {
  State s{};
  s.current_gain_db_smoothed = m_gain_db;
  s.current_gain_linear      = m_gain;
  s.last_short_term_lufs     = m_last_short_term;
  s.longterm_max_lufs        = m_stat->max_power();
  s.longterm_min_lufs        = m_stat->min_power();
  s.bypass_active            = (m_bypass_counter > 0);
  return s;
}

}  // namespace util::sound
