
#include <catch.hpp>
#include <iostream>
#include <algorithm>
#include "tests/src/utils.h"
#include "spench/include/errors.h"
#include "spench/src/audio_processing.h"

struct TestAudioProcessing {
  std::unique_ptr<spench::AudioProcessing> m_audio_proc;

  const std::string m_kBase = "data/";
  std::vector<float> m_wav_data;

 public:
  TestAudioProcessing() : m_audio_proc(nullptr) {}
  TestAudioProcessing& operator=(const TestAudioProcessing&) = delete;
  TestAudioProcessing(const TestAudioProcessing&) = delete;
  TestAudioProcessing(TestAudioProcessing&& orig) :
                                          m_audio_proc(std::move(orig.m_audio_proc)),
                                          m_wav_data(std::move(orig.m_wav_data)) {
    orig.m_wav_data.clear();
    orig.m_audio_proc = nullptr;
  }

  spench::AudioProcessing& getAudioProc() {
    return *(m_audio_proc.get());
  }

  TestAudioProcessing& InitAudioProcessing() {
    m_audio_proc = std::make_unique<spench::AudioProcessing>(spench::AudioProcessing());
    return *this;
  }

  TestAudioProcessing& InitAudioProcessing(const std::string& config_name) {
    std::string config_path = m_kBase + "config/" + config_name;
    m_audio_proc = std::make_unique<spench::AudioProcessing>(
                                                spench::AudioProcessing(config_path));
    return *this;
  }

  TestAudioProcessing& InitAudioProcessing(const char* config_name) {
    return InitAudioProcessing(std::string(config_name));
  }

  TestAudioProcessing& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data);
    return *this;
  }
};

TEST_CASE("SUCCEED Create and run audio processing with AGC default parameters [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addAGC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });

  REQUIRE(greater_than_1.size() == 0);
  // AGC could increase signal upto 1.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });
  REQUIRE(greater_than_1.size() != 0);
}

TEST_CASE("SUCCEED Create and run audio processing with RC default parameters [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.75
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Create and run audio processing with AGC and RC default parameters [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addAGC().addRC();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  REQUIRE(source_data_size == prediction.size());
  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.75
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Create and run audio empty audio processing [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  IsVectorsClose(reference, prediction, 1e-5);
}

TEST_CASE("SUCCEED Create and run audio processing with AGC specific parameters [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  chunks.resize(kChunks_to_compare);

  audio_proc_test_env.getAudioProc().addAGC({0.9, 20.0, 48000 / 4, 0.01, 0.01});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::vector<float> greater_than_09;
  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_09),
    [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_09.size() != 0);
  // AGC could increase signal upto 0.8
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_09),
    [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_09.size() != 0);
}

TEST_CASE("SUCCEED Create and run audio processing with RC specific parameters [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_8;
  chunks.resize(kChunks_to_compare);

  audio_proc_test_env.getAudioProc().addRC({48000*5, 0.8, 1e-4});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_8),
               [](float& i) { return std::fabs(i) > 0.8; });

  REQUIRE(greater_than_0_8.size() != 0);
  greater_than_0_8.resize(0);
  REQUIRE(greater_than_0_8.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.8
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_8),
               [](float& i) { return std::fabs(i) > 0.8; });
  REQUIRE(greater_than_0_8.size() == 0);
}

TEST_CASE("FAIL Run audio processing with incorrect config file name or parameters [audio processing]") {
  REQUIRE_THROWS_WITH(TestAudioProcessing().
                      InitAudioProcessing("config.json"),
                      "Due to errors audio processing could not be created! ret_code: -10");
}

TEST_CASE("SUCCEED Run audio processing with reading AGC parameters from config [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_agc_enabled_24khz.yaml").
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addAGC();

  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });

  REQUIRE(greater_than_1.size() == 0);
  // AGC could increase signal upto 1.0
  std::copy_if(std::begin(prediction), std::end(prediction), std::back_inserter(greater_than_1),
    [](float& i) { return std::fabs(i) > 1.0; });
  REQUIRE(greater_than_1.size() != 0);
}

TEST_CASE("SUCCEED Run audio processing with reading RC parameters from config [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_rc_enabled_24khz.yaml").
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.75
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run audio processing with reading AGC and RC parameters from config [audio procesing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_dsp_enabled_24khz.yaml").
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addAGC().addRC();
  const uint32_t kChunk_size = 16;
  const uint32_t kChunk_stride = 16;
  const uint32_t kAudio_src_sample_rate = 48000;
  const uint32_t kChunks_to_compare = 200;
  constexpr uint32_t source_data_size = kChunks_to_compare * (kAudio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = chunkWav<float>(audio_proc_test_env.m_wav_data,
                                kAudio_src_sample_rate,
                                kChunk_size, kChunk_stride);
  std::vector<float> reference(audio_proc_test_env.m_wav_data.begin(),
                               audio_proc_test_env.m_wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  REQUIRE(source_data_size == prediction.size());
  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);
  REQUIRE(greater_than_0_75.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.75
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}
