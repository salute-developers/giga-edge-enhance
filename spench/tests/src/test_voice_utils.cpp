#include "tests/src/utils.h"
#include <catch.hpp>
#include <vector>
#include <string>

#include <spench/util/sound/resampler.h>
#include <spench/util/sound/sound_stream.h>

struct TestVoiceUtils {
  const std::string m_kBase = "data/";

 public:
  uint16_t m_chunk_size_ms = 16;
  uint16_t m_stride_size_ms = 16;
  uint32_t m_src_sample_rate = 16000;
  uint32_t m_dst_sample_rate = 16000;
  std::vector<float> m_src_wav_data;
  std::vector<float> m_dst_wav_data;
  TestVoiceUtils() {}
  TestVoiceUtils& operator=(const TestVoiceUtils&) = delete;
  TestVoiceUtils(const TestVoiceUtils&) = delete;
  TestVoiceUtils(TestVoiceUtils&& orig) :
          m_chunk_size_ms(orig.m_chunk_size_ms),
          m_stride_size_ms(orig.m_stride_size_ms),
          m_src_sample_rate(orig.m_src_sample_rate),
          m_dst_sample_rate(orig.m_dst_sample_rate),
          m_src_wav_data(std::move(orig.m_src_wav_data)),
          m_dst_wav_data(std::move(orig.m_dst_wav_data)) {
    orig.m_src_wav_data.clear();
    orig.m_dst_wav_data.clear();
  }

  TestVoiceUtils& Init(uint16_t chunk_size,
                       uint16_t stride_size,
                       uint64_t src_sr,
                       uint64_t dst_sr) {
    m_chunk_size_ms = chunk_size;
    m_stride_size_ms = stride_size;
    m_src_sample_rate = src_sr;
    m_dst_sample_rate = dst_sr;
    return *this;
  }

  TestVoiceUtils& SetWav(const std::string& wav_path, std::vector<float>& wav_file) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), wav_file);
    return *this;
  }

  TestVoiceUtils& SetSrcAudio(const std::string& wav_path) {
    return SetWav(wav_path, m_src_wav_data);
  }

  TestVoiceUtils& SetDstAudio(const std::string& wav_path) {
    return SetWav(wav_path, m_dst_wav_data);
  }
};

#ifndef BUILD_WITHOUT_SOXR
TEST_CASE("FAIL Create resampler with equal sample rates [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                    Init(16, 16, 48000, 48000));

  CHECK_THROWS_WITH(util::sound::BuildResampler(voice_utils.m_src_sample_rate,
                                                voice_utils.m_dst_sample_rate, 1),
                    "BuildResampler: in_rate equals out_rate.");
}

TEST_CASE("SUCCEED Run offline audio downsampling 48kHz -> 24kHz [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(16, 16, 48000, 24000).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));
  util::sound::ResampleWrapper resampler =
                      util::sound::BuildResampler(voice_utils.m_src_sample_rate,
                                                  voice_utils.m_dst_sample_rate, 1);
  std::vector<float> output_data = util::sound::ResampleVec(resampler,
                                                            voice_utils.m_src_wav_data,
                                                            voice_utils.m_src_sample_rate,
                                                            voice_utils.m_dst_sample_rate);
  std::vector<float> ending_part_of_data = util::sound::FlushVec<float>(resampler);

  output_data.insert(end(output_data), begin(ending_part_of_data), end(ending_part_of_data));

  if (output_data.size() >= voice_utils.m_dst_wav_data.size()) {
    output_data.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(output_data.size());
  }

  IsVectorsClose(voice_utils.m_dst_wav_data, output_data, 0.003);
}

TEST_CASE("SUCCEED Run offline audio upsampling 24kHz -> 48kHz [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(16, 16, 24000, 48000).
                                     SetSrcAudio("rec_noise_001_24k.wav").
                                     SetDstAudio("rec_noise_001_48k.wav"));

  util::sound::ResampleWrapper resampler =
                      util::sound::BuildResampler(voice_utils.m_src_sample_rate,
                                                  voice_utils.m_dst_sample_rate, 1);
  std::vector<float> output_data = util::sound::ResampleVec(resampler,
                                                            voice_utils.m_src_wav_data,
                                                            voice_utils.m_src_sample_rate,
                                                            voice_utils.m_dst_sample_rate);

  std::vector<float> ending_part_of_data = util::sound::FlushVec<float>(resampler);

  output_data.insert(end(output_data), begin(ending_part_of_data), end(ending_part_of_data));
  IsVectorsClose(voice_utils.m_dst_wav_data, output_data, 0.0001);
}

TEST_CASE("SUCCEED Run stream audio downsampling 48kHz -> 24kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));

  util::sound::ResampleWrapper resampler =
                      util::sound::BuildResampler(voice_utils.m_src_sample_rate,
                                                  voice_utils.m_dst_sample_rate, 1);
  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);

  auto dst_chunks = chunkWav<float>(voice_utils.m_dst_wav_data,
                                    voice_utils.m_dst_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  CHECK(src_chunks.size() == dst_chunks.size());
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];

    std::vector<float> output_data = util::sound::ResampleVec(resampler, s_chunk,
                                                              voice_utils.m_src_sample_rate,
                                                              voice_utils.m_dst_sample_rate);
    if (output_data.size() > 0) {
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data = util::sound::FlushVec<float>(resampler);
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }

  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio downsampling via sound stream interface 44.1kHz -> 24kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 44100;
  const uint32_t kAudio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_24_clean.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                          voice_utils.m_dst_sample_rate,
                                                          voice_utils.m_chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);
    if (output_data.size() > 0) {
      CHECK(output_data.size() == kChunk_size * kAudio_dst_sample_rate / 1000);
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio downsampling via sound stream interface 32kHz -> 24kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 32000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test3_32_noised.wav").
                                     SetDstAudio("test3_24_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                          voice_utils.m_dst_sample_rate,
                                                          voice_utils.m_chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);
    if (output_data.size() > 0) {
      CHECK(output_data.size() == kChunk_size * kAudio_dst_sample_rate / 1000);
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCEED Run stream audio upsampling 24kHz -> 48kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("rec_noise_001_24k.wav").
                                     SetDstAudio("rec_noise_001_48k.wav"));

  util::sound::ResampleWrapper resampler =
                      util::sound::BuildResampler(voice_utils.m_src_sample_rate,
                                                  voice_utils.m_dst_sample_rate, 1);
  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);

  auto dst_chunks = chunkWav<float>(voice_utils.m_dst_wav_data,
                                    voice_utils.m_dst_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  CHECK(src_chunks.size() == dst_chunks.size());
  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = util::sound::ResampleVec(resampler, s_chunk,
                                                              voice_utils.m_src_sample_rate,
                                                              voice_utils.m_dst_sample_rate);
    if (output_data.size() > 0) {
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data = util::sound::FlushVec<float>(resampler);
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }

  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio upsampling via sound stream interface 24kHz -> 44.1kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test2_24_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                          voice_utils.m_dst_sample_rate,
                                                          voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio upsampling via sound stream interface 24kHz -> 32kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 32000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test3_24_noised.wav").
                                     SetDstAudio("test3_32_upsampled_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                          voice_utils.m_dst_sample_rate,
                                                          voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run stream audio downsampling via sound stream interface 48kHz -> 24kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                          voice_utils.m_dst_sample_rate,
                                                          voice_utils.m_chunk_size_ms,
                                                          util::sound::TemplateType::INPUT);

  std::vector<float> result;
  for (uint32_t i = 0; i < src_chunks.size(); ++i) {
    auto& s_chunk = src_chunks[i];
    std::vector<float> output_data = input_sound_stream->ReadChunk(s_chunk);

    if (output_data.size() > 0) {
      CHECK(output_data.size() == kChunk_size * kAudio_dst_sample_rate / 1000);
      result.insert(end(result), begin(output_data), end(output_data));
    }
  }

  std::vector<float> output_data =
                            input_sound_stream->FinalizeStream();
  result.insert(end(result), begin(output_data), end(output_data));

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run sound stream for audious with the equal sample rate [sound]") {
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(16, 16, 48000, 48000).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_48_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream = util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                          voice_utils.m_dst_sample_rate,
                                                          voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }

  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}

TEST_CASE("SUCCESS Run sound stream for sequenced downsampling and upsampling 48kHz -> 48kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kAudio_dst_sample_rate = 48000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test1_48_noised.wav").
                                     SetDstAudio("test1_48_upsampled_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                             voice_utils.m_dst_sample_rate / 2,
                                                             voice_utils.m_chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream<float>(voice_utils.m_dst_sample_rate / 2,
                                                             voice_utils.m_src_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.005);
}

TEST_CASE("SUCCESS Run sound stream for sequenced downsampling and upsampling 44.1kHz -> 44.1kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 44100;
  const uint32_t kAudio_model_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                             kAudio_model_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream<float>(kAudio_model_sample_rate,
                                                             voice_utils.m_src_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.005);
}

TEST_CASE("SUCCESS Run sound stream for sequenced downsampling and upsampling 32kHz -> 32kHz [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 32000;
  const uint32_t kAudio_model_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 32000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test3_32_noised.wav").
                                     SetDstAudio("test3_32_upsampled_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                             kAudio_model_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream<float>(kAudio_model_sample_rate,
                                                             voice_utils.m_src_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  // due to finalization error is a bit bigger
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.015);
}

#else
TEST_CASE("FAIL Create input resampler [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 44100;
  const uint32_t kAudio_model_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);

  CHECK_THROWS_WITH(util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                             kAudio_model_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
                                                             util::sound::TemplateType::INPUT),
                    std::string("SoundStream::SoundStream: Library built without resampling ability. Please ") +
                    std::string("use a sample rate that is supported by model!"));
}

TEST_CASE("FAIL Create output resampler [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 44100;
  const uint32_t kAudio_model_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 44100;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test2_44_1_clean.wav").
                                     SetDstAudio("test2_44_1_upsampled_clean.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);

  CHECK_THROWS_WITH(util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                             kAudio_model_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
                                                             util::sound::TemplateType::OUTPUT),
                    std::string("SoundStream::SoundStream: Library built without resampling ability. Please ") +
                    std::string("use a sample rate that is supported by model!"));
}

TEST_CASE("SUCCESS Run sound stream without resampling [sound]") {
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 24000;
  const uint32_t kAudio_model_sample_rate = 24000;
  const uint32_t kAudio_dst_sample_rate = 24000;
  auto voice_utils = std::move(TestVoiceUtils().
                                     Init(kChunk_size, kChunk_stride,
                                          kAudio_src_sample_rate, kAudio_dst_sample_rate).
                                     SetSrcAudio("test1_24_noised.wav").
                                     SetDstAudio("test1_24_noised.wav"));

  auto src_chunks = chunkWav<float>(voice_utils.m_src_wav_data,
                                    voice_utils.m_src_sample_rate,
                                    voice_utils.m_chunk_size_ms,
                                    voice_utils.m_stride_size_ms);
  auto input_sound_stream =
                        util::sound::BuildSoundStream<float>(voice_utils.m_src_sample_rate,
                                                             kAudio_model_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
                                                             util::sound::TemplateType::INPUT);

  auto output_sound_stream =
                        util::sound::BuildSoundStream<float>(kAudio_model_sample_rate,
                                                             voice_utils.m_src_sample_rate,
                                                             voice_utils.m_chunk_size_ms,
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

  if (result.size() >= voice_utils.m_dst_wav_data.size()) {
    result.resize(voice_utils.m_dst_wav_data.size());
  } else {
    voice_utils.m_dst_wav_data.resize(result.size());
  }
  // due to finalization error is a bit bigger
  IsVectorsClose(voice_utils.m_dst_wav_data, result, 0.0001);
}
#endif
