#include "tests/src/utils.h"
#include <catch.hpp>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <spench/util/sound/resampler.h>
#include <spench/util/sound/sound_stream.h>
#include <spench/util/sound/range_control.h>
#include <spench/util/sound/stat_collector.h>

struct TestVoiceUtils {
  const std::string data_root = "data/";

 public:
  uint16_t chunk_size_ms = 16;
  uint16_t stride_size_ms = 16;
  uint32_t src_sample_rate = 16000;
  uint32_t dst_sample_rate = 16000;
  std::vector<float> src_wav_data;
  std::vector<float> dst_wav_data;
  TestVoiceUtils() {}
  TestVoiceUtils& operator=(const TestVoiceUtils&) = delete;
  TestVoiceUtils(const TestVoiceUtils&) = delete;
  TestVoiceUtils(TestVoiceUtils&& orig) :
          chunk_size_ms(orig.chunk_size_ms),
          stride_size_ms(orig.stride_size_ms),
          src_sample_rate(orig.src_sample_rate),
          dst_sample_rate(orig.dst_sample_rate),
          src_wav_data(std::move(orig.src_wav_data)),
          dst_wav_data(std::move(orig.dst_wav_data)) {
    orig.src_wav_data.clear();
    orig.dst_wav_data.clear();
  }

  TestVoiceUtils& Init(uint16_t chunk_size,
                       uint16_t stride_size,
                       uint64_t src_sr,
                       uint64_t dst_sr) {
    chunk_size_ms = chunk_size;
    stride_size_ms = stride_size;
    src_sample_rate = src_sr;
    dst_sample_rate = dst_sr;
    return *this;
  }

  TestVoiceUtils& SetWav(const std::string& wav_path, std::vector<float>& wav_file) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_file);
    return *this;
  }

  TestVoiceUtils& SetSrcAudio(const std::string& wav_path) {
    return SetWav(wav_path, src_wav_data);
  }

  TestVoiceUtils& SetDstAudio(const std::string& wav_path) {
    return SetWav(wav_path, dst_wav_data);
  }
};

static float RefPercentileLinearFromSorted(const std::vector<float>& sorted, float q) {
  if (sorted.empty()) return std::numeric_limits<float>::quiet_NaN();
  if (q <= 0.0f) return sorted.front();
  if (q >= 1.0f) return sorted.back();
  const float pos = q * static_cast<float>(sorted.size() - 1);
  const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
  const std::size_t hi = std::min(lo + 1, sorted.size() - 1);
  const float frac = pos - static_cast<float>(lo);
  const float lo_val = sorted[lo];
  const float hi_val = sorted[hi];
  return lo_val + frac * (hi_val - lo_val);
}

struct RefStats {
  float minv, maxv, mean, median, q5, q95;
};

static RefStats MakeRefStats(std::vector<float> vals) {
  RefStats r{};
  if (vals.empty()) {
    r.minv = r.maxv = r.mean = r.median = r.q5 = r.q95 = std::numeric_limits<float>::quiet_NaN();
    return r;
  }
  r.minv = *std::min_element(vals.begin(), vals.end());
  r.maxv = *std::max_element(vals.begin(), vals.end());
  r.mean = static_cast<float>(std::accumulate(vals.begin(), vals.end(), 0.0) / static_cast<double>(vals.size()));

  std::sort(vals.begin(), vals.end());
  const std::size_t n = vals.size();
  if (n % 2 == 1) r.median = vals[n / 2];
  else            r.median = static_cast<float>(0.5 * (vals[n / 2 - 1] + vals[n / 2]));
  r.q5  = RefPercentileLinearFromSorted(vals, 0.05f);
  r.q95 = RefPercentileLinearFromSorted(vals, 0.95f);
  return r;
}

template <typename T>
static void CheckOptNear(const std::optional<T>& v,
                         T expected,
                         T tol = static_cast<T>(1e-3)) {
  REQUIRE(v.has_value());
  if (std::isnan(static_cast<double>(expected))) {
    CHECK(std::isnan(static_cast<double>(v.value())));
  } else {
    CHECK(v.value() == Approx(static_cast<double>(expected))
                           .margin(static_cast<double>(tol)));
  }
}

TEST_CASE("FAIL Create RangeControl with negative attack_time [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));
  CHECK_THROWS_WITH(util::sound::RangeControl(48000, -2.0, 5.0, -0.05, 300, 5),
                    "RangeControl: `attack_time` must be positive.");
}

TEST_CASE("FAIL Create RangeControl with negative release_time [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));
  CHECK_THROWS_WITH(util::sound::RangeControl(48000, -2.0, 5.0, 0.05, -300, 5),
                    "RangeControl: `release_time` must be positive.");
}

TEST_CASE("FAIL Create RangeControl with negative hold_time [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));
  CHECK_THROWS_WITH(util::sound::RangeControl(48000, -2.0, 5.0, 0.05, 300, -5),
                    "RangeControl: `hold_time` must be positive.");
}

TEST_CASE("SUCCEED Create RangeControl and check operator() [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));

  util::sound::RangeControl rctrl(48000, -3, 14);

  std::vector<float> test_input{0.0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 10.0, 0.0, 0.0,
                                0.0, 0.0, 0.0, 0.0, 0.0};

  std::vector<float> gt_out{0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 2.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0};

  auto test_out = rctrl(test_input);
  IsVectorsClose(test_out.out, gt_out, 0.1);
}

TEST_CASE("SUCCEED Create RangeControl and check process method [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));

  util::sound::RangeControl rctrl(48000, -3, 14);

  std::vector<float> test_input{0.0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 10.0, 0.0, 0.0,
                                0.0, 0.0, 0.0, 0.0, 0.0};

  std::vector<float> gt_out{0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 2.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0};

  auto test_out = rctrl.process(test_input);
  IsVectorsClose(test_out.out, gt_out, 0.1);
}

#ifndef BUILD_WITHOUT_SOXR
TEST_CASE("SUCCEED Resampler identity when sample rates match [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));

  util::sound::Resampler r(voice_utils.src_sample_rate, voice_utils.dst_sample_rate, 1);
  const std::vector<float> in{0.1f, -0.2f, 0.3f};
  REQUIRE(r.process(in) == in);
  REQUIRE(r.flush().empty());
  REQUIRE(r.accumulated_delay_ms() == 0.f);
}

TEST_CASE("SUCCEED Run offline audio downsampling 48kHz -> 24kHz [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(16, 16, 48000, 24000).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));
  auto resampler = util::sound::Resampler(voice_utils.src_sample_rate,
                                                  voice_utils.dst_sample_rate, 1);
  std::vector<float> output_data = resampler.process(voice_utils.src_wav_data);
  std::vector<float> ending_part_of_data = resampler.flush();

  output_data.insert(end(output_data), begin(ending_part_of_data), end(ending_part_of_data));

  if (output_data.size() >= voice_utils.dst_wav_data.size()) {
    output_data.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(output_data.size());
  }

  IsVectorsClose(voice_utils.dst_wav_data, output_data, 0.003);
}

TEST_CASE("SUCCEED Run offline audio upsampling 24kHz -> 48kHz [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(16, 16, 24000, 48000).
                                     SetSrcAudio("rec_noise_001_24k.wav").
                                     SetDstAudio("rec_noise_001_48k.wav"));

  auto resampler = util::sound::Resampler(voice_utils.src_sample_rate, voice_utils.dst_sample_rate, 1);
  std::vector<float> output_data = resampler.process(voice_utils.src_wav_data);
  std::vector<float> ending_part_of_data = resampler.flush();

  output_data.insert(end(output_data), begin(ending_part_of_data), end(ending_part_of_data));
  IsVectorsClose(voice_utils.dst_wav_data, output_data, 0.0001);
}

TEST_CASE("SUCCEED Run stream audio downsampling 48kHz -> 24kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t audio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));

  auto resampler = util::sound::Resampler(voice_utils.src_sample_rate,
                                                  voice_utils.dst_sample_rate, 1);
  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  auto dst_chunks = ChunkWav<float>(voice_utils.dst_wav_data,
                                    voice_utils.dst_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  CHECK(src_chunks.size() == dst_chunks.size());
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];

    std::vector<float> output_data = resampler.process(s_chunk);
    if (output_data.size() > 0) {
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data = resampler.flush();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }

  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio downsampling via sound stream interface 44.1kHz -> 24kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 44100;
  const uint32_t audio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_24_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                          voice_utils.dst_sample_rate,
                                                          voice_utils.chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);
    if (output_data.size() > 0) {
      CHECK(output_data.size() == chunk_size * audio_dst_sample_rate / 1000);
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  // Due to speciality of an algorithm for 44.1k resampling for our purpoces we expect worse precision
  IsVectorsClose(voice_utils.dst_wav_data, result, 1.1);
}

TEST_CASE("SUCCESS Run stream audio downsampling via sound stream interface 32kHz -> 24kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 32000;
  const uint32_t audio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test3_32_noised.wav").
                                     SetDstAudio("test3_24_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                          voice_utils.dst_sample_rate,
                                                          voice_utils.chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);
    if (output_data.size() > 0) {
      CHECK(output_data.size() == chunk_size * audio_dst_sample_rate / 1000);
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCEED Run stream audio upsampling 24kHz -> 48kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 48000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("rec_noise_001_24k.wav").
                                     SetDstAudio("rec_noise_001_48k.wav"));

  auto resampler = util::sound::Resampler(voice_utils.src_sample_rate, voice_utils.dst_sample_rate, 1);

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  auto dst_chunks = ChunkWav<float>(voice_utils.dst_wav_data,
                                    voice_utils.dst_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  CHECK(src_chunks.size() == dst_chunks.size());
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = resampler.process(s_chunk);
    if (output_data.size() > 0) {
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data = resampler.flush();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }

  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio upsampling via sound stream interface 24kHz -> 44.1kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test2_24_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                          voice_utils.dst_sample_rate,
                                                          voice_utils.chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);
    if (output_data.size() > 0) {
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  // Due to speciality of an algorithm for 44.1k resampling for our purpoces we expect worse precision
  IsVectorsClose(voice_utils.dst_wav_data, result, 1.0);
}

TEST_CASE("SUCCESS Run stream audio upsampling via sound stream interface 24kHz -> 32kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 32000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test3_24_noised.wav").
                                     SetDstAudio("test3_32_upsampled_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                          voice_utils.dst_sample_rate,
                                                          voice_utils.chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);
    if (output_data.size() > 0) {
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio downsampling via sound stream interface 48kHz -> 24kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t audio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                          voice_utils.dst_sample_rate,
                                                          voice_utils.chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);

    if (output_data.size() > 0) {
      CHECK(output_data.size() == chunk_size * audio_dst_sample_rate / 1000);
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run sound stream for audious with the equal sample rate [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(16, 16, 48000, 48000).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_48_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                          voice_utils.dst_sample_rate,
                                                          voice_utils.chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);

    REQUIRE(output_data.size() != 0);
    result.insert(end(result), begin(output_data), end(output_data));
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();

  REQUIRE(output_data.size() == 0);

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }

  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run sound stream for sequenced downsampling and upsampling 48kHz -> 48kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t audio_dst_sample_rate = 48000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_48_upsampled_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                             voice_utils.dst_sample_rate / 2,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream(voice_utils.dst_sample_rate / 2,
                                                             voice_utils.src_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::OUTPUT);
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> resampled_data = input_sound_stream->ReadChunk(s_chunk);
    if (resampled_data.size() > 0) {
      std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
      if (output_data.size() > 0) {
        result.insert(end(result), begin(output_data), end(output_data));
      }
    }
  }

  std::vector<float> resampled_data =
                            input_sound_stream->FinalizeStream();
  std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
  result.insert(end(result), begin(output_data), end(output_data));
  resampled_data = output_sound_stream->FinalizeStream();
  result.insert(end(result), begin(resampled_data), end(resampled_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.dst_wav_data, result, 0.005);
}

TEST_CASE("SUCCESS Run sound stream for sequenced downsampling and upsampling 44.1kHz -> 44.1kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 44100;
  const uint32_t audio_model_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                             audio_model_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream(audio_model_sample_rate,
                                                             voice_utils.src_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::OUTPUT);
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> resampled_data = input_sound_stream->ReadChunk(s_chunk);
    if (resampled_data.size() > 0) {
      std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
      if (output_data.size() > 0) {
        result.insert(end(result), begin(output_data), end(output_data));
      }
    }
  }

  std::vector<float> resampled_data =
                            input_sound_stream->FinalizeStream();
  std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
  result.insert(end(result), begin(output_data), end(output_data));
  resampled_data = output_sound_stream->FinalizeStream();
  result.insert(end(result), begin(resampled_data), end(resampled_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  // Due to speciality of an algorithm for 44.1k resampling for our purpoces we expect worse precision
  IsVectorsClose(voice_utils.dst_wav_data, result, 1.1);
}

TEST_CASE("SUCCESS Run sound stream for sequenced downsampling and upsampling 32kHz -> 32kHz [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 32000;
  const uint32_t audio_model_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 32000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test3_32_noised.wav").
                                     SetDstAudio("test3_32_upsampled_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                             audio_model_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream(audio_model_sample_rate,
                                                             voice_utils.src_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::OUTPUT);
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> resampled_data = input_sound_stream->ReadChunk(s_chunk);
    if (resampled_data.size() > 0) {
      std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
      if (output_data.size() > 0) {
        result.insert(end(result), begin(output_data), end(output_data));
      }
    }
  }

  std::vector<float> resampled_data =
                            input_sound_stream->FinalizeStream();
  std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
  result.insert(end(result), begin(output_data), end(output_data));
  resampled_data = output_sound_stream->FinalizeStream();
  result.insert(end(result), begin(resampled_data), end(resampled_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  // due to finalization error is a bit bigger
  IsVectorsClose(voice_utils.dst_wav_data, result, 0.015);
}

TEST_CASE("SUCCESS Input sound stream emits all resampled frames for 16*N ms chunk [sound]") {
  const uint32_t frame_ms = 16;
  const uint32_t src_sample_rate = 48000;
  const uint32_t dst_sample_rate = 8000;
  const uint32_t n = 4;  // 64 ms chunk

  const std::size_t in_frame = static_cast<std::size_t>(src_sample_rate) * frame_ms / 1000;
  const std::size_t in_chunk_long = in_frame * n;
  const std::size_t out_frame = static_cast<std::size_t>(dst_sample_rate) * frame_ms / 1000;
  const std::size_t expected_out = (in_chunk_long * dst_sample_rate) / src_sample_rate;
  const std::size_t expected_out_aligned = (expected_out / out_frame) * out_frame;

  std::vector<float> short_chunk(in_frame, 0.1f);
  std::vector<float> long_chunk(in_chunk_long, 0.1f);

  // Variable length: one N×16 ms push must return all resampled frames, not one frame.
  {
    auto stream = util::sound::BuildSoundStream(src_sample_rate, dst_sample_rate, frame_ms,
                                                util::sound::TemplateType::INPUT);
    std::vector<float> primed;
    for (uint32_t i = 0; i < 32 && primed.size() < out_frame; ++i) {
      const std::vector<float> part = stream->ReadChunk(short_chunk);
      primed.insert(primed.end(), part.begin(), part.end());
    }
    REQUIRE(primed.size() >= out_frame);

    const std::vector<float> out = stream->ReadChunk(long_chunk);
    REQUIRE(out.size() % out_frame == 0);
    REQUIRE(out.size() >= out_frame * 2);
    REQUIRE(out.size() == expected_out_aligned);
  }
}

TEST_CASE("SUCCESS Accumulated delay bounded for 44.1kHz -> 24kHz input stream [sound]") {
  const uint32_t chunk_size_ms = 16;
  const uint32_t src_sample_rate = 44100;
  const uint32_t dst_sample_rate = 24000;

  auto voice_utils = std::move(TestVoiceUtils()
                                   .Init(chunk_size_ms, chunk_size_ms, src_sample_rate, dst_sample_rate)
                                   .SetSrcAudio("test2_44_1_clean.wav")
                                   .SetDstAudio("test2_24_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  auto s = util::sound::BuildSoundStream(
      voice_utils.src_sample_rate, voice_utils.dst_sample_rate,
      voice_utils.chunk_size_ms, util::sound::TemplateType::INPUT);

  std::vector<double> delays_ms;
  int warn_count = 0;
  for (const auto& ch : src_chunks) {
    auto out = s->ReadChunk(ch);
    (void)out;
    const double d = s->GetAccumulatedDelayMs();
    delays_ms.push_back(d);

    double dtmp = 0.0;  // threshold 300 ms, throttling 30 s — should trigger rarely
    if (s->ShouldWarnDelay(300.0, 30000.0, &dtmp)) {
      ++warn_count;
    }
  }
  (void)s->FinalizeStream();  // final flush; d after flush tends toward 0

  REQUIRE_FALSE(delays_ms.empty());
  const std::size_t warmup = std::min<std::size_t>(delays_ms.size(), 50);  // ~0.8 s warm-up
  const double max_after_warmup = *std::max_element(delays_ms.begin() + warmup, delays_ms.end());
  const double last_delay = delays_ms.back();

  // The corridor should stay well below 500 ms.
  REQUIRE(max_after_warmup < 500.0);
  REQUIRE(last_delay < 420.0);
  // Warnings should not spam (usually <= 1 for short clips).
  REQUIRE(warn_count <= 2);
}

TEST_CASE("SUCCESS Accumulated delay bounded for 24kHz -> 44.1kHz stream [sound]") {
  const uint32_t chunk_size_ms = 16;
  const uint32_t src_sample_rate = 24000;
  const uint32_t dst_sample_rate = 44100;

  auto voice_utils = std::move(TestVoiceUtils()
                                   .Init(chunk_size_ms, chunk_size_ms, src_sample_rate, dst_sample_rate)
                                   .SetSrcAudio("test2_24_clean.wav")
                                   .SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  auto s = util::sound::BuildSoundStream(
      voice_utils.src_sample_rate, voice_utils.dst_sample_rate,
      voice_utils.chunk_size_ms, util::sound::TemplateType::INPUT);

  std::vector<double> delays_ms;
  int warn_count = 0;
  for (const auto& ch : src_chunks) {
    auto out = s->ReadChunk(ch);
    (void)out;
    const double d = s->GetAccumulatedDelayMs();
    delays_ms.push_back(d);

    double dtmp = 0.0;  // threshold 300 ms, throttling 30 s — should trigger rarely
    if (s->ShouldWarnDelay(300.0, 30000.0, &dtmp)) {
      ++warn_count;
    }
  }
  (void)s->FinalizeStream();

  REQUIRE_FALSE(delays_ms.empty());
  const std::size_t warmup = std::min<std::size_t>(delays_ms.size(), 50);
  const double max_after_warmup = *std::max_element(delays_ms.begin() + warmup, delays_ms.end());
  const double last_delay = delays_ms.back();

  CHECK(max_after_warmup < 500.0);
  CHECK(last_delay < 420.0);
  REQUIRE(warn_count <= 2);
}

TEST_CASE("SUCCESS Accumulated delay small and stable for 48kHz -> 24kHz [sound]") {
  const uint32_t chunk_size_ms = 16;
  const uint32_t src_sample_rate = 48000;
  const uint32_t dst_sample_rate = 24000;

  auto voice_utils = std::move(TestVoiceUtils()
                                   .Init(chunk_size_ms, chunk_size_ms, src_sample_rate, dst_sample_rate)
                                   .SetSrcAudio("test1_48_noised.wav")
                                   .SetDstAudio("test1_24_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  // Drift-control is not enabled here (no 44.1 kHz); delay should be small and stable.
  auto s = util::sound::BuildSoundStream(
      voice_utils.src_sample_rate, voice_utils.dst_sample_rate,
      voice_utils.chunk_size_ms, util::sound::TemplateType::INPUT);

  std::vector<double> delays_ms;
  int warn_count = 0;
  for (const auto& ch : src_chunks) {
    auto out = s->ReadChunk(ch);
    (void)out;
    const double d = s->GetAccumulatedDelayMs();
    delays_ms.push_back(d);

    double dtmp = 0.0;
    if (s->ShouldWarnDelay(200.0, 30000.0, &dtmp)) {
      ++warn_count;
    }
  }
  (void)s->FinalizeStream();

  REQUIRE_FALSE(delays_ms.empty());
  const std::size_t warmup = std::min<std::size_t>(delays_ms.size(), 30);
  const double max_after_warmup = *std::max_element(delays_ms.begin() + warmup, delays_ms.end());
  const double last_delay = delays_ms.back();

  // SOXR may contribute a small filter delay, but there must be no drift.
  CHECK(max_after_warmup < 150.0);
  CHECK(last_delay < 120.0);
  REQUIRE(warn_count == 0);
}

#else
TEST_CASE("FAIL Create input resampler [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 44100;
  const uint32_t audio_model_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  CHECK_THROWS_WITH(util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                             audio_model_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::INPUT),
                    std::string("SoundStream::SoundStream: Library built without resampling ability. Please ") +
                    std::string("use a sample rate that is supported by model!"));
}

TEST_CASE("FAIL Create output resampler [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 44100;
  const uint32_t audio_model_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);

  CHECK_THROWS_WITH(util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                             audio_model_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::OUTPUT),
                    std::string("SoundStream::SoundStream: Library built without resampling ability. Please ") +
                    std::string("use a sample rate that is supported by model!"));
}

TEST_CASE("SUCCESS Run sound stream without resampling [sound]") {
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 24000;
  const uint32_t audio_model_sample_rate = 24000;
  const uint32_t audio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(chunk_size, chunk_stride,
                                          audio_src_sample_rate, audio_dst_sample_rate).
                                     SetSrcAudio("test1_24_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));

  auto src_chunks = ChunkWav<float>(voice_utils.src_wav_data,
                                    voice_utils.src_sample_rate,
                                    voice_utils.chunk_size_ms,
                                    voice_utils.stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream(voice_utils.src_sample_rate,
                                                             audio_model_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream(audio_model_sample_rate,
                                                             voice_utils.src_sample_rate,
                                                             voice_utils.chunk_size_ms,
                                                             util::sound::TemplateType::OUTPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> resampled_data = input_sound_stream->ReadChunk(s_chunk);
    if (resampled_data.size() > 0) {
      std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
      if (output_data.size() > 0) {
        result.insert(end(result), begin(output_data), end(output_data));
      }
    }
  }

  std::vector<float> resampled_data =
                            input_sound_stream->FinalizeStream();
  std::vector<float> output_data = output_sound_stream->ReadChunk(resampled_data);
  result.insert(end(result), begin(output_data), end(output_data));
  resampled_data = output_sound_stream->FinalizeStream();
  result.insert(end(result), begin(resampled_data), end(resampled_data));

  if (result.size() >= voice_utils.dst_wav_data.size()) {
    result.resize(voice_utils.dst_wav_data.size());
  } else {
    voice_utils.dst_wav_data.resize(result.size());
  }
  // due to finalization error is a bit bigger
  IsVectorsClose(voice_utils.dst_wav_data, result, 0.0001);
}
#endif

TEST_CASE("SUCCEED StatCollector exact 1s window with evenly sized blocks [stat]") {
  // sr = 1000 Hz, window = 1s -> 1000 samples per flush
  util::sound::StatCollector col("basic", 1000, 1.0, util::sound::StatCollector::Options::All());

  std::optional<util::sound::StatCollector::Stat> res;
  // feed 10 blocks * 100 samples, values 1..10
  for (int i = 1; i <= 10; ++i) {
    res = col.update(static_cast<float>(i), /*samples_in_value=*/100);
  }
  REQUIRE(res.has_value());
  const auto s = *res;

  CheckOptNear(s.min, 1.0f);
  CheckOptNear(s.max, 10.0f);
  CheckOptNear(s.mean, 5.5f);
  CheckOptNear(s.median, 5.5f);       // even count -> (5 + 6) / 2
  CheckOptNear(s.q95, 9.55f, 1e-3f);  // linear interp with n=10
  CheckOptNear(s.q5, 1.45f, 1e-3f);

  CHECK(col.window_seconds() == Approx(1.0));
  CHECK(col.name() == std::string("basic"));
}

TEST_CASE("SUCCEED StatCollector carry-over KeepOverrun [stat]") {
  // sr = 10 Hz, window = 1s -> 10 samples
  util::sound::StatCollector col("carry", 10, 1.0, util::sound::StatCollector::Options::All());

  auto res = col.update(0.0f, /*samples_in_value=*/6);
  REQUIRE_FALSE(res.has_value());

  // +5 samples -> total 11, should flush once and keep 1 carried sample
  res = col.update(10.0f, /*samples_in_value=*/5);
  REQUIRE(res.has_value());
  auto s1 = *res;
  CheckOptNear(s1.min, 0.0f);
  CheckOptNear(s1.max, 10.0f);
  CheckOptNear(s1.mean, 5.0f);
  CheckOptNear(s1.median, 5.0f);
  CHECK(s1.q95.has_value());
  CHECK(s1.q5.has_value());

  // carried 1 sample; add 9 to complete next window
  res = col.update(1.0f, /*samples_in_value=*/9);
  REQUIRE(res.has_value());
  auto s2 = *res;
  CheckOptNear(s2.min, 1.0f);
  CheckOptNear(s2.max, 1.0f);
  CheckOptNear(s2.mean, 1.0f);
  CheckOptNear(s2.median, 1.0f);
  CheckOptNear(s2.q95, 1.0f);
  CheckOptNear(s2.q5, 1.0f);
}

TEST_CASE("SUCCEED StatCollector ignores missing values but advances time [stat]") {
  util::sound::StatCollector col("miss", 1000, 1.0, util::sound::StatCollector::Options::All());

  // No value this tick: advance time only → should flush after 1s and return empty stats
  auto res = col.advance(/*samples_in_value=*/1000);
  REQUIRE(res.has_value());
  auto s = *res;
  CHECK_FALSE(s.min.has_value());
  CHECK_FALSE(s.max.has_value());
  CHECK_FALSE(s.mean.has_value());
  CHECK_FALSE(s.median.has_value());
  CHECK_FALSE(s.q95.has_value());
  CHECK_FALSE(s.q5.has_value());

  // Next window: one value -> all stats collapse to that value
  res = col.update(3.0f, /*samples_in_value=*/1000);
  REQUIRE(res.has_value());
  s = *res;
  CheckOptNear(s.min, 3.0f);
  CheckOptNear(s.max, 3.0f);
  CheckOptNear(s.mean, 3.0f);
  CheckOptNear(s.median, 3.0f);
  CheckOptNear(s.q95, 3.0f);
  CheckOptNear(s.q5, 3.0f);
}

TEST_CASE("SUCCEED StatCollector set_window_seconds does hard reset [stat]") {
  util::sound::StatCollector col("reset", 100, 1.0, util::sound::StatCollector::Options::All());
  REQUIRE_FALSE(col.update(5.0f, /*samples_in_value=*/60).has_value());

  col.set_window_seconds(0.5);
  CHECK(col.window_seconds() == Approx(0.5));

  auto res = col.update(7.0f, /*samples_in_value=*/50);
  REQUIRE(res.has_value());
  const auto s = *res;
  CheckOptNear(s.min, 7.0f);
  CheckOptNear(s.max, 7.0f);
  CheckOptNear(s.mean, 7.0f);
  CheckOptNear(s.median, 7.0f);
  CheckOptNear(s.q95, 7.0f);
  CheckOptNear(s.q5, 7.0f);
}

TEST_CASE("SUCCEED StatCollector options toggle disables selected metrics [stat]") {
  util::sound::StatCollector::Options opt = util::sound::StatCollector::Options::All();
  opt.need_min = false;
  opt.need_max = false;
  opt.need_median = false;
  opt.need_q95 = false;
  opt.need_q5 = false;
  opt.need_mean = true;

  util::sound::StatCollector col("opts", 1000, 1.0, opt);

  std::optional<util::sound::StatCollector::Stat> res;
  for (int i = 1; i <= 4; ++i) {
    res = col.update(static_cast<float>(i), /*samples_in_value=*/250);
  }
  REQUIRE(res.has_value());
  const auto s = *res;

  CHECK_FALSE(s.min.has_value());
  CHECK_FALSE(s.max.has_value());
  CHECK_FALSE(s.median.has_value());
  CHECK_FALSE(s.q95.has_value());
  CHECK_FALSE(s.q5.has_value());
  CheckOptNear(s.mean, (1.0f + 2.0f + 3.0f + 4.0f) / 4.0f);
}

TEST_CASE("SUCCEED StatCollector progress_seconds reflects elapsed / sr [stat]") {
  util::sound::StatCollector col("progress", 1000, 10.0, util::sound::StatCollector::Options::All());
  REQUIRE_FALSE(col.update(1.0f, /*samples_in_value=*/2500).has_value());
  CHECK(col.progress_seconds() == Approx(2.5).margin(1e-6));
}

// Extra sanity on quantiles for 1..100 sequence
TEST_CASE("SUCCEED StatCollector quantile values for 1..100 [stat]") {
  util::sound::StatCollector col("q", 1000, 1.0, util::sound::StatCollector::Options::All());

  std::optional<util::sound::StatCollector::Stat> res;
  for (int i = 1; i <= 100; ++i) {
    res = col.update(static_cast<float>(i), /*samples_in_value=*/10);  // 100 * 10 = 1000 samples
  }
  REQUIRE(res.has_value());
  const auto s = *res;

  // min/max
  CheckOptNear(s.min, 1.0f);
  CheckOptNear(s.max, 100.0f);

  // mean/median for 1..100
  CheckOptNear(s.mean, 50.5f);
  CheckOptNear(s.median, 50.5f);

  // q5, q95 with linear interpolation idx = q*(n-1)
  CheckOptNear(s.q5, 5.95f, 1e-3f);    // 0.05*(99)=4.95 -> 5 + 0.95*(6-5) = 5.95
  CheckOptNear(s.q95, 95.05f, 1e-3f);  // 0.95*(99)=94.05 -> 95 + 0.05*(96-95) = 95.05

  // monotonicity
  REQUIRE(s.q5.has_value());
  REQUIRE(s.median.has_value());
  REQUIRE(s.q95.has_value());
  CHECK(s.q5.value() <= s.median.value());
  CHECK(s.median.value() <= s.q95.value());
}

TEST_CASE("FAIL invalid parameters: sr==0 and window<=0 are clamped [stat]") {
  // Both should clamp internally to sr=1, window=1
  util::sound::StatCollector col("clamp", /*sample_rate=*/0, /*window_seconds=*/0.0,
                                 util::sound::StatCollector::Options::All());
  CHECK(col.window_seconds() == Approx(1.0));

  // After construction, exact 1s worth of samples should flush once
  auto res = col.update(2.0f, /*samples_in_value=*/1);  // sr was clamped to 1
  REQUIRE(res.has_value());
  const auto s = *res;
  CheckOptNear(s.mean, 2.0f);
}

TEST_CASE("FAIL zero-sample updates do not flush and do not advance time [stat]") {
  util::sound::StatCollector col("zerosamp", 1000, 1.0, util::sound::StatCollector::Options::All());

  // Many zero-sample updates should neither flush nor advance progress
  for (int i = 0; i < 100; ++i) {
    auto res = col.update(1.0f, /*samples_in_value=*/0);
    REQUIRE_FALSE(res.has_value());
    CHECK(col.progress_seconds() == Approx(0.0));
  }

  // Now push full window to ensure flush still works
  auto res = col.update(1.0f, /*samples_in_value=*/1000);
  REQUIRE(res.has_value());
  const auto s = *res;
  CheckOptNear(s.mean, 1.0f);
}

TEST_CASE("FAIL large overrun: one flush per call and leftover time kept [stat]") {
  // sr=100, window=1s -> 100 samples. Push 307 samples at once: expect 1 flush and 7-sample carry.
  util::sound::StatCollector col("overrun", 100, 1.0, util::sound::StatCollector::Options::All());

  auto res = col.update(5.0f, /*samples_in_value=*/307);
  REQUIRE(res.has_value());  // exactly one flush per update() call
  const auto s = *res;
  CheckOptNear(s.min, 5.0f);
  CheckOptNear(s.max, 5.0f);
  CheckOptNear(s.mean, 5.0f);
  CheckOptNear(s.median, 5.0f);
  CheckOptNear(s.q5, 5.0f);
  CheckOptNear(s.q95, 5.0f);

  // Progress should be 7/100 seconds leftover
  CHECK(col.progress_seconds() == Approx(0.07).margin(1e-6));
}

TEST_CASE("FAIL set_window_seconds(0) clamps to 1.0 and hard-resets [stat]") {
  util::sound::StatCollector col("set0", 1000, 2.0, util::sound::StatCollector::Options::All());
  REQUIRE_FALSE(col.update(1.0f, /*samples_in_value=*/500).has_value());  // partial fill
  col.set_window_seconds(0.0);  // clamp to 1.0 and hard reset
  CHECK(col.window_seconds() == Approx(1.0));

  // Exactly 1s should now flush
  auto res = col.update(3.0f, /*samples_in_value=*/1000);
  REQUIRE(res.has_value());
  const auto s = *res;
  CheckOptNear(s.mean, 3.0f);
}

TEST_CASE("SUCCESS StatCollector matches reference stats on random vectors [stat]") {
  std::mt19937 rng(0xC0FFEEu);
  std::uniform_int_distribution<int> len_dist(1, 200);
  std::uniform_real_distribution<float> val_dist(-80.0f, 10.0f);

  for (int trial = 0; trial < 50; ++trial) {
    const int n = len_dist(rng);
    std::vector<float> vals; vals.reserve(n);
    for (int i = 0; i < n; ++i) vals.push_back(val_dist(rng));

    util::sound::StatCollector col("prop-ref", 1, static_cast<double>(n),
                                   util::sound::StatCollector::Options::All());

    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng);

    std::optional<util::sound::StatCollector::Stat> out;
    for (int k = 0; k < n; ++k) {
      out = col.update(vals[ idx[k] ], /*samples_in_value=*/1);
    }
    REQUIRE(out.has_value());

    const RefStats ref = MakeRefStats(vals);
    const auto s = *out;

    const float tolMean = 1e-6f;
    const float tolMedQ = 1e-5f;

    CheckOptNear(s.min, ref.minv, 0.0f);
    CheckOptNear(s.max, ref.maxv, 0.0f);
    CheckOptNear(s.mean, ref.mean, tolMean);
    CheckOptNear(s.median, ref.median, tolMedQ);
    CheckOptNear(s.q5, ref.q5, tolMedQ);
    CheckOptNear(s.q95, ref.q95, tolMedQ);

    CHECK(s.min.value() <= s.q5.value());
    CHECK(s.q5.value() <= s.median.value());
    CHECK(s.median.value() <= s.q95.value());
    CHECK(s.q95.value() <= s.max.value());
    CHECK(s.mean.value() >= s.min.value());
    CHECK(s.mean.value() <= s.max.value());
  }
}

TEST_CASE("SUCCESS StatCollector ignores missing values but matches reference [stat]") {
  std::mt19937 rng(0xBADC0DEu);
  std::uniform_int_distribution<int> len_dist(1, 300);
  std::uniform_real_distribution<float> val_dist(-50.0f, 50.0f);
  std::bernoulli_distribution keep_dist(0.7);

  for (int trial = 0; trial < 40; ++trial) {
    const int n = len_dist(rng);
    std::vector<float> kept;
    kept.reserve(n);

    util::sound::StatCollector col(
        "prop-nullopt",
        /*sample_rate=*/1,
        /*window_seconds=*/static_cast<double>(n),
        util::sound::StatCollector::Options::All());

    std::optional<util::sound::StatCollector::Stat> out;
    for (int i = 0; i < n; ++i) {
      const float v = val_dist(rng);
      if (keep_dist(rng)) {
        kept.push_back(v);
        out = col.update(v, /*samples_in_value=*/1);
      } else {
        out = col.advance(/*samples_in_value=*/1);
      }
    }

    REQUIRE(out.has_value());
    const auto s = *out;

    if (kept.empty()) {
      CHECK_FALSE(s.min.has_value());
      CHECK_FALSE(s.max.has_value());
      CHECK_FALSE(s.mean.has_value());
      CHECK_FALSE(s.median.has_value());
      CHECK_FALSE(s.q5.has_value());
      CHECK_FALSE(s.q95.has_value());
    } else {
      const RefStats ref = MakeRefStats(kept);
      const float tolMean = 1e-6f;
      const float tolMedQ = 1e-5f;
      CheckOptNear(s.min,    ref.minv,   0.0f);
      CheckOptNear(s.max,    ref.maxv,   0.0f);
      CheckOptNear(s.mean,   ref.mean,   tolMean);
      CheckOptNear(s.median, ref.median, tolMedQ);
      CheckOptNear(s.q5,     ref.q5,     tolMedQ);
      CheckOptNear(s.q95,    ref.q95,    tolMedQ);
    }
  }
}

TEST_CASE("SUCCESS StatCollector quantiles monotonic constant input [stat]") {
  {
    const int n = 1000;
    util::sound::StatCollector col("const", 1, static_cast<double>(n),
                                   util::sound::StatCollector::Options::All());
    std::optional<util::sound::StatCollector::Stat> out;
    for (int i = 0; i < n; ++i) out = col.update(3.14f, 1);
    REQUIRE(out.has_value());
    const auto s = *out;
    CheckOptNear(s.min, 3.14f, 0.0f);
    CheckOptNear(s.max, 3.14f, 0.0f);
    CheckOptNear(s.mean, 3.14f, 1e-7f);
    CheckOptNear(s.median, 3.14f, 0.0f);
    CheckOptNear(s.q5, 3.14f, 0.0f);
    CheckOptNear(s.q95, 3.14f, 0.0f);
  }

  // Stohastic signal
  {
    std::mt19937 rng(123456u);
    std::uniform_int_distribution<int> len_dist(5, 500);
    std::normal_distribution<float> val_norm(0.0f, 10.0f);

    for (int trial = 0; trial < 30; ++trial) {
      const int n = len_dist(rng);
      util::sound::StatCollector col("mono", 1, static_cast<double>(n),
                                     util::sound::StatCollector::Options::All());
      std::optional<util::sound::StatCollector::Stat> out;
      for (int i = 0; i < n; ++i) out = col.update(val_norm(rng), 1);
      REQUIRE(out.has_value());
      const auto s = *out;
      REQUIRE(s.min.has_value());
      REQUIRE(s.q5.has_value());
      REQUIRE(s.median.has_value());
      REQUIRE(s.q95.has_value());
      REQUIRE(s.max.has_value());
      CHECK(s.min.value() <= s.q5.value());
      CHECK(s.q5.value() <= s.median.value());
      CHECK(s.median.value() <= s.q95.value());
      CHECK(s.q95.value() <= s.max.value());
      CHECK(s.mean.value() >= s.min.value());
      CHECK(s.mean.value() <= s.max.value());
    }
  }
}
