#include <catch.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <dsplib.h>

#include "util/vad/rnnoise_vad.h"
#include "util/vad/voice_activity.h"

using spench::util::vad::IVad;
using spench::util::vad::RNNoiseVad;
using spench::util::vad::VadBuilder;
using spench::util::vad::VadConfig;
using spench::util::vad::VadType;

namespace {

std::vector<float> MakeFrame(uint32_t n, float amp = 0.0f) {
  std::vector<float> x(n, 0.0f);
  if (amp == 0.0f) { return x; }
  // Simple deterministic excitation: alternating +/- amp.
  for (uint32_t i = 0; i < n; ++i) { x[i] = (i & 1U) ? amp : -amp; }
  return x;
}

}  // namespace

TEST_CASE("FAIL Create returns nullptr for SIMPLE type [voice activity detection]") {
  VadConfig cfg;
  cfg.type = VadType::SIMPLE;

  auto vad = VadBuilder(cfg).create();
  REQUIRE(vad == nullptr);
}

TEST_CASE("SUCCEED Create returns RNNoiseVad for RNNOISE type [voice activity detection]") {
  VadConfig cfg;
  cfg.type = VadType::RNNOISE;
  cfg.sample_rate = 16000;
  cfg.threshold = 0.5f;

  auto vad = VadBuilder(cfg).create();
  REQUIRE(vad != nullptr);

  auto rn = std::dynamic_pointer_cast<RNNoiseVad>(vad);
  REQUIRE(rn != nullptr);

  // Basic API sanity.
  REQUIRE(rn->sample_rate() == cfg.sample_rate);
  REQUIRE(rn->frame_len() > 0);
  REQUIRE(rn->delay() == (cfg.sample_rate * 10U) / 1000U);  // 10 ms
}

TEST_CASE("FAIL Create returns nullptr for unknown VAD type [voice activity detection]") {
  VadConfig cfg;
  cfg.type = static_cast<VadType>(12345);

  auto vad = VadBuilder(cfg).create();
  REQUIRE(vad == nullptr);
}

TEST_CASE("SUCCEED Builder setters affect created VAD instance [voice activity detection]") {
  constexpr uint32_t kFs = 48000;

  auto vad = VadBuilder()
                 .set_type(VadType::RNNOISE)
                 .set_sample_rate(kFs)
                 .set_threshold(0.25f)
                 .create();

  REQUIRE(vad != nullptr);
  REQUIRE(vad->sample_rate() == kFs);
  REQUIRE(vad->delay() == (kFs * 10U) / 1000U);
  REQUIRE(vad->frame_len() > 0);
}

TEST_CASE("SUCCEED RNNoiseVad initial state is default [voice activity detection]") {
  VadConfig cfg;
  cfg.type = VadType::RNNOISE;
  cfg.sample_rate = 16000;
  cfg.threshold = 0.5f;

  RNNoiseVad vad(cfg);
  auto st = vad.state();

  REQUIRE(st.speech == false);
  REQUIRE(st.score == Approx(0.0f));
}

TEST_CASE("SUCCEED RNNoiseVad process updates internal state and returns it [voice activity detection]") {
  VadConfig cfg;
  cfg.type = VadType::RNNOISE;
  cfg.sample_rate = 16000;
  cfg.threshold = 0.5f;

  RNNoiseVad vad(cfg);

  const uint32_t n = vad.frame_len();
  REQUIRE(n > 0);

  auto x = MakeFrame(n, 0.0f);

  IVad::State ret;
  REQUIRE_NOTHROW(ret = vad.process(x));

  const auto st = vad.state();
  REQUIRE(ret.speech == st.speech);
  REQUIRE(ret.score == Approx(st.score));
  REQUIRE(std::isfinite(st.score));
}

TEST_CASE("SUCCEED Threshold extremes force speech decision [voice activity detection]") {
  // High threshold -> must be false (unless score is absurdly huge).
  {
    VadConfig cfg;
    cfg.type = VadType::RNNOISE;
    cfg.sample_rate = 16000;
    cfg.threshold = 1000.0f;

    RNNoiseVad vad(cfg);
    auto x = MakeFrame(vad.frame_len(), 0.1f);

    auto st = vad.process(x);
    REQUIRE(std::isfinite(st.score));
    REQUIRE(st.speech == false);
  }

  // Very low threshold -> must be true (unless score is NaN, which would fail finite check).
  {
    VadConfig cfg;
    cfg.type = VadType::RNNOISE;
    cfg.sample_rate = 16000;
    cfg.threshold = -1000.0f;

    RNNoiseVad vad(cfg);
    auto x = MakeFrame(vad.frame_len(), 0.0f);

    auto st = vad.process(x);
    REQUIRE(std::isfinite(st.score));
    REQUIRE(st.speech == true);
  }
}

TEST_CASE("SUCCEED Create and process voice activity detector [voice activity detection]") {
  constexpr uint32_t fs = 48000;
  constexpr uint32_t chunk_ms = 10;
  constexpr uint32_t frame_len = fs * chunk_ms / 1000;
  constexpr uint32_t num_frames = (60 * fs) / frame_len;

  auto vad = VadBuilder()
                 .set_sample_rate(fs)
                 .set_frame_len(frame_len)
                 .set_threshold(0.5)
                 .set_type(VadType::RNNOISE)
                 .create();

  dsplib::arr_real vad_state(num_frames);
  for (int32_t i = 0; i < num_frames; ++i) {
    const auto x = 1e-5 * dsplib::randn(frame_len);
    vad->process(x.to_vec());
    vad_state[i] = static_cast<int32_t>(vad->state().speech);
  }
  // false positive is smaller 1%
  REQUIRE(dsplib::mean(vad_state) < 0.01);
}
