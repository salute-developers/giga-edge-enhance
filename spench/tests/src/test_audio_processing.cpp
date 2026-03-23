
#include <dsplib.h>

#include <algorithm>
#include <filesystem>
#include <catch.hpp>
#include <spench/errors.h>

#include "spench/src/handler/audio_processor_handler.h"
#include "util/echo/echo_canceler.h"
#include "tests/src/utils.h"

struct TestAudioProcessing {
  std::unique_ptr<spench::AudioProcessorHandler> audio_proc;

  const std::string data_root = "data/";
  std::vector<float> wav_data;

 public:
  TestAudioProcessing() : audio_proc(nullptr) {}
  TestAudioProcessing& operator=(const TestAudioProcessing&) = delete;
  TestAudioProcessing(const TestAudioProcessing&) = delete;
  TestAudioProcessing(TestAudioProcessing&& orig) :
      audio_proc(std::move(orig.audio_proc)),
      wav_data(std::move(orig.wav_data)) {
    orig.wav_data.clear();
    orig.audio_proc = nullptr;
  }

  spench::AudioProcessorHandler& getAudioProc() {
    return *(audio_proc.get());
  }

  // Default init: use config reader with default settings.
  TestAudioProcessing& InitAudioProcessing() {
    // Use default-constructed AudioProcessorConfig (no path).
    auto reader = std::make_unique<spench::AudioProcessorConfig>();
    audio_proc = std::make_unique<spench::AudioProcessorHandler>(std::move(reader));
    return *this;
  }

  // Init with a specific YAML config file under data/config/.
  TestAudioProcessing& InitAudioProcessing(const std::string& yaml_conf) {
    auto reader = std::make_unique<spench::AudioProcessorConfig>(spench::StringConfigReader(yaml_conf));
    audio_proc = std::make_unique<spench::AudioProcessorHandler>(std::move(reader));
    return *this;
  }

  TestAudioProcessing& InitAudioProcessing(const std::filesystem::path& config_name) {
    auto reader = std::make_unique<spench::AudioProcessorConfig>(spench::FileConfigReader(cfgPath(config_name)));
    audio_proc = std::make_unique<spench::AudioProcessorHandler>(std::move(reader));
    return *this;
  }

  TestAudioProcessing& SetWav(const std::string& wav_path) {
    const std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_data);
    return *this;
  }

  [[nodiscard]] std::string cfgPath(const std::filesystem::path& rel) const {
    std::filesystem::path base{data_root};
    return (base / "config" / rel).string();
  }
};


TEST_CASE("SUCCEED Create and run audio processing with AGC default parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addAGC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Create and run audio processing with RC default parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Create and run audio processing with RC and NG [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC().addNoiseGate({48000, -20, 0.05, 0.02, 0.05});

  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Create and run audio empty audio processing [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  IsVectorsClose(reference, prediction, 1e-5);
}

TEST_CASE("SUCCEED Create and run audio processing with AGC specific parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  chunks.resize(chunks_to_compare);

  audio_proc_test_env.getAudioProc().addAGC({0.9, 20.0, 48000 / 4, 0.01, 0.01});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Create and run audio processing with RC specific parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_8;
  chunks.resize(chunks_to_compare);

  audio_proc_test_env.getAudioProc().addRC({48000, -4, 10, 0.05, 300, 5});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Run audio processing with reading AGC parameters from config [audio processing handler]") {
  auto audio_proc_test_env =
            std::move(TestAudioProcessing().
            InitAudioProcessing((std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml"))).
            SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addAGC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_1;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Run audio processing with reading RC parameters from config [audio processing handler]") {
  auto audio_proc_test_env =
          std::move(TestAudioProcessing().
          InitAudioProcessing(std::filesystem::path("default_configuration_fast_tflite_rc_enabled_24khz.yaml")).
          SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Run audio processing with reading LC parameters from config [audio processing handler]") {
  auto audio_proc_test_env =
          std::move(TestAudioProcessing().
              InitAudioProcessing(std::filesystem::path("default_configuration_fast_tflite_rc_enabled_24khz.yaml")));
  audio_proc_test_env.getAudioProc().addLC();
  constexpr int32_t fs = 48000;
  constexpr int32_t chunk_ms = 16;
  constexpr int32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (10 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;
  const int32_t bypass_time = 3;

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
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Run audio processing with reading RC and NG parameters from config [audio processing handler]") {
  auto audio_proc_test_env =
        std::move(TestAudioProcessing().
            InitAudioProcessing(std::filesystem::path("default_configuration_fast_tflite_dsp_enabled_24khz.yaml")).
            SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addRC().addNoiseGate();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_9;
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Create and run audio processing with changed NG default parameters [audio processing handler]") {
  const char change[] = R"(
sound_processing:
  noise_gate_ctrl:
    threshold: -15.0
)";
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing(std::string(change)).
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addNoiseGate();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Run audio processing with reading NG parameters from config [audio processing handler]") {
  auto audio_proc_test_env =
    std::move(TestAudioProcessing().
              InitAudioProcessing(std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml")).
              SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addNoiseGate();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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


TEST_CASE("SUCCEED Create and run audio processing with NG specific parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> equal_0;
  chunks.resize(chunks_to_compare);

  // set high threshold and noise gate will not suppress noise
  audio_proc_test_env.getAudioProc().addNoiseGate({48000, -40, 0.05, 0.02, 0.05});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Create and run audio processing with DRC default parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addDRCompressor();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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

TEST_CASE("SUCCEED Run audio processing with reading DRC parameters from config [audio processing handler]") {
  auto audio_proc_test_env =
          std::move(TestAudioProcessing().
                InitAudioProcessing(std::filesystem::path("default_configuration_fast_tflite_agc_enabled_24khz.yaml")).
                SetWav("test1_48_noised.wav"));
  audio_proc_test_env.getAudioProc().addDRCompressor();
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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


TEST_CASE("SUCCEED Create and run audio processing with DRC specific parameters [audio processing handler]") {
  auto audio_proc_test_env = std::move(TestAudioProcessing().
                                       InitAudioProcessing().
                                       SetWav("test1_48_noised.wav"));
  const uint32_t chunk_size = 16;
  const uint32_t chunk_stride = 16;
  const uint32_t audio_src_sample_rate = 48000;
  const uint32_t chunks_to_compare = 200;
  constexpr uint32_t source_data_size = chunks_to_compare * (audio_src_sample_rate / 1000) * 16;
  std::vector<float> prediction;
  auto chunks = ChunkWav<float>(audio_proc_test_env.wav_data,
                                audio_src_sample_rate,
                                chunk_size, chunk_stride);
  std::vector<float> reference(audio_proc_test_env.wav_data.begin(),
                               audio_proc_test_env.wav_data.begin() + source_data_size);
  std::vector<float> greater_than_0_75;
  chunks.resize(chunks_to_compare);

  // set low threshold = 0
  audio_proc_test_env.getAudioProc().addDRCompressor({48000, 0, 5, 0.0, 0.01, 0.2});

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    audio_proc_test_env.audio_proc->process(chunk, ret);
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
  constexpr int32_t fs = 48000;
  constexpr int32_t chunk_ms = 16;
  constexpr int32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t num_frames = (10 * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;
  constexpr int32_t filter_len = 8192;

  auto echo = util::echo::EchoCancelerBuilder()
                  .SetSampleRate(fs)
                  .SetFilterLen(filter_len)
                  .SetFrameLen(frame_len)
                  .SetType(util::echo::EchoCancelerType::SPEEX)
                  .SetDelayCompensator(true)
                  .Create();

  // loopback - noise, speech - sinus
  auto rir_flt = dsplib::FirFilterR(DumbRir(filter_len * 0.7));
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
  for (int32_t i = 0; i < num_frames; ++i) {
    const int32_t t1 = i * frame_len;
    const int32_t t2 = t1 + frame_len;
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

TEST_CASE("SUCCEED Create and run audio processing with Limiter default parameters [audio processing handler]") {
  auto env = std::move(TestAudioProcessing().InitAudioProcessing().SetWav("test1_48_noised.wav"));
  env.getAudioProc().addLimiter();

  const uint32_t fs = 48000;
  const uint32_t chunk_ms = 16;
  const uint32_t chunk = fs * chunk_ms / 1000;
  const uint32_t n_chunks = 200;
  const uint32_t total = n_chunks * chunk;

  std::vector<float> pred;
  auto chunks = ChunkWav<float>(env.wav_data, fs, chunk_ms, chunk_ms);
  chunks.resize(n_chunks);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& c = chunks[i];
    std::vector<float> out;
    env.audio_proc->process(c, out);
    pred.insert(pred.end(), out.begin(), out.end());
  }

  REQUIRE(pred.size() == total);

  // Limiter must hard-limit overs.
  std::vector<float> overs;
  std::copy_if(pred.begin(), pred.end(), std::back_inserter(overs),
               [](float v) { return std::fabs(v) > 1.0f; });
  REQUIRE(overs.empty());
}

TEST_CASE("SUCCEED Create and run audio processing with Limiter specific parameters [audio processing handler]") {
  auto env = std::move(TestAudioProcessing().InitAudioProcessing().SetWav("test1_48_noised.wav"));
  // Tight knee and low threshold to enforce limiting.
  env.getAudioProc().addLimiter({48000, -6.0, 0.0, 0.0, 0.05});

  const uint32_t fs = 48000;
  const uint32_t chunk_ms = 16;
  const uint32_t chunk = fs * chunk_ms / 1000;
  const uint32_t n_chunks = 200;
  const uint32_t total = n_chunks * chunk;

  std::vector<float> pred;
  auto chunks = ChunkWav<float>(env.wav_data, fs, chunk_ms, chunk_ms);
  chunks.resize(n_chunks);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& c = chunks[i];
    std::vector<float> out;
    env.audio_proc->process(c, out);
    pred.insert(pred.end(), out.begin(), out.end());
  }

  REQUIRE(pred.size() == total);

  // Values above ~0.5..0.7 should be reduced by limiter under -6 dB threshold.
  size_t gt075 = std::count_if(pred.begin(), pred.end(),
                               [](float v) { return std::fabs(v) > 0.75f; });
  REQUIRE(gt075 == 0);
}

TEST_CASE("SUCCEED Create and run LoudnessControl with default parameters [audio processing handler]") {
  auto env = std::move(TestAudioProcessing().InitAudioProcessing());
  env.getAudioProc().addLC({48000, -15, 30, 3, false});

  constexpr int32_t fs = 48000;
  constexpr int32_t chunk_ms = 16;
  constexpr int32_t frame_len = fs * chunk_ms / 1000;
  constexpr int32_t seconds = 10;
  constexpr int32_t num_frames = (seconds * fs) / frame_len;
  constexpr int32_t num_samples = num_frames * frame_len;
  const int32_t bypass_time = 3;

  const auto tt = dsplib::arange(num_samples) / fs;
  const auto in = 0.25 * sin(2 * dsplib::pi * 1000.0 * tt);

  dsplib::arr_real out;
  for (int32_t i = 0; i < num_frames; ++i) {
    const int32_t t1 = i * frame_len;
    const int32_t t2 = t1 + frame_len;
    const auto x = *in.slice(t1, t2);
    std::vector<float> chunk = x.to_vec();
    std::vector<float> ret;
    env.audio_proc->process(chunk, ret);
    out |= dsplib::arr_real(ret);
  }

  // RMS after bypass should be close to target program loudness.
  {
    auto rms_after = dsplib::mag2db(dsplib::rms(out.slice(fs * bypass_time, num_samples)));
    const dsplib::real_t target = -14.9;  // near default target_level_db
    REQUIRE(std::abs(rms_after - target) < 0.5);
  }

  // Before bypass, signal should be nearly unchanged.
  {
    auto rms_in = dsplib::mag2db(dsplib::rms(in.slice(0, fs * bypass_time)));
    auto rms_out = dsplib::mag2db(dsplib::rms(out.slice(0, fs * bypass_time)));
    REQUIRE(std::abs(rms_out - rms_in) < 0.5);
  }
}

TEST_CASE("SUCCEED ProcessFrame works with empty chain [audio processing handler]") {
  auto env = std::move(TestAudioProcessing().InitAudioProcessing().SetWav("test1_48_noised.wav"));

  const uint32_t fs = 48000;
  const uint32_t chunk_ms = 16;
  const uint32_t n_chunks = 10;

  auto chunks = ChunkWav<float>(env.wav_data, fs, chunk_ms, chunk_ms);
  chunks.resize(n_chunks);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    std::vector<float> out;
    auto rc = env.audio_proc->ProcessFrame(nullptr, chunks[i], out);
    REQUIRE(rc == SpenchRetCode::SUCCESS);
    REQUIRE(out.size() == chunks[i].size());
  }
}

TEST_CASE("FAIL ProcessFrame handles empty input gracefully [audio processing handler]") {
  auto env = std::move(TestAudioProcessing().InitAudioProcessing());
  std::vector<float> in;
  std::vector<float> out;

  auto rc = env.audio_proc->ProcessFrame(nullptr, in, out);
  REQUIRE(rc == SpenchRetCode::SUCCESS);
  REQUIRE(out.empty());
}

TEST_CASE("SUCCEED Processing order affects result (AGC+Limiter vs Limiter+AGC) [audio processing handler]") {
  auto env1 = std::move(TestAudioProcessing().InitAudioProcessing().SetWav("test1_48_noised.wav"));
  auto env2 = std::move(TestAudioProcessing().InitAudioProcessing().SetWav("test1_48_noised.wav"));

  env1.getAudioProc().addAGC().addLimiter();
  env2.getAudioProc().addLimiter().addAGC();

  const uint32_t fs = 48000;
  const uint32_t chunk_ms = 16;
  const uint32_t n_chunks = 40;

  auto chunks1 = ChunkWav<float>(env1.wav_data, fs, chunk_ms, chunk_ms);
  auto chunks2 = ChunkWav<float>(env2.wav_data, fs, chunk_ms, chunk_ms);
  chunks1.resize(n_chunks);
  chunks2.resize(n_chunks);

  std::vector<float> y1, y2;

  for (uint32_t i = 0; i < n_chunks; ++i) {
    std::vector<float> o1, o2;
    env1.audio_proc->process(chunks1[i], o1);
    env2.audio_proc->process(chunks2[i], o2);
    y1.insert(y1.end(), o1.begin(), o1.end());
    y2.insert(y2.end(), o2.begin(), o2.end());
  }

  // Order should change samples for at least some portion of data.
  size_t diff = 0;
  for (size_t i = 0; i < y1.size(); ++i) {
    if (std::fabs(y1[i] - y2[i]) > 1e-6f) { ++diff; }
  }
  REQUIRE(diff > 0);
}

TEST_CASE("SUCCEED Run audio processing with reading Limiter parameters from config [audio processing handler]") {
  auto env =
        std::move(TestAudioProcessing().
              InitAudioProcessing(std::filesystem::path("default_configuration_fast_tflite_dsp_enabled_24khz.yaml")).
              SetWav("test1_48_noised.wav"));
  env.getAudioProc().addLimiter();

  const uint32_t fs = 48000;
  const uint32_t chunk_ms = 16;
  const uint32_t frame_len = fs * chunk_ms / 1000;
  const uint32_t n_chunks = 200;
  const uint32_t total = n_chunks * frame_len;

  std::vector<float> pred;
  auto chunks = ChunkWav<float>(env.wav_data, fs, chunk_ms, chunk_ms);
  chunks.resize(n_chunks);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    std::vector<float> out;
    env.audio_proc->process(chunks[i], out);
    pred.insert(pred.end(), out.begin(), out.end());
  }

  REQUIRE(pred.size() == total);

  // Ensure no overs after limiting with config params.
  const bool has_over = std::any_of(pred.begin(), pred.end(),
                                    [](float v) { return std::fabs(v) > 1.0f; });
  REQUIRE(has_over == false);
}

TEST_CASE("FAIL Run audio processing with YAML string non-scalar [audio processing handler]") {
const char bad[] = R"(
sound_processing:
  range_ctrl:
    sample_rate: [48000]        # must be scalar
    target_level_db: -3.0
    max_gain_db: 10
    attack_time: 0.0
    release_time: 300
    hold_time: 5
)";
  REQUIRE_THROWS_AS(TestAudioProcessing().InitAudioProcessing(std::string(bad)), std::runtime_error);
}

TEST_CASE("SUCCESS Run audio processing with YAML string and init nodes [audio processing handler]") {
const char change[] = R"(
sound_processing:
  dynamic_range_compressor_ctrl:
    sample_rate: 24000
    threshold: -15.0
    ratio: 5
    knee_width: 0.0
    attack_time: 0.01
    release_time: 0.2
  loudness_ctrl:
    sample_rate: 48000
    target_level_db: -3.0
    bypass_time: 0.2
    max_gain_db: 20
  limiter_ctrl:
    sample_rate: 24000
    threshold: -2.0
    knee_width: 0.1
    attack_time: 0.1
    release_time: 0.2
)";
  // Construct from YAML string
  auto env = std::move(TestAudioProcessing().
                       InitAudioProcessing(std::string(change)).
                       SetWav("test1_48_noised.wav"));

  // Build a small chain reading params from YAML
  REQUIRE_NOTHROW(env.getAudioProc().addRC());
  REQUIRE_NOTHROW(env.getAudioProc().addAGC());
  REQUIRE_NOTHROW(env.getAudioProc().addNoiseGate());
  REQUIRE_NOTHROW(env.getAudioProc().addDRCompressor());
  REQUIRE_NOTHROW(env.getAudioProc().addLimiter());
  REQUIRE_NOTHROW(env.getAudioProc().addLC());
}
