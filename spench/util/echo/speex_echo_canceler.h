#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <chrono>  //NOLINT
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "spench/util/echo/delay_compensator.h"
#include "spench/util/echo/echo_canceler.h"
#include "spench/util/sound/loudness_meter.h"

namespace util::echo {

constexpr int ECHO_SUPPRESS = -60;
constexpr int ECHO_SUPPRESS_ACTIVE = -10;
constexpr int ECHO_SUPPRESS_PREMUTE = -30;
constexpr int LBK_ACTIVE_THRESHOLD = -30;
constexpr int INT16_SCALE = 32767;

using SpeexEchoStatePtr = std::shared_ptr<SpeexEchoState>;

class SpeexPreprocess {
 public:
  explicit SpeexPreprocess(SpeexEchoStatePtr echo, size_t frame_size, size_t sample_rate)
      : m_echo{echo}, m_frame_size{frame_size} {
    auto state = speex_preprocess_state_init(frame_size, sample_rate);
    m_proc = std::shared_ptr<SpeexPreprocessState>(state, speex_preprocess_state_destroy);
    speex_preprocess_ctl(m_proc.get(), SPEEX_PREPROCESS_SET_ECHO_STATE, m_echo.get());
  }

  void Process(std::vector<int16_t>& sig) {
    if (sig.size() != m_frame_size) {
      throw std::runtime_error("frame size error");
    }
    // TODO(vayulis): use VAD result?
    speex_preprocess_run(m_proc.get(), sig.data());
  }

  std::vector<int16_t> Process(const std::vector<int16_t>& sig) {
    std::vector<int16_t> out(sig);
    this->Process(out);
    return out;
  }

  void SetEchoSuppression(int32_t supress_db, int32_t active_db) {
    speex_preprocess_ctl(m_proc.get(), SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &supress_db);
    speex_preprocess_ctl(m_proc.get(), SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &active_db);
  }

 private:
  std::shared_ptr<SpeexPreprocessState> m_proc;
  SpeexEchoStatePtr m_echo;
  const size_t m_frame_size;
};


class PreMuter {
 public:
  explicit PreMuter(int32_t fs, float threshold_db = -30, float suppress_db = -30, float time = 1)
      : m_threshold_db{threshold_db}, m_target{dsplib::db2mag(suppress_db)} {
    m_steps = std::round((time * fs) / 1000);
    m_loudm = std::make_unique<sound::LoudnessMeter>(fs);
  }

  float process(const std::vector<float>& sig) {
    bool active = false;
    if (m_loudm) {
      const auto state = m_loudm->update(sig);
      active = (state.short_term > m_threshold_db);
    }

    // smooth gain values
    if (active) {
      m_gain = (m_gain * 0.1) + (m_target * 0.9);
      m_steps -= sig.size();
    } else {
      m_gain = (m_gain * 0.1) + 0.9;
    }

    // release module
    if (m_loudm && m_steps <= 0) {
      m_loudm.reset();
      m_steps = 0;
    }

    return m_gain;
  }

 private:
  const float m_threshold_db{0};
  const float m_target{1.0};
  float m_gain{1.0};
  int32_t m_steps{0};
  std::unique_ptr<sound::LoudnessMeter> m_loudm;
};

class SpeexEchoCanceler : public IEchoCanceler {
 public:
  explicit SpeexEchoCanceler(const EchoCancelerConfig& config) : m_cfg{config} {
    std::string speex_error("Speex echo canceler error! ");
    if (config.num_mic != 1 || config.num_lbk != 1) {
      throw std::runtime_error(speex_error + std::string("Only single mic and lbk supported."));
    }

    if (config.echo_premute > 0) {
      m_premute = std::make_unique<PreMuter>(config.sample_rate, LBK_ACTIVE_THRESHOLD, ECHO_SUPPRESS_PREMUTE,
                                             config.echo_premute);
    }

    auto state = speex_echo_state_init_mc(config.frame_len, config.filter_len, config.num_mic, config.num_lbk);
    if (state == nullptr) {
      throw std::runtime_error(speex_error + std::string("Failed to create speex echo."));
    }

    m_echo = SpeexEchoStatePtr(state, speex_echo_state_destroy);
    int32_t fs = config.sample_rate;
    speex_echo_ctl(m_echo.get(), SPEEX_ECHO_SET_SAMPLING_RATE, static_cast<void*>(&fs));
    m_mic16.resize(m_cfg.frame_len);
    m_lbk16.resize(m_cfg.frame_len);
    m_out16.resize(m_cfg.frame_len);

    if (m_cfg.delay_compensator) {
      // max_delay is 1 sec, update range is 10 ms
      m_dcomp = std::make_unique<DelayCompensator>(fs, fs * 10 / 1000);
    }

    if (m_cfg.echo_suppression) {
      m_pproc = std::make_unique<SpeexPreprocess>(m_echo, m_cfg.frame_len, m_cfg.sample_rate);
      m_pproc->SetEchoSuppression(ECHO_SUPPRESS, ECHO_SUPPRESS_ACTIVE);  // TODO(vayulis): move to up level config?
    }
  }

  virtual ~SpeexEchoCanceler() {}

  void Process(const std::vector<float>& mic, const std::vector<float>& lbk, std::vector<float>& out) final {
    const size_t n = m_cfg.frame_len;
    out.resize(n);

    if (mic.size() != n || lbk.size() != n || out.size() != n) {
      std::string speex_error("Speex echo canceler error! ");
      throw std::runtime_error(
          speex_error + std::string("Input buffer size incorrect. Possibly configuration and real data differ."));
    }

    dsplib::arr_real tmic(mic);
    dsplib::arr_real tlbk(lbk);

    // speaker delay compensate
    if (m_dcomp) {
      tlbk = m_dcomp->Process(tmic, tlbk);
    }

    for (size_t i = 0; i < n; ++i) {
      m_mic16[i] = std::round(tmic[i] * INT16_SCALE);
      m_lbk16[i] = std::round(tlbk[i] * INT16_SCALE);
    }

    speex_echo_cancellation(m_echo.get(), m_mic16.data(), m_lbk16.data(), m_out16.data());

    // residual echo suppression
    if (m_pproc) {
      m_pproc->Process(m_out16);
    }

    // premute result (only with active lbk channel)
    float gain = 1.0F / INT16_SCALE;
    if (m_premute) {
      gain *= m_premute->process(lbk);
    }

    for (size_t i = 0; i < n; ++i) {
      out[i] = m_out16[i] * gain;
    }
  }

  const EchoCancelerConfig& Config() const noexcept final { return m_cfg; }

  int DelayLen() const noexcept final { return 0; }

 private:
  SpeexEchoStatePtr m_echo;
  const EchoCancelerConfig m_cfg;
  std::vector<spx_int16_t> m_mic16;
  std::vector<spx_int16_t> m_lbk16;
  std::vector<spx_int16_t> m_out16;
  std::unique_ptr<DelayCompensator> m_dcomp;
  std::unique_ptr<SpeexPreprocess> m_pproc;
  std::unique_ptr<PreMuter> m_premute;
};

}  // namespace util::echo
