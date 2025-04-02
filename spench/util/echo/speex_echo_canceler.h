#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "spench/util/echo/delay_compensator.h"
#include "spench/util/echo/echo_canceler.h"


namespace util::echo {

constexpr int ECHO_SUPPRESS = -60;
constexpr int ECHO_SUPPRESS_ACTIVE = -10;

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

class SpeexEchoCanceler : public IEchoCanceler {
 public:
  explicit SpeexEchoCanceler(const EchoCancelerConfig& config) : m_cfg{config} {
    std::string speex_error("Speex echo canceler error! ");
    if (config.num_mic != 1 || config.num_lbk != 1) {
      throw std::runtime_error(speex_error + std::string("Only single mic and lbk supported."));
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
      m_mic16[i] = std::round(tmic[i] * 32767);
      m_lbk16[i] = std::round(tlbk[i] * 32767);
    }

    speex_echo_cancellation(m_echo.get(), m_mic16.data(), m_lbk16.data(), m_out16.data());

    // preprocessing
    if (m_pproc) {
      m_pproc->Process(m_out16);
    }

    for (size_t i = 0; i < n; ++i) {
      out[i] = m_out16[i] * (1 / 32767.0F);
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
};

}  // namespace util::echo
