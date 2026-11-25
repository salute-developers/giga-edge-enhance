#include <catch.hpp>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "enhance/echo_canceller.h"
#include "util/echo/echo_canceler.h"
#include "util/sound/loudness_control.h"
#include "util/sound/resampler.h"

TEST_CASE("VAD loudness control reports its bypass period", "[loudness][state]") {
  auto control = util::sound::LoudnessControlBuilder()
                     .set_sample_rate(16000)
                     .set_method(util::sound::ILoudnessControl::vadbase)
                     .set_bypass_time(0.02F)
                     .create();
  REQUIRE(control->state().bypass_active);
  control->process(std::vector<float>(160, 0.1F));
  REQUIRE(control->state().bypass_active);
  control->process(std::vector<float>(160, 0.1F));
  REQUIRE_FALSE(control->state().bypass_active);
}

TEST_CASE("Resampler consumes single-sample downsampling chunks", "[resampler]") {
  std::vector<float> input(3001);
  for (size_t i = 0; i < input.size(); ++i) {
    input[i] = 0.3F * std::sin(static_cast<float>(i) * 0.11F);
  }
  util::sound::Resampler whole(48000, 8000, 1);
  auto expected = whole.process(input);
  const auto expected_tail = whole.flush();
  expected.insert(expected.end(), expected_tail.begin(), expected_tail.end());
  util::sound::Resampler stream(48000, 8000, 1);
  std::vector<float> actual;
  for (float sample : input) {
    const auto chunk = stream.process({sample});
    actual.insert(actual.end(), chunk.begin(), chunk.end());
  }
  const auto tail = stream.flush();
  actual.insert(actual.end(), tail.begin(), tail.end());
  REQUIRE(actual.size() == expected.size());
  for (size_t i = 0; i < actual.size(); ++i) {
    REQUIRE(actual[i] == Approx(expected[i]).margin(2e-6));
  }
}

TEST_CASE("AEC builder seconds agree with YAML premute milliseconds", "[aec][premute]") {
  util::echo::EchoCancelerConfig config;
  config.sample_rate = 16000;
  config.frame_len = 256;
  config.filter_len = 1024;
  config.echo_premute = 0.8F;
  auto native = util::echo::EchoCancelerBuilder(config).create();
  const std::string yaml =
      "echo_cancellation:\n  type: speex\n  sample_rate: 16000\n  chunk_len_ms: 16\n"
      "  filter_len: 1024\n  residual_echo_suppression_enabled: false\n"
      "  delay_compensator_enabled: false\n  echo_premute_ms: 800\n";
  giga_edge::enhance::EchoCanceller public_api(yaml);
  std::mt19937 random(417);
  std::uniform_real_distribution<float> sample(-0.2F, 0.2F);
  for (uint32_t block = 0; block < 100; ++block) {
    std::vector<float> microphone(256);
    std::vector<float> reference(256);
    for (size_t i = 0; i < microphone.size(); ++i) {
      microphone[i] = sample(random);
      reference[i] = sample(random);
    }
    const auto expected = native->process(microphone, reference);
    std::vector<float> actual;
    REQUIRE(public_api.ProcessChunk(microphone, reference, actual) == GigaEdgeEnhanceRetCode::SUCCESS);
    REQUIRE(actual == expected);
  }
}

TEST_CASE("AEC builder premute lasts seconds rather than milliseconds", "[aec][premute]") {
  util::echo::EchoCancelerConfig config;
  config.sample_rate = 16000;
  config.frame_len = 256;
  config.filter_len = 1024;
  config.echo_premute = 0.2F;
  auto shorter = util::echo::EchoCancelerBuilder(config).create();
  config.echo_premute = 0.8F;
  auto longer = util::echo::EchoCancelerBuilder(config).create();
  std::mt19937 random(812);
  std::uniform_real_distribution<float> sample(-0.2F, 0.2F);
  double short_power = 0;
  double long_power = 0;
  for (uint32_t block = 0; block < 45; ++block) {
    std::vector<float> microphone(256);
    std::vector<float> reference(256);
    for (size_t i = 0; i < microphone.size(); ++i) {
      microphone[i] = sample(random);
      reference[i] = sample(random);
    }
    const auto short_output = shorter->process(microphone, reference);
    const auto long_output = longer->process(microphone, reference);
    if (block >= 25) {
      for (size_t i = 0; i < short_output.size(); ++i) {
        short_power += short_output[i] * short_output[i];
        long_power += long_output[i] * long_output[i];
      }
    }
  }
  REQUIRE(short_power > 0.1);
  REQUIRE(long_power < short_power * 0.01);
}
