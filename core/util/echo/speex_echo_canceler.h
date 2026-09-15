#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "util/echo/delay_compensator.h"
#include "util/echo/echo_canceler.h"
#include "util/sound/loudness_meter.h"

namespace util::echo {

constexpr int32_t echo_suppress = -60;
constexpr int32_t echo_suppress_active = -10;
constexpr int32_t echo_suppress_premute = -30;
constexpr int32_t lbk_active_threshold = -30;
constexpr int32_t int16_scale = 32767;

using SpeexEchoStatePtr = std::shared_ptr<SpeexEchoState>;

class SpeexPreprocess {
 public:
  explicit SpeexPreprocess(SpeexEchoStatePtr echo, size_t frame_size, size_t sample_rate)
      : echo_{echo}, frame_size_{frame_size} {
    auto state = speex_preprocess_state_init(frame_size, sample_rate);
    proc_ = std::shared_ptr<SpeexPreprocessState>(state, speex_preprocess_state_destroy);
    speex_preprocess_ctl(proc_.get(), SPEEX_PREPROCESS_SET_ECHO_STATE, echo_.get());
  }

  void process(std::vector<int16_t>& sig) {
    if (sig.size() != frame_size_) {
      throw std::runtime_error("frame size error");
    }
    // TODO(vayulis): use VAD result?
    speex_preprocess_run(proc_.get(), sig.data());
  }

  std::vector<int16_t> process(const std::vector<int16_t>& sig) {
    std::vector<int16_t> out(sig);
    this->process(out);
    return out;
  }

  void set_echo_suppression(int32_t supress_db, int32_t active_db) {
    speex_preprocess_ctl(proc_.get(), SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &supress_db);
    speex_preprocess_ctl(proc_.get(), SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &active_db);
  }

 private:
  std::shared_ptr<SpeexPreprocessState> proc_;
  SpeexEchoStatePtr echo_;
  const size_t frame_size_;
};


class PreMuter {
 public:
  explicit PreMuter(int32_t fs, float threshold_db = -30, float suppress_db = -30, float time = 1)
      : threshold_db_{threshold_db}, target_level_{dsplib::db2mag(suppress_db)} {
    steps_ = std::round((time * fs) / 1000);
    loud_meter_ = std::make_unique<sound::LoudnessMeter>(fs);
  }

  float process(const std::vector<float>& sig) {
    bool active = false;

    if (loud_meter_) {
      loud_meter_->update(sig);
      auto state = loud_meter_->state();
      auto st = state.short_term.value_or(threshold_db_);
      active = (st > threshold_db_);
    }

    // smooth gain values
    if (active) {
      gain_ = (gain_ * 0.1) + (target_level_ * 0.9);
      steps_ -= sig.size();
    } else {
      gain_ = (gain_ * 0.1) + 0.9;
    }

    // release module
    if (loud_meter_ && steps_ <= 0) {
      loud_meter_.reset();
      steps_ = 0;
    }

    return gain_;
  }

 private:
  float threshold_db_{0};
  float target_level_{1.0f};
  float gain_{1.0};
  int32_t steps_{0};
  std::unique_ptr<sound::LoudnessMeter> loud_meter_;
};

class SpeexEchoCanceler : public IEchoCanceler {
 public:
  explicit SpeexEchoCanceler(const EchoCancelerConfig& config) : cfg_{config} {
    std::string speex_error("Speex echo canceler error! ");
    if (config.num_mic != 1 || config.num_lbk != 1) {
      throw std::runtime_error(speex_error + std::string("Only single mic and lbk supported."));
    }

    if (config.echo_premute > 0) {
      premute_ = std::make_unique<PreMuter>(config.sample_rate, lbk_active_threshold, echo_suppress_premute,
                                            config.echo_premute);
    }

    auto state = speex_echo_state_init_mc(config.frame_len, config.filter_len, config.num_mic, config.num_lbk);
    if (state == nullptr) {
      throw std::runtime_error(speex_error + std::string("Failed to create speex echo."));
    }

    echo_ = SpeexEchoStatePtr(state, speex_echo_state_destroy);
    int32_t fs = config.sample_rate;
    speex_echo_ctl(echo_.get(), SPEEX_ECHO_SET_SAMPLING_RATE, static_cast<void*>(&fs));
    mic16_.resize(cfg_.frame_len);
    lbk16_.resize(cfg_.frame_len);
    out16_.resize(cfg_.frame_len);

    if (cfg_.delay_compensator) {
      // max_delay is 1 sec, update range is 10 ms
      dcomp_ = std::make_unique<DelayCompensator>(fs, fs * 10 / 1000);
    }

    if (cfg_.echo_suppression) {
      pproc_ = std::make_unique<SpeexPreprocess>(echo_, cfg_.frame_len, cfg_.sample_rate);
      pproc_->set_echo_suppression(echo_suppress, echo_suppress_active);  // TODO(vayulis): move to up level config?
    }
  }

  virtual ~SpeexEchoCanceler() {}

  void process(const std::vector<float>& mic, const std::vector<float>& lbk, std::vector<float>& out) final {
    const size_t n = cfg_.frame_len;
    out.resize(n);

    if (mic.size() != n || lbk.size() != n || out.size() != n) {
      std::string speex_error("Speex echo canceler error! ");
      throw std::runtime_error(
          speex_error + std::string("Input buffer size incorrect. Possibly configuration and real data differ."));
    }

    dsplib::arr_real tmic(mic);
    dsplib::arr_real tlbk(lbk);

    // speaker delay compensate
    if (dcomp_) {
      tlbk = dcomp_->process(tmic, tlbk);
    }

    for (size_t i = 0; i < n; ++i) {
      mic16_[i] = std::round(tmic[i] * int16_scale);
      lbk16_[i] = std::round(tlbk[i] * int16_scale);
    }

    speex_echo_cancellation(echo_.get(), mic16_.data(), lbk16_.data(), out16_.data());

    // residual echo suppression
    if (pproc_) {
      pproc_->process(out16_);
    }

    // premute result (only with active lbk channel)
    float gain = 1.0F / int16_scale;
    if (premute_) {
      gain *= premute_->process(lbk);
    }

    for (size_t i = 0; i < n; ++i) {
      out[i] = out16_[i] * gain;
    }
  }

  const EchoCancelerConfig& config() const noexcept final { return cfg_; }

  int32_t delay_len() const noexcept final { return 0; }

 private:
  SpeexEchoStatePtr echo_;
  EchoCancelerConfig cfg_;
  std::vector<spx_int16_t> mic16_;
  std::vector<spx_int16_t> lbk16_;
  std::vector<spx_int16_t> out16_;
  std::unique_ptr<DelayCompensator> dcomp_;
  std::unique_ptr<SpeexPreprocess> pproc_;
  std::unique_ptr<PreMuter> premute_;
};

}  // namespace util::echo
