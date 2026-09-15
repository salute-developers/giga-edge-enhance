#include "util/sound/loudness_control.h"

#include <dsplib.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "util/sound/binary_medfilt.h"
#include "util/sound/common.h"
#include "util/sound/loudness_meter.h"
#include "util/sound/voice_activity.h"

namespace util::sound {

namespace {

constexpr float vad_threshold = 0.8;  // rnnoise vad threshold
constexpr float vad_mflt_win = 0.1;   // median filter (in sec) for rnnoise VAD

constexpr float speech_activity_log = 3.0;  // speech activity buffer len (in sec)

constexpr float comp_offset = 6.0;  // offset (db) for compressor threshold relatively target LUFS
constexpr float comp_ratio = 3;
constexpr float comp_knee = 5;
constexpr float limiter_threshold = -1;

constexpr float noise_floor_db = -80;

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
  explicit LoudnessControlImpl(uint32_t sample_rate, float target_level = -23.0f, bool postprocess = false,
                               float bypass_time = 3.0f, float max_gain = 40.0f)
      : target_db_{target_level}, max_gain_db_{max_gain} {
    mtr_ = std::make_shared<LoudnessMeter>(sample_rate);
    stat_ = std::make_shared<LoudnessStat>();
    bypass_counter_ = static_cast<int64_t>(bypass_time * static_cast<float>(sample_rate));

    // dynamic post-processing
    if (postprocess) {
      comp_ = std::make_shared<dsplib::Compressor>(sample_rate, target_level + comp_offset, comp_ratio, comp_knee);
      limit_ = std::make_shared<dsplib::Limiter>(sample_rate, limiter_threshold);
    }
  }


  [[nodiscard]] std::vector<float> process(const std::vector<float>& x) {
    dsplib::arr_real out(x);

    mtr_->update(x);
    auto short_term = mtr_->state().short_term.value_or(noise_floor_db);
    last_short_term_ = short_term;
    stat_->update(short_term);

    // limit max gain
    float db = target_db_ - stat_->max_power();
    db = std::min(db, max_gain_db_);
    db = std::max(db, -max_gain_db_);

    // smooth gain value
    gain_db_ = static_cast<float>((gain_db_ * 0.9f) + (db * 0.1f));
    gain_ = dsplib::db2mag(gain_db_);

    // ignore some first data
    if (bypass_counter_ > 0) {
      bypass_counter_ -= static_cast<int64_t>(x.size());
      gain_ = 1.0f;
    }
    out *= gain_;

    // apply compressor
    // TODO(vayulis): add makeup gain
    if (comp_) {
      out = comp_->process(out).out;
    }

    // apply limiter
    if (limit_) {
      out = limit_->process(out).out;
    }
    return out.to_vec();
  }

  [[nodiscard]] State state() const noexcept final {
    State s{};
    s.current_gain_db_smoothed = gain_db_;
    s.last_short_term_lufs = last_short_term_;
    s.longterm_max_lufs = stat_->max_power();
    s.longterm_min_lufs = stat_->min_power();
    s.bypass_active = (bypass_counter_ > 0);
    return s;
  }

  Method method() const noexcept final { return ILoudnessControl::simple; }


 private:
  std::shared_ptr<LoudnessMeter> mtr_;
  std::shared_ptr<LoudnessStat> stat_;
  int64_t bypass_counter_{0};
  float target_db_;
  float max_gain_db_;
  float gain_db_{0.0f};
  float gain_{1.0f};

  std::shared_ptr<dsplib::Compressor> comp_;
  std::shared_ptr<dsplib::Limiter> limit_;

  std::optional<float> last_short_term_;  // cached from meter
};

}  // namespace legacy

class SmoothValue {
 public:
  explicit SmoothValue(size_t update_period, float init, float min, float max, float attack = 0.1, float release = 0.05)
      : period_{update_period}, value_{init}, target_{init}, min_{min}, max_{max}, attack_{attack}, release_{release} {
    DSPLIB_ASSERT(attack >= 0 && attack <= 1.0, "`attack` must be in range [0, 1]");
    DSPLIB_ASSERT(release >= 0 && release <= 1.0, "`release` must be in range [0, 1]");
  }

  dsplib::arr_real update(size_t count) {
    dsplib::arr_real out(count);
    for (size_t i = 0; i < count; ++i) {
      step_++;
      if (step_ == period_) {
        step_ = 0;
        if (target_ > value_) {
          value_ = (target_ * attack_) + (value_ * (1 - attack_));
        } else {
          value_ = (target_ * release_) + (value_ * (1 - release_));
        }

        value_ = std::max(value_, min_);
        value_ = std::min(value_, max_);
      }
      out[i] = value_;
    }
    return out;
  }

  void set(float value) noexcept { target_ = value; }

  [[nodiscard]] float get() const noexcept { return value_; }

 private:
  size_t period_;
  size_t step_{0};
  float value_;
  float target_;
  float min_;
  float max_;
  float attack_;
  float release_;
};


class LoudnessControlImpl : public ILoudnessControl {
 public:
  LoudnessControlImpl(uint32_t sample_rate, float target_level, float max_gain, bool postprocess, float bypass)
      : fs_{sample_rate}, target_db_{target_level}, current_db_{target_level} {
    mtr_ = std::make_shared<LoudnessMeter>(fs_);

    skip_counter_ = static_cast<int64_t>(sample_rate * bypass);

    // internal processing chunk size (10 ms)
    frame_len_ = fs_ / 100;

    // voice activity detector
    vad_ = VadBuilder().set_sample_rate(fs_).set_frame_len(frame_len_).set_type(VadType::rnnoise).create();

    // median filter for VAD output
    {
      const size_t dlen = (fs_ * vad_mflt_win) / frame_len_;
      vadflt_ = std::make_shared<BinaryMedfilt>(dlen, vad_threshold, false);
    }

    // compensate delay for VAD median filter
    {
      const size_t dlen = (fs_ * vad_mflt_win / 2) / frame_len_;
      cdelay_ = std::make_shared<dsplib::Delay<float>>(dsplib::ones(dlen) * noise_floor_db);
    }

    // smooth gain controller
    gain_ctrl_ = std::make_shared<SmoothValue>(frame_len_, 0, -max_gain, max_gain);

    // dynamic post-processing
    if (postprocess) {
      comp_ = std::make_shared<dsplib::Compressor>(fs_, target_level + comp_offset, comp_ratio, comp_knee);
      limit_ = std::make_shared<dsplib::Limiter>(fs_, limiter_threshold);
    }
  }

  std::vector<float> process(const std::vector<float>& x) final {
    dsplib::arr_real out(x);

    // processing input by 10 ms chunks
    buf_.write(x);
    while (buf_.size() >= frame_len_) {
      auto dx = buf_.read(frame_len_);

      // in bypass mode update stat is not allow
      if (skip_counter_ > 0) {
        skip_counter_ -= frame_len_;
      }

      // voice activity detector
      {
        auto [_, vad_score] = vad_->process(dx);
        bool is_speech = vadflt_->process(vad_score);

        bool is_speech_to_noise = !is_speech && is_speech_;
        bool is_noise_to_speech = is_speech && !is_speech_;

        is_speech_ = is_speech;

        // TODO(vayulis): dont allow stat if noise before speech was louder
        allow_stat_ = (is_speech_to_noise || is_speech_) && !is_noise_to_speech && (skip_counter_ <= 0);
      }

      // loudness meter
      {
        mtr_->update(dx);
        auto state = mtr_->state();

        // use momentary power (400 ms window)
        // short term metric (3 sec) can capture near speech noises
        bool is_valid_meas = state.momentary.has_value();
        auto pow_db = state.momentary.value_or(noise_floor_db);

        // delay module must be always processing
        auto dl_pow_db = cdelay_->process(dsplib::arr_real{pow_db})[0];

        // update loudness stat
        if (is_valid_meas && allow_stat_) {
          if (peakstory_) {
            peakstory_->push(dl_pow_db);
            current_db_ = quantile(peakstory_->data(), 0.9);

            // update global min/max stat
            max_lufs_ = std::max(current_db_, max_lufs_);
            min_lufs_ = std::min(current_db_, min_lufs_);
          } else {
            // trick: first word attack
            if (!peakstory_) {
              init_peakstory(dl_pow_db);
            }
            current_db_ = dl_pow_db;
          }
        }
      }

      // adaptive current peak loudness to target
      gain_ctrl_->set(target_db_ - current_db_);
    }

    // apply normalization gain
    auto gain = dsplib::db2mag(gain_ctrl_->update(x.size()));
    out *= gain;

    // apply compressor
    // TODO(vayulis): add makeup gain
    if (comp_) {
      out = comp_->process(out).out;
    }

    // apply limiter
    if (limit_) {
      out = limit_->process(out).out;
    }

    return out.to_vec();
  }

  Method method() const noexcept final { return ILoudnessControl::vadbase; }

  State state() const noexcept final {
    State s{};
    s.current_gain_db_smoothed = gain_ctrl_->get();
    s.last_short_term_lufs = current_db_;
    s.longterm_max_lufs = max_lufs_;
    s.longterm_min_lufs = min_lufs_;
    return s;
  }


 private:
  void init_peakstory(float init) {
    const size_t dlen = (fs_ * speech_activity_log) / frame_len_;
    peakstory_ = std::make_shared<CycleBuffer>(dlen, init);
  }

  size_t fs_;
  uint32_t frame_len_;
  int64_t skip_counter_{0};

  FIFOBuffer<float> buf_;

  float target_db_;
  float current_db_;
  float max_lufs_{-80};
  float min_lufs_{0};

  bool is_speech_{false};
  bool allow_stat_{false};

  std::shared_ptr<LoudnessMeter> mtr_;
  std::shared_ptr<SmoothValue> gain_ctrl_;
  std::shared_ptr<CycleBuffer> peakstory_;
  std::shared_ptr<dsplib::Delay<float>> cdelay_;  /// to compensate BinaryMedfilt delay
  std::shared_ptr<IVad> vad_;
  std::shared_ptr<BinaryMedfilt> vadflt_;

  std::shared_ptr<dsplib::Compressor> comp_;
  std::shared_ptr<dsplib::Limiter> limit_;
};

std::unique_ptr<ILoudnessControl> LoudnessControlBuilder::create() {
  const uint32_t sample_rate = sample_rate_.value();
  const float target_level = target_level_db_.value();
  const float max_gain = max_gain_db_.value();
  const float bypass_time = bypass_time_.value_or(method_ == ILoudnessControl::simple ? 3.0F : 0.5F);
  if (sample_rate < 100 || sample_rate > 100000) {
    throw std::invalid_argument("Loudness Control sample rate must be in [100, 100000].");
  }
  if (!std::isfinite(target_level) || !std::isfinite(max_gain) || !std::isfinite(bypass_time) || max_gain < 0.0F ||
      bypass_time < 0.0F) {
    throw std::invalid_argument("Loudness Control levels and timing are invalid.");
  }

  if (method_ == ILoudnessControl::simple) {
    return std::make_unique<legacy::LoudnessControlImpl>(sample_rate, target_level, postprocessing_.value_or(false),
                                                         bypass_time, max_gain);
  }
  if (method_ == ILoudnessControl::vadbase) {
    return std::make_unique<LoudnessControlImpl>(sample_rate, target_level, max_gain, postprocessing_.value_or(false),
                                                 bypass_time);
  }
  return nullptr;
}

}  // namespace util::sound
