#include "contrib/yaml-cpp/yaml.h"
#include "spench/src/feature_extractor_runner.h"
#include <memory>
#include <catch.hpp>
#include <chrono> // NOLINT

#include "tests/src/utils.h"

struct TestFERunner {
  std::unique_ptr<spench::FeatureExtractorRunner> m_fe_runner_ptr;
  const std::size_t m_kNattempts = 1000;
  std::vector<float> m_wav_data;
  const std::string m_kBase = "data/";

  TestFERunner() {}
  TestFERunner& operator=(const TestFERunner&) = delete;
  TestFERunner(const TestFERunner&) = delete;
  TestFERunner(TestFERunner&& orig) :
          m_fe_runner_ptr(std::move(orig.m_fe_runner_ptr)),
          m_wav_data(std::move(orig.m_wav_data)) {
    orig.m_wav_data.clear();
    orig.m_fe_runner_ptr = nullptr;
  }

  TestFERunner& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data);
    return *this;
  }

  TestFERunner& LoadDataFromBin(const std::string& dump, uint32_t step_size_samples,
                        std::vector<std::vector<float>>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFERunner& LoadSpecDataFromBin(const std::string& dump,
                        std::vector<std::vector<float>>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    uint32_t step_size_samples =
            m_fe_runner_ptr.get()->GetPowSpecFeatSizeSamples();
    loadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFERunner& LoadDataFromBin(const std::string& dump,
                        std::vector<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestFERunner& LoadDataFromBin(const std::string& dump,
                        std::valarray<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }

  YAML::Node GetYAML(const std::string& yaml_config_name) {
    std::string file_path = m_kBase + "config/" + yaml_config_name;
    return YAML::LoadFile(file_path);
  }

  TestFERunner& InitFERunner(const std::string& yaml_config_name) {
    auto config = GetYAML(yaml_config_name);
    m_fe_runner_ptr =
              std::unique_ptr<spench::FeatureExtractorRunner>
              (new spench::FeatureExtractorRunner(config["feature_params"]));
    return *this;
  }
};

TEST_CASE("SUCCEED Launch runner for denoiser input generation with default parameters (audio 16kHz) [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));
  constexpr size_t kIterations = 1 + 1;

  std::vector<std::vector<float>> reference_mag;
  std::vector<std::vector<float>> result_mag;
  test_data.LoadSpecDataFromBin("10_mag_stft_tensors.dump", reference_mag);

  REQUIRE(test_data.m_fe_runner_ptr.get()->GetSamplingFreq() == 16000);
  REQUIRE(test_data.m_fe_runner_ptr.get()->GetFrameSizeMs() == 32);
  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::ORIG_CHUNK;
  std::vector<float> audio(test_data.m_wav_data.begin(),
                           test_data.m_wav_data.begin() +
                           test_data.m_fe_runner_ptr.get()->GetFrameHopSizeSamples() * kIterations);
  denoise_task.m_data.m_waveform = audio;
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 0);

  test_data.m_fe_runner_ptr.get()->Run(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::FOURIER_TRANSFORMED_SPEC);
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 1);

  for (size_t i = 0; i < kIterations - 1 ; ++i) {
    result_mag.push_back(denoise_task.m_data.m_input_spectrogram[i].getMagnitude());
  }

  IsVectorsClose(reference_mag[0], result_mag[0], 0.0001);
}

TEST_CASE("SUCCEED Launch runner for masking with default parameters (audio 16kHz) [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));
  std::vector<float> reference_signal;
  std::valarray<float> predicted_mask_real;
  std::valarray<float> predicted_mask_imag;
  std::valarray<float> prev_predicted_mask_real;
  std::valarray<float> prev_predicted_mask_imag;
  std::vector<float> prev_features_real;
  std::vector<float> prev_features_imag;

  test_data.LoadDataFromBin("1_enhanced_audio_vector_16khz.dump", reference_signal);
  test_data.LoadDataFromBin("1_predicted_mask_real_16khz.dump", predicted_mask_real);
  test_data.LoadDataFromBin("1_predicted_mask_imag_16khz.dump", predicted_mask_imag);
  test_data.LoadDataFromBin("1_previous_predicted_mask_real_16khz.dump", prev_predicted_mask_real);
  test_data.LoadDataFromBin("1_previous_predicted_mask_imag_16khz.dump", prev_predicted_mask_imag);
  test_data.LoadDataFromBin("1_previous_features_real_16khz.dump", prev_features_real);
  test_data.LoadDataFromBin("1_previous_features_imag_16khz.dump", prev_features_imag);

  spench::ComplexNumber<float> prev_features(prev_features_real, prev_features_imag);
  spench::SpenchTask denoise_task;

  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  // update previous features for test
  denoise_task.m_data.m_context->m_prev_features.push(prev_features);
  denoise_task.m_data.m_context->m_prev_features.push(prev_features);
  denoise_task.m_stage = spench::PipelineStage::PREDICTED_MASK;

  REQUIRE(test_data.m_fe_runner_ptr.get()->GetSamplingFreq() == 16000);
  REQUIRE(test_data.m_fe_runner_ptr.get()->GetFrameSizeMs() == 32);

  denoise_task.m_data.m_new_spectrogram.real = predicted_mask_real;
  denoise_task.m_data.m_new_spectrogram.imag = predicted_mask_imag;
  // add random value to avoid crashes because of empty contaner
  denoise_task.m_data.m_input_spectrogram.push_back(prev_features);

  denoise_task.m_data.m_context->m_prev_spectrogram.real = prev_predicted_mask_real;
  denoise_task.m_data.m_context->m_prev_spectrogram.imag = prev_predicted_mask_imag;

  REQUIRE(denoise_task.m_data.m_enhanced_wav.size() == 0);
  test_data.m_fe_runner_ptr.get()->Run(&denoise_task);

  REQUIRE(denoise_task.m_data.m_enhanced_wav.size() ==
                                  test_data.m_fe_runner_ptr.get()->GetFrameHopSizeSamples());
  REQUIRE(denoise_task.m_stage == spench::PipelineStage::ENHANCED_CHUNK);

  IsVectorsClose(reference_signal, denoise_task.m_data.m_enhanced_wav, 0.05);
}

TEST_CASE("SUCCEED Read config for runner with modified .yaml file and check values [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("test_configuration_with_no_default_values.yaml").
                                     SetWav("rec_noise_001_16k.wav"));

  REQUIRE(test_data.m_fe_runner_ptr.get()->GetSamplingFreq() == 8000);
  REQUIRE(test_data.m_fe_runner_ptr.get()->GetFrameSizeMs() == 40);
}

TEST_CASE("FAIL Read config for runner with incorrect sampling frequency value [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));

  YAML::Node config = test_data.GetYAML("test_configuration_with_incorrect_sampling_freq.yaml");
  CHECK_THROWS(test_data.m_fe_runner_ptr.get()->ReadConfig(config["feature_params"]));
}

TEST_CASE("FAIL Read config for runner with incorrect frame size value [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));

  YAML::Node config = test_data.GetYAML("test_configuration_with_incorrect_frame_size.yaml");
  CHECK_THROWS_WITH(test_data.m_fe_runner_ptr.get()->ReadConfig(config["feature_params"]),
                                                "Config reading error. Frame size is not a scalar!");
}

TEST_CASE("FAIL Read config for runner with incorrect window hop value [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));

  YAML::Node config = test_data.GetYAML("test_configuration_with_incorrect_window_hop.yaml");
  CHECK_THROWS_WITH(test_data.m_fe_runner_ptr.get()->ReadConfig(config["feature_params"]),
                                         "Config reading error. Window hop length is not a scalar!");
}

TEST_CASE("FAIL Read config for runner with incorrect window type value [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));

  YAML::Node config = test_data.GetYAML("test_configuration_with_incorrect_window_type.yaml");
  CHECK_THROWS_WITH(test_data.m_fe_runner_ptr.get()->ReadConfig(config["feature_params"]),
                                         "Window type must be in [\"hanning\", \"hamming\"]");
}

TEST_CASE("SUCCEED Launch runner with configuration read from .yaml config (audio 16kHz) [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));
  constexpr size_t kIterations = 1 + 1;

  std::vector<std::vector<float>> reference_mag;
  std::vector<std::vector<float>> result_mag;
  test_data.LoadSpecDataFromBin("10_mag_stft_tensors.dump", reference_mag);

  REQUIRE(test_data.m_fe_runner_ptr.get()->GetSamplingFreq() == 16000);
  REQUIRE(test_data.m_fe_runner_ptr.get()->GetFrameSizeMs() == 32);

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::ORIG_CHUNK;
  std::vector<float> audio(test_data.m_wav_data.begin(),
                           test_data.m_wav_data.begin() +
                           test_data.m_fe_runner_ptr.get()->GetFrameHopSizeSamples() * kIterations);
  denoise_task.m_data.m_waveform = audio;
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 0);

  test_data.m_fe_runner_ptr.get()->Run(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::FOURIER_TRANSFORMED_SPEC);
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 1);

  for (size_t i = 0; i < kIterations - 1 ; ++i) {
    result_mag.push_back(denoise_task.m_data.m_input_spectrogram[i].getMagnitude());
  }

  IsVectorsClose(reference_mag[0], result_mag[0], 0.0001);
}

TEST_CASE("SUCCEED Launch runner for iSTFT calc with skipped frame and check states (audio 16kHz) [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));
  constexpr size_t kIterations = 1 + 1;

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::SKIPPED_CHUNK;
  std::vector<float> audio(test_data.m_wav_data.begin(),
                           test_data.m_wav_data.begin() +
                           test_data.m_fe_runner_ptr.get()->GetFrameHopSizeSamples() * kIterations);
  denoise_task.m_data.m_waveform = audio;
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 0);

  test_data.m_fe_runner_ptr.get()->Run(&denoise_task);

  CHECK(denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK);
  REQUIRE(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 0);
}

TEST_CASE("SUCCEED Launch runner for iSTFT calc with first valid chunk and check states (audio 16kHz) [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));
  constexpr size_t kIterations = 1 + 1;

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  std::valarray<float> predicted_mask_real;
  std::valarray<float> predicted_mask_imag;
  test_data.LoadDataFromBin("1_predicted_mask_real_16khz.dump", predicted_mask_real);
  test_data.LoadDataFromBin("1_predicted_mask_imag_16khz.dump", predicted_mask_imag);

  std::vector<float> audio(test_data.m_wav_data.begin(),
                           test_data.m_wav_data.begin() +
                           test_data.m_fe_runner_ptr.get()->GetFrameHopSizeSamples() * kIterations);
  denoise_task.m_data.m_waveform = audio;
  for (uint16_t i = 0; i < 3; ++i) {
    denoise_task.m_stage = spench::PipelineStage::ORIG_CHUNK;
    test_data.m_fe_runner_ptr.get()->Run(&denoise_task);
    denoise_task.m_data.m_context->m_prev_features.push(denoise_task.m_data.m_input_spectrogram[0]);
  }
  REQUIRE(denoise_task.m_data.m_context->m_prev_features.size() == 3);
  REQUIRE(denoise_task.m_data.m_context->m_prev_spectrogram.real.size() == 0);
  REQUIRE(denoise_task.m_data.m_context->m_prev_spectrogram.imag.size() == 0);

  denoise_task.m_stage = spench::PipelineStage::PREDICTED_MASK;

  denoise_task.m_data.m_new_spectrogram.real = predicted_mask_real;
  denoise_task.m_data.m_new_spectrogram.imag = predicted_mask_imag;

  test_data.m_fe_runner_ptr.get()->Run(&denoise_task);
  CHECK(denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK);
  CHECK(denoise_task.m_data.m_context->m_prev_spectrogram.real.size() ==
                                                    test_data.m_fe_runner_ptr.get()->GetPowSpecFeatSizeSamples());
  CHECK(denoise_task.m_data.m_context->m_prev_spectrogram.imag.size() ==
                                                    test_data.m_fe_runner_ptr.get()->GetPowSpecFeatSizeSamples());
}

TEST_CASE("SUCCEED Launch runner with configuration read from .yaml config (audio 24kHz) [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("default_configuration_best_quality_onnx_24khz.yaml").
                                     SetWav("rec_noise_001_16k.wav"));
  constexpr size_t kIterations = 1 + 1;

  std::vector<std::vector<float>> reference_mag;
  std::vector<std::vector<float>> result_mag;
  reference_mag.emplace_back(std::vector<float>(test_data.m_fe_runner_ptr.get()->GetPowSpecFeatSizeSamples(), 0.0));
  REQUIRE(test_data.m_fe_runner_ptr.get()->GetSamplingFreq() == 24000);
  REQUIRE(test_data.m_fe_runner_ptr.get()->GetFrameSizeMs() == 32);

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::ORIG_CHUNK;
  std::vector<float> audio(test_data.m_fe_runner_ptr.get()->GetFrameHopSizeSamples() * kIterations, 0.0);
  denoise_task.m_data.m_waveform = audio;
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 0);

  test_data.m_fe_runner_ptr.get()->Run(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::FOURIER_TRANSFORMED_SPEC);
  REQUIRE(denoise_task.m_data.m_input_spectrogram.size() == 1);

  for (size_t i = 0; i < kIterations - 1 ; ++i) {
    result_mag.push_back(denoise_task.m_data.m_input_spectrogram[i].getMagnitude());
  }
  IsVectorsClose(reference_mag[0], result_mag[0], 0.00001);
}
