#include <yaml-cpp/yaml.h>
#include "spench/src/spench_config_reader.h"
#include "spench/src/feature_extractor_runner.h"
#include <memory>
#include <catch.hpp>
#include <chrono> // NOLINT

#include "tests/src/utils.h"

struct TestFERunner {
  std::unique_ptr<spench::FeatureExtractorRunner> fe_runner_ptr;
  std::shared_ptr<spench::DenoiserConfig> cfg_ptr;
  const std::size_t num_attempts = 1000;
  std::vector<float> wav_data;
  const std::string data_root = "data/";

  TestFERunner() {}
  TestFERunner& operator=(const TestFERunner&) = delete;
  TestFERunner(const TestFERunner&) = delete;
  TestFERunner(TestFERunner&& orig) :
          fe_runner_ptr(std::move(orig.fe_runner_ptr)),
          wav_data(std::move(orig.wav_data)) {
    orig.wav_data.clear();
    orig.fe_runner_ptr = nullptr;
  }

  TestFERunner& SetWav(const std::string& wav_path) {
    std::string full_wav_path = data_root + "wavs/" + wav_path;
    LoadWavFx(full_wav_path.c_str(), wav_data);
    return *this;
  }

  TestFERunner& LoadDataFromBin(const std::string& dump, uint32_t step_size_samples,
                        std::vector<std::vector<float>>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFERunner& LoadSpecDataFromBin(const std::string& dump,
                        std::vector<std::vector<float>>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    uint32_t step_size_samples =
            fe_runner_ptr.get()->get_pow_spec_feat_size_samples();
    LoadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFERunner& LoadDataFromBin(const std::string& dump,
                        std::vector<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestFERunner& LoadDataFromBin(const std::string& dump,
                        std::valarray<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }

  YAML::Node GetYAML(const std::string& yaml_config_name) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    return YAML::LoadFile(file_path);
  }

  void InitYAMLReader(const std::string& yaml_config_name) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    cfg_ptr = std::make_shared<spench::DenoiserConfig>(spench::FileConfigReader(file_path));
    // return YAML::LoadFile(file_path);
  }

  TestFERunner& InitFERunner(const std::string& yaml_config_name) {
    InitYAMLReader(yaml_config_name);
    fe_runner_ptr =
              std::unique_ptr<spench::FeatureExtractorRunner>
              (new spench::FeatureExtractorRunner(cfg_ptr));
    return *this;
  }
};

// TEST_CASE("SUCCEED Launch runner for denoiser input generation with default parameters (audio 16kHz) [fe runner]") {
//   auto test_data = std::move(TestFERunner().
//                                      InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
//                                      SetWav("rec_noise_001_16k.wav"));
//   constexpr size_t iterations = 1 + 1;

//   std::vector<std::vector<float>> reference_mag;
//   std::vector<std::vector<float>> result_mag;
//   test_data.LoadSpecDataFromBin("10_mag_stft_tensors.dump", reference_mag);

//   REQUIRE(test_data.fe_runner_ptr.get()->get_sampling_freq() == 16000);
//   REQUIRE(test_data.fe_runner_ptr.get()->get_frame_size_ms() == 32);
//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
//   denoise_task.data.context = ctx.get();
//   denoise_task.stage = spench::PipelineStage::orig_chunk;
//   std::vector<float> audio(test_data.wav_data.begin(),
//                            test_data.wav_data.begin() +
//                            test_data.fe_runner_ptr.get()->get_frame_hop_size_samples() * iterations);
//   denoise_task.data.waveform = audio;
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 0);

//   test_data.fe_runner_ptr.get()->run(&denoise_task);

//   REQUIRE(denoise_task.stage == spench::PipelineStage::fourier_transformed_spec);
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 1);

//   for (size_t i = 0; i < iterations - 1 ; ++i) {
//     result_mag.push_back(denoise_task.data.input_spectrogram[i].get_magnitude());
//   }

//   IsVectorsClose(reference_mag[0], result_mag[0], 0.0001);
// }

// TEST_CASE("SUCCEED Launch runner for masking with default parameters (audio 16kHz) [fe runner]") {
//   auto test_data = std::move(TestFERunner().
//                                      InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
//                                      SetWav("rec_noise_001_16k.wav"));
//   std::vector<float> reference_signal;
//   std::valarray<float> predicted_mask_real;
//   std::valarray<float> predicted_mask_imag;
//   std::valarray<float> prev_predicted_mask_real;
//   std::valarray<float> prev_predicted_mask_imag;
//   std::vector<float> prev_features_real;
//   std::vector<float> prev_features_imag;

//   test_data.LoadDataFromBin("1_enhanced_audio_vector_16khz.dump", reference_signal);
//   test_data.LoadDataFromBin("1_predicted_mask_real_16khz.dump", predicted_mask_real);
//   test_data.LoadDataFromBin("1_predicted_mask_imag_16khz.dump", predicted_mask_imag);
//   test_data.LoadDataFromBin("1_previous_predicted_mask_real_16khz.dump", prev_predicted_mask_real);
//   test_data.LoadDataFromBin("1_previous_predicted_mask_imag_16khz.dump", prev_predicted_mask_imag);
//   test_data.LoadDataFromBin("1_previous_features_real_16khz.dump", prev_features_real);
//   test_data.LoadDataFromBin("1_previous_features_imag_16khz.dump", prev_features_imag);

//   spench::ComplexNumber<float> prev_features(prev_features_real, prev_features_imag);
//   spench::SpenchTask denoise_task;

//   std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
//   denoise_task.data.context = ctx.get();
//   // update previous features for test
//   denoise_task.data.context->prev_features.push(prev_features);
//   denoise_task.data.context->prev_features.push(prev_features);
//   denoise_task.stage = spench::PipelineStage::predicted_mask;

//   REQUIRE(test_data.fe_runner_ptr.get()->get_sampling_freq() == 16000);
//   REQUIRE(test_data.fe_runner_ptr.get()->get_frame_size_ms() == 32);

//   denoise_task.data.new_spectrogram.real = predicted_mask_real;
//   denoise_task.data.new_spectrogram.imag = predicted_mask_imag;
//   // add random value to avoid crashes because of empty contaner
//   denoise_task.data.input_spectrogram.push_back(prev_features);

//   denoise_task.data.context->prev_spectrogram.real = prev_predicted_mask_real;
//   denoise_task.data.context->prev_spectrogram.imag = prev_predicted_mask_imag;

//   REQUIRE(denoise_task.data.enhanced_wav.size() == 0);
//   test_data.fe_runner_ptr.get()->run(&denoise_task);

//   REQUIRE(denoise_task.data.enhanced_wav.size() ==
//                                   test_data.fe_runner_ptr.get()->get_frame_hop_size_samples());
//   REQUIRE(denoise_task.stage == spench::PipelineStage::enhanced_chunk);

//   IsVectorsClose(reference_signal, denoise_task.data.enhanced_wav, 0.05);
// }

TEST_CASE("SUCCEED Read config for runner with modified .yaml file and check values [fe runner]") {
  auto test_data = std::move(TestFERunner().
                                     InitFERunner("test_configuration_with_no_default_values.yaml").
                                     SetWav("rec_noise_001_16k.wav"));

  REQUIRE(test_data.fe_runner_ptr.get()->get_sampling_freq() == 8000);
  REQUIRE(test_data.fe_runner_ptr.get()->get_frame_size_ms() == 40);
}

TEST_CASE("FAIL Read config for runner with incorrect sampling frequency value [fe runner]") {
  CHECK_THROWS(TestFERunner().InitFERunner("test_configuration_with_incorrect_sampling_freq.yaml"));
}

TEST_CASE("FAIL Read config for runner with incorrect frame size value [fe runner]") {
  CHECK_THROWS_WITH(TestFERunner().InitFERunner("test_configuration_with_incorrect_frame_size.yaml"),
                                                "Config reading error. Frame size is not a scalar!");
}

TEST_CASE("FAIL Read config for runner with incorrect window hop value [fe runner]") {
  CHECK_THROWS_WITH(TestFERunner().InitFERunner("test_configuration_with_incorrect_window_hop.yaml"),
                                          "Config reading error. Frame size stide is not a scalar!");
}

TEST_CASE("FAIL Read config for runner with incorrect window type value [fe runner]") {
  CHECK_THROWS_WITH(TestFERunner().InitFERunner("test_configuration_with_incorrect_window_type.yaml"),
                                                  "Window type must be in [\"hanning\", \"hamming\"]");
}

// TEST_CASE("SUCCEED Launch runner with configuration read from .yaml config (audio 16kHz) [fe runner]") {
//   auto test_data = std::move(TestFERunner().
//                                      InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
//                                      SetWav("rec_noise_001_16k.wav"));
//   constexpr size_t iterations = 1 + 1;

//   std::vector<std::vector<float>> reference_mag;
//   std::vector<std::vector<float>> result_mag;
//   test_data.LoadSpecDataFromBin("10_mag_stft_tensors.dump", reference_mag);

//   REQUIRE(test_data.fe_runner_ptr.get()->get_sampling_freq() == 16000);
//   REQUIRE(test_data.fe_runner_ptr.get()->get_frame_size_ms() == 32);

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
//   denoise_task.data.context = ctx.get();
//   denoise_task.stage = spench::PipelineStage::orig_chunk;
//   std::vector<float> audio(test_data.wav_data.begin(),
//                            test_data.wav_data.begin() +
//                            test_data.fe_runner_ptr.get()->get_frame_hop_size_samples() * iterations);
//   denoise_task.data.waveform = audio;
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 0);

//   test_data.fe_runner_ptr.get()->run(&denoise_task);

//   REQUIRE(denoise_task.stage == spench::PipelineStage::fourier_transformed_spec);
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 1);

//   for (size_t i = 0; i < iterations - 1 ; ++i) {
//     result_mag.push_back(denoise_task.data.input_spectrogram[i].get_magnitude());
//   }

//   IsVectorsClose(reference_mag[0], result_mag[0], 0.0001);
// }

// TEST_CASE("SUCCEED Launch runner for iSTFT calc with skipped frame and check states (audio 16kHz) [fe runner]") {
//   auto test_data = std::move(TestFERunner().
//                                      InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
//                                      SetWav("rec_noise_001_16k.wav"));
//   constexpr size_t iterations = 1 + 1;

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
//   denoise_task.data.context = ctx.get();
//   denoise_task.stage = spench::PipelineStage::skipped_chunk;
//   std::vector<float> audio(test_data.wav_data.begin(),
//                            test_data.wav_data.begin() +
//                            test_data.fe_runner_ptr.get()->get_frame_hop_size_samples() * iterations);
//   denoise_task.data.waveform = audio;
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 0);

//   test_data.fe_runner_ptr.get()->run(&denoise_task);

//   CHECK(denoise_task.stage == spench::PipelineStage::skipped_chunk);
//   REQUIRE(std::vector<float>(denoise_task.data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.data.enhanced_wav);
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 0);
// }

// TEST_CASE("SUCCEED Launch runner for iSTFT calc with first valid chunk and check states (audio 16kHz) [fe runner]") {
//   auto test_data = std::move(TestFERunner().
//                                      InitFERunner("default_configuration_best_quality_onnx_16khz.yaml").
//                                      SetWav("rec_noise_001_16k.wav"));
//   constexpr size_t iterations = 1 + 1;

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
//   denoise_task.data.context = ctx.get();
//   std::valarray<float> predicted_mask_real;
//   std::valarray<float> predicted_mask_imag;
//   test_data.LoadDataFromBin("1_predicted_mask_real_16khz.dump", predicted_mask_real);
//   test_data.LoadDataFromBin("1_predicted_mask_imag_16khz.dump", predicted_mask_imag);

//   std::vector<float> audio(test_data.wav_data.begin(),
//                            test_data.wav_data.begin() +
//                            test_data.fe_runner_ptr.get()->get_frame_hop_size_samples() * iterations);
//   denoise_task.data.waveform = audio;
//   for (uint16_t i = 0; i < 3; ++i) {
//     denoise_task.stage = spench::PipelineStage::orig_chunk;
//     test_data.fe_runner_ptr.get()->run(&denoise_task);
//     denoise_task.data.context->prev_features.push(denoise_task.data.input_spectrogram[0]);
//   }
//   REQUIRE(denoise_task.data.context->prev_features.size() == 3);
//   REQUIRE(denoise_task.data.context->prev_spectrogram.real.size() == 0);
//   REQUIRE(denoise_task.data.context->prev_spectrogram.imag.size() == 0);

//   denoise_task.stage = spench::PipelineStage::predicted_mask;

//   denoise_task.data.new_spectrogram.real = predicted_mask_real;
//   denoise_task.data.new_spectrogram.imag = predicted_mask_imag;

//   test_data.fe_runner_ptr.get()->run(&denoise_task);
//   CHECK(denoise_task.stage == spench::PipelineStage::skipped_chunk);
//   CHECK(denoise_task.data.context->prev_spectrogram.real.size() ==
//                                                     test_data.fe_runner_ptr.get()->get_pow_spec_feat_size_samples());
//   CHECK(denoise_task.data.context->prev_spectrogram.imag.size() ==
//                                                     test_data.fe_runner_ptr.get()->get_pow_spec_feat_size_samples());
// }

// TEST_CASE("SUCCEED Launch runner with configuration read from .yaml config (audio 24kHz) [fe runner]") {
//   auto test_data = std::move(TestFERunner().
//                                      InitFERunner("default_configuration_best_quality_onnx_24khz.yaml").
//                                      SetWav("rec_noise_001_16k.wav"));
//   constexpr size_t iterations = 1 + 1;

//   std::vector<std::vector<float>> reference_mag;
//   std::vector<std::vector<float>> result_mag;
//   reference_mag.emplace_back(
//       std::vector<float>(test_data.fe_runner_ptr.get()->get_pow_spec_feat_size_samples(), 0.0));
//   REQUIRE(test_data.fe_runner_ptr.get()->get_sampling_freq() == 24000);
//   REQUIRE(test_data.fe_runner_ptr.get()->get_frame_size_ms() == 32);

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::SpenchContext<float>> ctx(new spench::SpenchContext<float>);
//   denoise_task.data.context = ctx.get();
//   denoise_task.stage = spench::PipelineStage::orig_chunk;
//   std::vector<float> audio(test_data.fe_runner_ptr.get()->get_frame_hop_size_samples() * iterations, 0.0);
//   denoise_task.data.waveform = audio;
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 0);

//   test_data.fe_runner_ptr.get()->run(&denoise_task);

//   REQUIRE(denoise_task.stage == spench::PipelineStage::fourier_transformed_spec);
//   REQUIRE(denoise_task.data.input_spectrogram.size() == 1);

//   for (size_t i = 0; i < iterations - 1 ; ++i) {
//     result_mag.push_back(denoise_task.data.input_spectrogram[i].get_magnitude());
//   }
//   IsVectorsClose(reference_mag[0], result_mag[0], 0.00001);
// }
