
#include <dsplib.h>

#include <algorithm>
#include <catch.hpp>

#include <spench/errors.h>
#include "spench/src/audio_processing.h"
#include "tests/src/utils.h"
#include "util/echo/echo_canceler.h"

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

TEST_CASE("SUCCEED Create and run audio processing with AGC default parameters [audio processing]") {
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

TEST_CASE("SUCCEED Create and run audio processing with RC default parameters [audio processing]") {
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
  std::vector<float> greater_than_0_9;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);
  REQUIRE(greater_than_0_9.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.9
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);
}

TEST_CASE("SUCCEED Create and run audio processing with RC and NG [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC().addNoiseGate({48000, -20, 0.05, 0.02, 0.05});

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
  std::vector<float> greater_than_0_9;
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  REQUIRE(source_data_size == prediction.size());
  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });
  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0_size = equal_0.size();
  equal_0.resize(0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });
  REQUIRE(equal_0.size() > orig_equal_0_size);
}

TEST_CASE("SUCCEED Create and run audio empty audio processing [audio processing]") {
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

TEST_CASE("SUCCEED Create and run audio processing with AGC specific parameters [audio processing]") {
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

TEST_CASE("SUCCEED Create and run audio processing with RC specific parameters [audio processing]") {
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

  audio_proc_test_env.getAudioProc().addRC({48000, -4, 10, 0.05, 300, 5});

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

TEST_CASE("SUCCEED Run audio processing with reading AGC parameters from config [audio processing]") {
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

TEST_CASE("SUCCEED Run audio processing with reading RC parameters from config [audio processing]") {
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
  std::vector<float> greater_than_0_9;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);
  REQUIRE(greater_than_0_9.size() == 0);
  // RC leave signal as it was if signal's values are not greater 0.9
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);
}

TEST_CASE("SUCCEED Run audio processing with reading LC parameters from config [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_rc_enabled_24khz.yaml"));
  audio_proc_test_env.getAudioProc().addLC();
  constexpr int fs = 48000;
  constexpr int chunk_ms = 16;
  constexpr int frame_len = fs * chunk_ms / 1000;
  constexpr int num_frames = (10 * fs) / frame_len;
  constexpr int num_samples = num_frames * frame_len;
  const int bypass_time = 3;

  const auto tt = dsplib::arange(num_samples) / fs;
  const auto in = 0.5 * sin(2 * dsplib::pi * 440.0 * tt);

  auto rms_before = dsplib::mag2db(dsplib::rms(in));

  // process frame by frame
  dsplib::arr_real out;
  for (uint32_t i = 0; i < num_frames; ++i) {
    const uint32_t t1 = i * frame_len;
    const uint32_t t2 = t1 + frame_len;
    const auto x = *in.slice(t1, t2);
    std::vector<float> chunk = x.to_vec();
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    out |= dsplib::arr_real(ret);
  }

  // rms changed (after bypass time)
  {
    auto rms_gained = dsplib::mag2db(dsplib::rms(out.slice(fs * bypass_time, num_samples)));
    const dsplib::real_t rms_target = -22.31;
    REQUIRE(std::abs(rms_gained - rms_target) < 0.5);
  }

  // rms not changed (before bypass time)
  {
    auto rms_bypassed = dsplib::mag2db(dsplib::rms(out.slice(0, fs * bypass_time)));
    REQUIRE(std::abs(rms_bypassed - rms_before) < 0.5);
  }
}

TEST_CASE("SUCCEED Run audio processing with reading RC and NG parameters from config [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_dsp_enabled_24khz.yaml").
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC().addNoiseGate();
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
  std::vector<float> greater_than_0_9;
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  REQUIRE(source_data_size == prediction.size());
  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });

  REQUIRE(greater_than_0_9.size() != 0);
  greater_than_0_9.resize(0);

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });
  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0_size = equal_0.size();
  equal_0.resize(0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_9),
               [](float& i) { return std::fabs(i) > 0.9; });
  REQUIRE(greater_than_0_9.size() == 0);

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });
  REQUIRE(equal_0.size() > orig_equal_0_size);
}

TEST_CASE("SUCCEED Create and run audio processing with NG default parameters [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addNoiseGate();
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
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() > orig_equal_0);
}

TEST_CASE("SUCCEED Run audio processing with reading NG parameters from config [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_agc_enabled_24khz.yaml").
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addNoiseGate();
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
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() > orig_equal_0);
}


TEST_CASE("SUCCEED Create and run audio processing with NG specific parameters [audio processing]") {
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
  std::vector<float> equal_0;
  chunks.resize(kChunks_to_compare);

  // set high threshold and noise gate will not suppress noise
  audio_proc_test_env.getAudioProc().addNoiseGate({48000, -40, 0.05, 0.02, 0.05});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.m_audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }
  REQUIRE(source_data_size == prediction.size());

  std::copy_if(std::begin(reference), std::end(reference),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() != 0);

  auto orig_equal_0 = equal_0.size();
  equal_0.resize(0);
  REQUIRE(equal_0.size() == 0);
  // NoiseGate sets all values which less than threshold to 0.0
  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() == orig_equal_0);
}

TEST_CASE("SUCCEED Create and run audio processing with DRC default parameters [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addDRCompressor();
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

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run audio processing with reading DRC parameters from config [audio processing]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing("default_configuration_fast_tflite_agc_enabled_24khz.yaml").
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addDRCompressor();
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

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}


TEST_CASE("SUCCEED Create and run audio processing with DRC specific parameters [audio processing]") {
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

  // set low threshold = 0
  audio_proc_test_env.getAudioProc().addDRCompressor({48000, 0, 5, 0.0, 0.01, 0.2});

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

  std::copy_if(std::begin(prediction), std::end(prediction),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() != 0);
}

TEST_CASE("SUCCEED Create and process speex echo canceler [echo canceling]") {
  constexpr int fs = 48000;
  constexpr int chunk_ms = 16;
  constexpr int frame_len = fs * chunk_ms / 1000;
  constexpr int num_frames = (10 * fs) / frame_len;
  constexpr int num_samples = num_frames * frame_len;
  constexpr int filter_len = 8192;

  auto echo = util::echo::EchoCancelerBuilder()
                  .SetSampleRate(fs)
                  .SetFilterLen(filter_len)
                  .SetFrameLen(frame_len)
                  .SetType(util::echo::EchoCancelerType::SPEEX)
                  .SetDelayCompensator(true)
                  .Create();

  // loopback - noise, speech - sinus
  auto rir_flt = dsplib::FirFilterR(DumbRIR(filter_len * 0.7));
  auto tt = dsplib::arange(num_samples) / fs;
  auto sig = 0.1 * sin(2 * dsplib::pi * 440.0 * tt);
  auto ref = 0.5 * dsplib::rand({-1, 1}, num_samples);
  auto noise = rir_flt(ref);
  auto mic = 0.1 * (sig + noise);

  // add noise (nonlinear effects)
  mic += 1e-5 * dsplib::randn(num_samples);
  ref += 1e-5 * dsplib::randn(num_samples);

  // delay mic signal by `filter_len` samples (aec will not work without delay compensator)
  mic = dsplib::delayseq(mic, filter_len);

  const auto snr_before = dsplib::mag2db(rms(sig) / rms(noise));

  // process frame by frame
  dsplib::arr_real out;
  for (int i = 0; i < num_frames; ++i) {
    const int t1 = i * frame_len;
    const int t2 = t1 + frame_len;
    const auto d = *mic.slice(t1, t2);
    const auto x = *ref.slice(t1, t2);
    const auto y = echo->Process(d.to_vec(), x.to_vec());
    out |= dsplib::arr_real(y);
  }

  // skip first 2 second
  out = out.slice(fs * 2, num_samples);
  auto spec = dsplib::welch(out, 1024).pxx;
  const auto snr_after = dsplib::sinad(spec, dsplib::SinadType::Psd);

  REQUIRE(snr_after - snr_before >= 20);
}
