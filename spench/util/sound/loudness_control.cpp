#include "util/sound/loudness_control.h"

#include <dsplib.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "util/sound/binary_medfilt.h"
#include "util/sound/common.h"
#include "util/sound/loudness_meter.h"
#include "util/sound/voice_activity.h"

namespace util::sound {

namespace {

constexpr float VAD_THRESHOLD = 0.8;  // rnnoise vad threshold
constexpr float VAD_MFLT_WIN = 0.1;   // median filter (in sec) for rnnoise VAD

constexpr float SPEECH_ACTIVITY_LOG = 3.0;  // speech activity buffer len (in sec)

constexpr float COMP_OFFSET = 6.0;  // offset (db) for compressor threshold relatively target LUFS
constexpr float COMP_RATIO = 3;
constexpr float COMP_KNEE = 5;
constexpr float LIMITER_THRESHOLD = -1;

constexpr float NOISE_FLOOR_DB = -80;

}  // namespace

// TODO(vayulis): remove in future
namespace legacy {

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

class LoudnessControlImpl : public ILoudnessControl {
 public:
  explicit LoudnessControlImpl(uint32_t sample_rate, float target_level = -23.0f, float bypass_time = 3.0f,
                               float max_gain = 40.0f)
      : m_target_db{target_level}, m_max_gain_db{max_gain} {
    m_mtr = std::make_shared<LoudnessMeter>(sample_rate);
    m_stat = std::make_shared<LoudnessStat>();
    m_bypass_counter = static_cast<int64_t>(bypass_time * static_cast<float>(sample_rate));
  }


  [[nodiscard]] std::vector<float> process(const std::vector<float>& x) {
    dsplib::arr_real out(x);

    m_mtr->update(x);
    auto short_term = m_mtr->state().short_term.value_or(NOISE_FLOOR_DB);
    m_last_short_term = short_term;
    m_stat->update(short_term);

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

  [[nodiscard]] State state() const noexcept final {
    State s{};
    s.current_gain_db_smoothed = m_gain_db;
    s.last_short_term_lufs = m_last_short_term;
    s.longterm_max_lufs = m_stat->max_power();
    s.longterm_min_lufs = m_stat->min_power();
    s.bypass_active = (m_bypass_counter > 0);
    return s;
  }

  Method method() const noexcept final { return ILoudnessControl::SIMPLE; }


 private:
  std::shared_ptr<LoudnessMeter> m_mtr;
  std::shared_ptr<LoudnessStat> m_stat;
  int64_t m_bypass_counter{0};
  const float m_target_db;
  const float m_max_gain_db;
  float m_gain_db{0.0f};
  float m_gain{1.0f};

  std::optional<float> m_last_short_term;  // cached from meter
};

}  // namespace legacy

class SmoothValue {
 public:
  explicit SmoothValue(size_t update_period, float init, float min, float max, float attack = 0.1, float release = 0.05)
      : m_period{update_period},
        m_value{init},
        m_target{init},
        m_min{min},
        m_max{max},
        m_attack{attack},
        m_release{release} {
    DSPLIB_ASSERT(attack >= 0 && attack <= 1.0, "`attack` must be in range [0, 1]");
    DSPLIB_ASSERT(release >= 0 && release <= 1.0, "`release` must be in range [0, 1]");
  }

  dsplib::arr_real update(size_t count) {
    dsplib::arr_real out(count);
    for (size_t i = 0; i < count; ++i) {
      m_step++;
      if (m_step == m_period) {
        m_step = 0;
        if (m_target > m_value) {
          m_value = (m_target * m_attack) + (m_value * (1 - m_attack));
        } else {
          m_value = (m_target * m_release) + (m_value * (1 - m_release));
        }

        m_value = std::max(m_value, m_min);
        m_value = std::min(m_value, m_max);
      }
      out[i] = m_value;
    }
    return out;
  }

  void set(float value) noexcept { m_target = value; }

  [[nodiscard]] float get() const noexcept { return m_value; }

 private:
  size_t m_period;
  size_t m_step{0};
  float m_value;
  float m_target;
  float m_min;
  float m_max;
  float m_attack;
  float m_release;
};


class LoudnessControlImpl : public ILoudnessControl {
 public:
  LoudnessControlImpl(uint32_t sample_rate, float target_level, float max_gain, bool postprocess, float bypass)
      : m_fs{sample_rate}, m_target_db{target_level}, m_current_db{target_level} {
    m_mtr = std::make_shared<LoudnessMeter>(m_fs);

    m_skip_counter = static_cast<int64_t>(sample_rate * bypass);

    // internal processing chunk size (10 ms)
    m_frame_len = m_fs / 100;

    // voice activity detector
    m_vad = VadBuilder().set_sample_rate(m_fs).set_frame_len(m_frame_len).set_type(VadType::RNNOISE).create();

    // median filter for VAD output
    {
      const size_t dlen = (m_fs * VAD_MFLT_WIN) / m_frame_len;
      m_vadflt = std::make_shared<BinaryMedfilt>(dlen, VAD_THRESHOLD, false);
    }

    // compensate delay for VAD median filter
    {
      const size_t dlen = (m_fs * VAD_MFLT_WIN / 2) / m_frame_len;
      m_cdelay = std::make_shared<dsplib::Delay<float>>(dsplib::ones(dlen) * NOISE_FLOOR_DB);
    }

    // smooth gain controller
    m_gain_ctrl = std::make_shared<SmoothValue>(m_frame_len, 0, -max_gain, max_gain);

    // dynamic post-processing
    if (postprocess) {
      m_comp = std::make_shared<dsplib::Compressor>(m_fs, target_level + COMP_OFFSET, COMP_RATIO, COMP_KNEE);
      m_limit = std::make_shared<dsplib::Limiter>(m_fs, LIMITER_THRESHOLD);
    }
  }

  std::vector<float> process(const std::vector<float>& x) final {
    dsplib::arr_real out(x);

    // processing input by 10 ms chunks
    m_buf.write(x);
    while (m_buf.size() >= m_frame_len) {
      auto dx = m_buf.read(m_frame_len);

      // in bypass mode update stat is not allow
      if (m_skip_counter > 0) {
        m_skip_counter -= m_frame_len;
      }

      // voice activity detector
      {
        auto [_, vad_score] = m_vad->process(dx);
        bool is_speech = m_vadflt->process(vad_score);

        bool is_speech_to_noise = !is_speech && m_is_speech;
        bool is_noise_to_speech = is_speech && !m_is_speech;

        m_is_speech = is_speech;

        // TODO(vayulis): dont allow stat if noise before speech was louder
        m_allow_stat = (is_speech_to_noise || m_is_speech) && !is_noise_to_speech && (m_skip_counter <= 0);
      }

      // loudness meter
      {
        m_mtr->update(dx);
        auto state = m_mtr->state();

        // use momentary power (400 ms window)
        // short term metric (3 sec) can capture near speech noises
        bool is_valid_meas = state.momentary.has_value();
        auto pow_db = state.momentary.value_or(NOISE_FLOOR_DB);

        // delay module must be always processing
        auto dl_pow_db = m_cdelay->process(dsplib::arr_real{pow_db})[0];

        // update loudness stat
        if (is_valid_meas && m_allow_stat) {
          if (m_peakstory) {
            m_peakstory->push(dl_pow_db);
            m_current_db = quantile(m_peakstory->data(), 0.9);

            // update global min/max stat
            m_max_lufs = std::max(m_current_db, m_max_lufs);
            m_min_lufs = std::min(m_current_db, m_min_lufs);
          } else {
            // trick: first word attack
            if (!m_peakstory) {
              _init_peakstory(dl_pow_db);
            }
            m_current_db = dl_pow_db;
          }
        }
      }

      // adaptive current peak loudness to target
      m_gain_ctrl->set(m_target_db - m_current_db);
    }

    // apply normalization gain
    auto gain = dsplib::db2mag(m_gain_ctrl->update(x.size()));
    out *= gain;

    // apply compressor
    // TODO(vayulis): add makeup gain
    if (m_comp) {
      out = m_comp->process(out).out;
    }

    // apply limiter
    if (m_limit) {
      out = m_limit->process(out).out;
    }

    return out.to_vec();
  }

  Method method() const noexcept final { return ILoudnessControl::VADBASE; }

  State state() const noexcept final {
    State s{};
    s.current_gain_db_smoothed = m_gain_ctrl->get();
    s.last_short_term_lufs = m_current_db;
    s.longterm_max_lufs = m_max_lufs;
    s.longterm_min_lufs = m_min_lufs;
    return s;
  }


 private:
  void _init_peakstory(float init) {
    const size_t dlen = (m_fs * SPEECH_ACTIVITY_LOG) / m_frame_len;
    m_peakstory = std::make_shared<CycleBuffer>(dlen, init);
  }

  size_t m_fs;
  uint32_t m_frame_len;
  int64_t m_skip_counter{0};

  FIFOBuffer<float> m_buf;

  float m_target_db;
  float m_current_db;
  float m_max_lufs{-80};
  float m_min_lufs{0};

  bool m_is_speech{false};
  bool m_allow_stat{false};

  std::shared_ptr<LoudnessMeter> m_mtr;
  std::shared_ptr<SmoothValue> m_gain_ctrl;
  std::shared_ptr<CycleBuffer> m_peakstory;
  std::shared_ptr<dsplib::Delay<float>> m_cdelay;  /// to compensate BinaryMedfilt delay
  std::shared_ptr<IVad> m_vad;
  std::shared_ptr<BinaryMedfilt> m_vadflt;

  std::shared_ptr<dsplib::Compressor> m_comp;
  std::shared_ptr<dsplib::Limiter> m_limit;
};

std::unique_ptr<ILoudnessControl> LoudnessControlBuilder::create() {
  if (m_method == ILoudnessControl::SIMPLE) {
    return std::make_unique<legacy::LoudnessControlImpl>(m_sample_rate.value(), m_target_level_db.value(),
                                                         m_bypass_time.value(), m_max_gain_db.value());
  }
  if (m_method == ILoudnessControl::VADBASE) {
    return std::make_unique<LoudnessControlImpl>(m_sample_rate.value(), m_target_level_db.value(),
                                                 m_max_gain_db.value(), m_postprocessing.value(),
                                                 m_bypass_time.value_or(0.5));
  }
  return nullptr;
}

}  // namespace util::sound
