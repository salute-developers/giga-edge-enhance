#include <audiosup/audio-reader.h>
#include <dsplib.h>

#include <catch.hpp>
#include <cmath>
#include <string>
#include <vector>

#include "util/sound/loudness_control.h"
#include "util/sound/loudness_meter.h"

using util::sound::ILoudnessControl;
using util::sound::LoudnessControlBuilder;
using util::sound::LoudnessMeter;

namespace {

float _integrated_loudness(const std::vector<LoudnessMeter::State>& stat) {
  auto x = dsplib::zeros(stat.size());
  for (size_t i = 0; i < stat.size(); ++i) {
    x[i] = stat[i].momentary.value_or(-70);
  }
  x = dsplib::db2pow(x);
  float min_th = dsplib::db2pow(-70);
  x = x[x > min_th];
  auto rel_gate = -0.691 + dsplib::pow2db(dsplib::mean(x)) - 10;
  rel_gate = dsplib::db2pow(rel_gate);
  x = x[x > rel_gate];
  auto integrated = dsplib::mean(x);
  integrated = -0.691F + dsplib::pow2db(integrated + 1e-8);
  return integrated;
}

float _peak_short_term_loudness(const std::vector<LoudnessMeter::State>& stat) {
  auto x = dsplib::zeros(stat.size());
  for (size_t i = 0; i < stat.size(); ++i) {
    x[i] = stat[i].short_term.value_or(-70);
  }
  return dsplib::max(x);
}

float _mean_abs(const std::vector<float>& x) {
  if (x.empty()) {
    return 0.0f;
  }
  double s = 0.0;
  for (float v : x) {
    s += std::abs(v);
  }
  return static_cast<float>(s / static_cast<double>(x.size()));
}

float _max_abs(const std::vector<float>& x) {
  float m = 0.0f;
  for (float v : x) {
    m = std::max(m, std::abs(v));
  }
  return m;
}

std::vector<float> _const_vec(size_t n, float v) {
  return std::vector<float>(n, v);
}
}  // namespace

TEST_CASE("Loudness control on keyboard clicks [Loudness control]") {
  float max_gain_db = 30;
  float target_lufs = -15;
  const std::string input = "data/sounds/macbook_noise_clicks.flac";
  const int32_t fs = audiosup::audio_info(input)->sample_rate;
  const int32_t frame_len = fs * 16 / 1000;

  auto lc = LoudnessControlBuilder()
                .set_sample_rate(fs)
                .set_method(ILoudnessControl::vadbase)
                .set_target_level(target_lufs)
                .set_max_gain(max_gain_db)
                .set_post_processing(true)
                .create();

  auto in_mtr = LoudnessMeter(fs);
  auto out_mtr = LoudnessMeter(fs);

  std::vector<LoudnessMeter::State> in_stat;
  std::vector<LoudnessMeter::State> out_stat;

  auto reader = audiosup::AudioReader(input, frame_len);
  while (!reader.is_end()) {
    auto in = reader.next().get_channel(0).to_vec();
    in_mtr.update(in);
    in_stat.push_back(in_mtr.state());

    auto out = lc->process(in);
    out_mtr.update(out);
    out_stat.push_back(out_mtr.state());
  }

  // loudness is not changed

  auto orig_st_max = _peak_short_term_loudness(in_stat);
  auto proc_st_max = _peak_short_term_loudness(out_stat);
  REQUIRE(dsplib::abs(orig_st_max - proc_st_max) < 1.0);

  auto orig_int = _integrated_loudness(in_stat);
  auto proc_int = _integrated_loudness(out_stat);
  REQUIRE(dsplib::abs(orig_int - proc_int) < 1.0);
}


TEST_CASE("Loudness control for speech + noise [Loudness control]") {
  float max_gain_db = 30;
  float target_lufs = -15;

  const std::string noise_file = "data/sounds/macbook_noise_clicks.flac";
  const std::string speech_file = "data/sounds/macbook_clean_speech.flac";
  const int32_t fs = audiosup::audio_info(noise_file)->sample_rate;
  const int32_t frame_len = fs * 16 / 1000;

  dsplib::arr_real speech_gains = {-15, -10, -5, 0, 5, 10};

  for (auto gain_db : speech_gains) {
    const auto gain = dsplib::db2mag(gain_db);

    auto lc = LoudnessControlBuilder()
                  .set_sample_rate(fs)
                  .set_method(ILoudnessControl::vadbase)
                  .set_target_level(target_lufs)
                  .set_max_gain(max_gain_db)
                  .set_post_processing(false)
                  .create();

    auto in_mtr = LoudnessMeter(fs);
    auto out_mtr = LoudnessMeter(fs);

    std::vector<LoudnessMeter::State> out_stat;

    auto noise_r = audiosup::AudioReader(noise_file, frame_len);
    auto speech_r = audiosup::AudioReader(speech_file, frame_len);
    REQUIRE(noise_r.info().sample_rate == speech_r.info().sample_rate);

    while (!noise_r.is_end() && !speech_r.is_end()) {
      auto in_noise = dsplib::arr_real(noise_r.next().get_channel(0).to_vec());
      auto in_speech = dsplib::arr_real(speech_r.next().get_channel(0).to_vec());
      auto in = in_noise + (in_speech * gain);

      in_mtr.update(in.to_vec());

      auto out = lc->process(in.to_vec());
      out_mtr.update(out);
      out_stat.push_back(out_mtr.state());
    }

    auto proc_db = _integrated_loudness(out_stat);
    REQUIRE(dsplib::abs(target_lufs - proc_db) < 3.0);
  }
}

TEST_CASE("FAIL LoudnessControlBuilder create without sample rate [Loudness control]") {
  REQUIRE_THROWS_AS(
      (LoudnessControlBuilder()
           .set_method(ILoudnessControl::vadbase)
           .set_target_level(-15.0f)
           .set_max_gain(30.0f)
           .set_post_processing(false)
           .create()),
      std::bad_optional_access);
}

TEST_CASE("FAIL LoudnessControlBuilder create with unknown method returns nullptr [Loudness control]") {
  const uint32_t sample_rate = 16000;
  auto lc = LoudnessControlBuilder()
                .set_sample_rate(sample_rate)
                .set_method(static_cast<ILoudnessControl::Method>(777))
                .create();
  REQUIRE(lc == nullptr);
}

TEST_CASE("SUCCEED LoudnessControlBuilder default method is SIMPLE [Loudness control]") {
  const uint32_t sample_rate = 16000;
  auto lc = LoudnessControlBuilder().set_sample_rate(sample_rate).create();
  REQUIRE(lc != nullptr);
  REQUIRE(lc->method() == ILoudnessControl::simple);
}

TEST_CASE("SUCCEED SIMPLE bypass keeps gain = 1 for first samples [Loudness control]") {
  const uint32_t sample_rate = 1000;
  const float target_lufs = -23.0f;
  const float max_gain_db = 40.0f;
  const float bypass_time_sec = 0.05f;  // 50 samples

  auto lc = LoudnessControlBuilder()
                .set_method(ILoudnessControl::simple)
                .set_sample_rate(sample_rate)
                .set_target_level(target_lufs)
                .set_max_gain(max_gain_db)
                .set_post_processing(false)
                .set_bypass_time(bypass_time_sec)
                .create();
  REQUIRE(lc != nullptr);
  REQUIRE(lc->method() == ILoudnessControl::simple);

const std::vector<float> in1 = _const_vec(25, 1.0f);
auto out1 = lc->process(in1);
REQUIRE(_mean_abs(out1) == Approx(_mean_abs(in1)).margin(1e-6));
REQUIRE(lc->state().bypass_active);

const std::vector<float> in2 = _const_vec(25, 1.0f);
auto out2 = lc->process(in2);
REQUIRE(_mean_abs(out2) == Approx(_mean_abs(in2)).margin(1e-6));
REQUIRE_FALSE(lc->state().bypass_active);

const std::vector<float> in3 = _const_vec(200, 1.0f);
auto out3 = lc->process(in3);

for (int32_t i = 0; i < 10; ++i) {
  out3 = lc->process(in3);
}

REQUIRE(_mean_abs(out3) < _mean_abs(in3) * 0.98f);
}

TEST_CASE("SUCCEED SIMPLE max_gain clamps commanded gain [Loudness control]") {
  const uint32_t sample_rate = 16000;
  const float target_lufs = -15.0f;
  const float max_gain_db = 6.0f;

  auto lc = LoudnessControlBuilder()
                .set_method(ILoudnessControl::simple)
                .set_sample_rate(sample_rate)
                .set_target_level(target_lufs)
                .set_max_gain(max_gain_db)
                .set_post_processing(false)
                .set_bypass_time(0.0f)
                .create();
  REQUIRE(lc != nullptr);

  const std::vector<float> in = _const_vec(1024, 1e-6f);

  for (int32_t i = 0; i < 50; ++i) {
    (void)lc->process(in);
  }

  auto s = lc->state();
  REQUIRE(s.current_gain_db_smoothed <= max_gain_db + 0.5f);
  REQUIRE(s.current_gain_db_smoothed >= -max_gain_db - 0.5f);
  REQUIRE(s.longterm_max_lufs >= s.longterm_min_lufs);
}

TEST_CASE("SUCCEED VADBASE method and state fields are sane [Loudness control]") {
  const uint32_t sample_rate = 16000;
  const float target_lufs = -15.0f;
  const float max_gain_db = 30.0f;

  auto lc = LoudnessControlBuilder()
                .set_method(ILoudnessControl::vadbase)
                .set_sample_rate(sample_rate)
                .set_target_level(target_lufs)
                .set_max_gain(max_gain_db)
                .set_post_processing(false)
                .create();
  REQUIRE(lc != nullptr);
  REQUIRE(lc->method() == ILoudnessControl::vadbase);

  const uint32_t frame_len = sample_rate / 100;

  for (int32_t i = 0; i < 200; ++i) {
    float v = (i < 50) ? 0.001f : 0.2f;
    auto in = _const_vec(frame_len, v);
    (void)lc->process(in);
  }

  auto s = lc->state();
  REQUIRE(std::isfinite(s.current_gain_db_smoothed));
  REQUIRE(std::isfinite(s.last_short_term_lufs.value_or(0.0f)));
  REQUIRE(s.current_gain_db_smoothed <= max_gain_db + 1.0f);
  REQUIRE(s.current_gain_db_smoothed >= -max_gain_db - 1.0f);

  if (s.longterm_max_lufs > -79.9f || s.longterm_min_lufs < -0.1f) {
    REQUIRE(s.longterm_max_lufs >= s.longterm_min_lufs);
  }
}

TEST_CASE("SUCCEED VADBASE limiter prevents clipping when postprocessing enabled [Loudness control]") {
  const uint32_t sample_rate = 16000;
  const float target_lufs = -15.0f;
  const float max_gain_db = 30.0f;

  auto lc = LoudnessControlBuilder()
                .set_method(ILoudnessControl::vadbase)
                .set_sample_rate(sample_rate)
                .set_target_level(target_lufs)
                .set_max_gain(max_gain_db)
                .set_post_processing(true)
                .create();
  REQUIRE(lc != nullptr);

  const std::vector<float> in = _const_vec(4096, 2.0f);
  auto out = lc->process(in);

  REQUIRE(out.size() == in.size());

  const float limiter_mag = dsplib::db2mag(-1.0f);
  REQUIRE(_max_abs(out) <= limiter_mag + 5e-3f);
}

TEST_CASE("SUCCEED process with empty input returns empty output [Loudness control]") {
  const uint32_t sample_rate = 16000;
  auto lc = LoudnessControlBuilder()
                .set_method(ILoudnessControl::vadbase)
                .set_sample_rate(sample_rate)
                .set_target_level(-15.0f)
                .set_max_gain(30.0f)
                .set_post_processing(false)
                .create();
  REQUIRE(lc != nullptr);

  const std::vector<float> in;
  auto out = lc->process(in);
  REQUIRE(out.empty());
}
