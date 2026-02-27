#include "tests/src/utils.h"
#include <catch.hpp>
#include <vector>
#include <string>
#include <spdlog/spdlog.h>
#include "spench/util/tflite/tflite_runner.h"

struct TestTfliteRunner {
  std::unique_ptr<util::tf_lite::BaseTfliteRunner> denoiser_runner;

  const std::string kBase = "data/";

 public:
  uint16_t frame_size_ms = 16;
  uint64_t sample_rate = 16000;
  TestTfliteRunner() = default;

  TestTfliteRunner& InitRunner(const std::string& yaml_config_name,
                               const std::string& password = "",
                               const std::string& checksum = "",
                               uint16_t num_threads = 1) {
    std::string file_path = kBase + "config/" + yaml_config_name;
    YAML::Node node = YAML::LoadFile(file_path);
    // bool on_gpu = node["inference"]["gpu"].as<bool>(false);
    std::string model_type = node["inference"]["model_type"].as<std::string>("best_quality");

    denoiser_runner = (model_type == "best_quality") ?
                    std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                                new util::tf_lite::SpenchQTfliteRunner(node["model"],
                                                                                       password,
                                                                                       checksum,
                                                                                       num_threads)):
                    std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                                new util::tf_lite::SpenchFTfliteRunner(node["model"],
                                                                                       password,
                                                                                       checksum,
                                                                                       num_threads));
    frame_size_ms = node["feature_params"]["frame_size_ms"].as<uint16_t>(16);
    sample_rate = node["feature_params"]["sampling_freq"].as<uint64_t>(16000);
    return *this;
  }

  TestTfliteRunner& LoadDataFromBin(const std::string& dump,
                        std::vector<float>& output) {
    std::string full_wav_path = kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestTfliteRunner& LoadDataFromBin(const std::string& dump,
                        std::valarray<float>& output) {
    std::string full_wav_path = kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
};

TEST_CASE("SUCCEED Run relu model dummy inference [tflite]") {
  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::BuildFromFile("data/models/dummy_relu_model.tflite");
  REQUIRE(model != nullptr);

  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*model, resolver);
  std::unique_ptr<tflite::Interpreter> interpreter;
  builder(&interpreter);
  REQUIRE(interpreter != nullptr);

  REQUIRE(interpreter->AllocateTensors() == kTfLiteOk);
  std::vector<float> input_vector {-1.3, 0.3, 1.0};
  std::vector<float> gt_vector {0, 0.3, 1.0};

  float* input_data_ptr = interpreter->typed_input_tensor<float>(0);
  for (auto const& value : input_vector) {
    *(input_data_ptr) = value;
    input_data_ptr++;
  }

  interpreter->Invoke();

  float* output_data_ptr = interpreter->typed_output_tensor<float>(0);
  std::vector<float> output;
  output.assign(output_data_ptr, output_data_ptr + input_vector.size());

  IsVectorsClose(output, gt_vector, 0.0000001);
}

TEST_CASE("SUCCEED Run PrepareInputInPlace on one real frame with fast tflite model (audio 24 kHz) [tflite runner]") {
  auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_24khz.yaml"));
  std::vector<float> noisy_mag_5_old;
  std::vector<float> noisy_mag_1_new;
  std::vector<float> noisy_mag_5_new;
  std::vector<std::vector<float>*> inputs;

  data.LoadDataFromBin("1_noisy_mag_5_old.dump", noisy_mag_5_old);
  data.LoadDataFromBin("1_noisy_mag_1_new.dump", noisy_mag_1_new);
  data.LoadDataFromBin("1_noisy_mag_5_new.dump", noisy_mag_5_new);

  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>);
  spench::SpenchContext<float>* denoise_contex = ctx.get();
  ctx.get()->noisy_mag = noisy_mag_5_old;
  IsVectorsClose(ctx.get()->noisy_mag, noisy_mag_5_old, 0.000001);
  data.denoiser_runner->PrepareInputInPlace(noisy_mag_1_new, inputs, denoise_contex);

  IsVectorsClose(ctx.get()->noisy_mag, noisy_mag_5_new, 0.000001);
}

TEST_CASE("FAIL Read config with corrupted model [tflite runner]") {
  CHECK_THROWS_WITH(TestTfliteRunner()
      .InitRunner("test_configuration_with_corrupted_fast_tflite_24khz.yaml"),
                                            "Model reading error. There is problem with checkpoint file.");
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("FAIL Read config with incorrect encrypted model path or name [tflite runner]") {
  CHECK_THROWS_WITH(TestTfliteRunner()
      .InitRunner("incorrect_configuration_fast_tflite_encrypted_24khz.yaml", "test"),
                                            "Model reading error. Detailes: file doesn't contain header");
}

TEST_CASE("FAIL Pass incorrect password for encrypted model [tflite runner]") {
  CHECK_THROWS_WITH(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_encrypted_24khz.yaml", "test1"),
                                            "Model reading error. Detailes: chunk is corrupted");
}

TEST_CASE("FAIL Pass no password for encrypted model [tflite runner]") {
  std::string error_message = std::string("Model reading error. There is problem with checkpoint file.");
  CHECK_THROWS_WITH(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_encrypted_24khz.yaml", ""), error_message.c_str());
}

TEST_CASE("FAIL Pass incorrect password for licensed encrypted model[tflite runner]") {
  CHECK_THROWS_WITH(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_lic_encrypted_24khz.yaml", "qwerty1"),
                                            "Model reading error. Detailes: chunk is corrupted");
}


TEST_CASE("FAIL Pass incorrect checksum for licensed encrypted model [tflite runner]") {
  CHECK_THROWS_WITH(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_lic_encrypted_24khz.yaml",
                  "qwerty",
                  "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+i="),
                                            "Model reading error. Detailes: error deserializing base64 checksum");
}
#endif

TEST_CASE("SUCCEED Run fast tflite model with real data and check first two iterations (24 kHz) [tflite runner]") {
  auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_24khz.yaml"));

  std::valarray<float> real, imag;
  std::vector<float> gt_fullband_hidden, gt_fullband_cell_state;
  std::vector<float> gt_real_output, gt_imag_output;
  std::vector<float> gt_prev_fb_out_5, gt_noisy_mag_5_new;
  data.LoadDataFromBin("1_noisy_real_in.dump", real);
  data.LoadDataFromBin("1_noisy_imag_in.dump", imag);
  data.LoadDataFromBin("1_fb_h_new_fast_24kHz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_new_fast_24kHz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_new_fast_24kHz.dump", gt_prev_fb_out_5);
  data.LoadDataFromBin("1_noisy_real_out.dump", gt_real_output);
  data.LoadDataFromBin("1_noisy_imag_out.dump", gt_imag_output);
  data.LoadDataFromBin("1_noisy_mag_5_new.dump", gt_noisy_mag_5_new);

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>);
  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));

  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
                           denoise_task.m_data.enhanced_wav);

  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->fullband_hidden, 0.00001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->fullband_cell_state, 0.00001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->prev_fb_out, 0.00001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.new_spectrogram.real, 0.00001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.new_spectrogram.imag, 0.00001);
}

TEST_CASE("SUCCEED Run quality tflite model with real data and check first two iterations (48 kHz) [tflite runner]") {
  auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_best_quality_tflite_48khz.yaml"));

  std::vector<float> gt_fb_hidden_step1;
  std::vector<float> gt_fb_hidden_step2;
  std::vector<float> gt_fb_snr_noise_psd_step1;
  std::vector<float> gt_sb_snr_noise_psd_step1;
  std::vector<float> gt_fb_snr_noise_psd_step2;
  std::vector<float> gt_sb_snr_noise_psd_step2;
  std::vector<float> gt_b1_in_step1, gt_b3_in_step1;
  std::vector<float> gt_b1_out_step1, gt_b3_out_step1;
  std::vector<float> gt_b1_out_step2, gt_b3_out_step2;
  std::valarray<float> noisy_real_0, noisy_imag_0;
  std::valarray<float> noisy_real_1, noisy_imag_1;
  std::vector<float> gt_fb_snr_noise_pg_step1;
  std::vector<float> gt_sb_snr_noise_pg_step1;
  std::vector<float> gt_fb_snr_noise_pg_step2;
  std::vector<float> gt_sb_snr_noise_pg_step2;

  data.LoadDataFromBin("1_cur_fb_hidden_out_48khz.dump", gt_fb_hidden_step1);
  data.LoadDataFromBin("2_cur_fb_hidden_out_48khz.dump", gt_fb_hidden_step2);
  data.LoadDataFromBin("1_cur_fb_snr_noise_psd_out_48khz.dump", gt_fb_snr_noise_psd_step1);
  data.LoadDataFromBin("1_cur_sb_snr_noise_psd_out_48khz.dump", gt_sb_snr_noise_psd_step1);
  data.LoadDataFromBin("2_cur_fb_snr_noise_psd_out_48khz.dump", gt_fb_snr_noise_psd_step2);
  data.LoadDataFromBin("2_cur_sb_snr_noise_psd_out_48khz.dump", gt_sb_snr_noise_psd_step2);
  data.LoadDataFromBin("1_noisy_real_frame0_48khz.dump", noisy_real_0);
  data.LoadDataFromBin("1_noisy_imag_frame0_48khz.dump", noisy_imag_0);
  data.LoadDataFromBin("1_noisy_real_frame1_48khz.dump", noisy_real_1);
  data.LoadDataFromBin("1_noisy_imag_frame1_48khz.dump", noisy_imag_1);
  data.LoadDataFromBin("step1_prev_fb_conv1_buf_in_48khz.dump", gt_b1_in_step1);
  data.LoadDataFromBin("step1_prev_fb_conv3_buf_in_48khz.dump", gt_b3_in_step1);
  data.LoadDataFromBin("1_cur_fb_conv1_buf_out_48khz.dump", gt_b1_out_step1);
  data.LoadDataFromBin("1_cur_fb_conv3_buf_out_48khz.dump", gt_b3_out_step1);
  data.LoadDataFromBin("2_cur_fb_conv1_buf_out_48khz.dump", gt_b1_out_step2);
  data.LoadDataFromBin("2_cur_fb_conv3_buf_out_48khz.dump", gt_b3_out_step2);
  data.LoadDataFromBin("1_cur_fb_snr_noise_pg_out_48khz.dump", gt_fb_snr_noise_pg_step1);
  data.LoadDataFromBin("1_cur_sb_snr_noise_pg_out_48khz.dump", gt_sb_snr_noise_pg_step1);
  data.LoadDataFromBin("2_cur_fb_snr_noise_pg_out_48khz.dump", gt_fb_snr_noise_pg_step2);
  data.LoadDataFromBin("2_cur_sb_snr_noise_pg_out_48khz.dump", gt_sb_snr_noise_pg_step2);

  spench::SpenchTask denoise_task;
  auto ctx = std::make_unique<spench::DenoiseContextQ<float>>();

  data.LoadDataFromBin("step1_prev_fb_hidden_in_48khz.dump", ctx->fullband_hidden);
  data.LoadDataFromBin("step1_prev_fb_snr_noise_psd_in_48khz.dump", ctx->fb_snr_noise_psd);
  data.LoadDataFromBin("step1_prev_sb_snr_noise_psd_in_48khz.dump", ctx->sb_snr_noise_psd);
  data.LoadDataFromBin("step1_prev_fb_snr_noise_pg_in_48khz.dump", ctx->fb_snr_noise_pg);
  data.LoadDataFromBin("step1_prev_sb_snr_noise_pg_in_48khz.dump", ctx->sb_snr_noise_pg);

  REQUIRE(ctx->fullband_hidden.size() == 2 * 256);

  denoise_task.m_data.context = ctx.get();
  denoise_task.m_data.input_spectrogram.clear();
  denoise_task.m_data.input_spectrogram.emplace_back(noisy_real_0, noisy_imag_0);
  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;

  data.LoadDataFromBin("step1_prev_fb_conv1_buf_in_48khz.dump", ctx->fb_conv1_buf);
  data.LoadDataFromBin("step1_prev_fb_conv3_buf_in_48khz.dump", ctx->fb_conv3_buf);
  REQUIRE(ctx->fb_conv1_buf.size() == gt_b1_in_step1.size());
  REQUIRE(ctx->fb_conv3_buf.size() == gt_b3_in_step1.size());
  IsVectorsClose(gt_b1_in_step1, ctx->fb_conv1_buf, 1e-5f);
  IsVectorsClose(gt_b3_in_step1, ctx->fb_conv3_buf, 1e-5f);

  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE((denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK ||
           denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK));

  IsVectorsClose(gt_fb_hidden_step1, ctx->fullband_hidden, 1e-5f);
  IsVectorsClose(gt_fb_snr_noise_psd_step1, ctx->fb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_sb_snr_noise_psd_step1, ctx->sb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_b1_out_step1, ctx->fb_conv1_buf, 1e-5f);
  IsVectorsClose(gt_b3_out_step1, ctx->fb_conv3_buf, 1e-5f);
  IsVectorsClose(gt_fb_snr_noise_pg_step1, ctx->fb_snr_noise_pg, 1e-5f);
  IsVectorsClose(gt_sb_snr_noise_pg_step1, ctx->sb_snr_noise_pg, 1e-5f);

  denoise_task.m_data.input_spectrogram.clear();
  denoise_task.m_data.input_spectrogram.emplace_back(noisy_real_1, noisy_imag_1);
  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;

  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE((denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK ||
           denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK));

  IsVectorsClose(gt_fb_hidden_step2, ctx->fullband_hidden, 1e-5f);
  IsVectorsClose(gt_fb_snr_noise_psd_step2, ctx->fb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_sb_snr_noise_psd_step2, ctx->sb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_b1_out_step2, ctx->fb_conv1_buf, 1e-5f);
  IsVectorsClose(gt_b3_out_step2, ctx->fb_conv3_buf, 1e-5f);
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("SUCCEED Run encrypted model with real data and check first two iterations (24 kHz) [tflite runner]") {
  auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_encrypted_24khz.yaml", "test"));
  std::valarray<float> real, imag;
  std::vector<float> gt_fullband_hidden, gt_fullband_cell_state;
  std::vector<float> gt_real_output, gt_imag_output;
  std::vector<float> gt_prev_fb_out_5, gt_noisy_mag_5_new;
  data.LoadDataFromBin("1_noisy_real_in.dump", real);
  data.LoadDataFromBin("1_noisy_imag_in.dump", imag);
  data.LoadDataFromBin("1_fb_h_new_fast_24kHz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_new_fast_24kHz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_new_fast_24kHz.dump", gt_prev_fb_out_5);
  data.LoadDataFromBin("1_noisy_real_out.dump", gt_real_output);
  data.LoadDataFromBin("1_noisy_imag_out.dump", gt_imag_output);
  data.LoadDataFromBin("1_noisy_mag_5_new.dump", gt_noisy_mag_5_new);

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>);
  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));

  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
                           denoise_task.m_data.enhanced_wav);

  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->fullband_hidden, 0.00001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->fullband_cell_state, 0.00001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->prev_fb_out, 0.00001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.new_spectrogram.real, 0.00001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.new_spectrogram.imag, 0.00001);
}

TEST_CASE("SUCCEED Run licensed encrypted model and check first two iterations (24 kHz) [tflite runner]") {
  auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_lic_encrypted_24khz.yaml",
                  "qwerty", "Hhlr0ZEoL7KcTNoM9rbFVbpfN+k6yuamg3K2THn4K+I="));
  std::valarray<float> real, imag;
  std::vector<float> gt_fullband_hidden, gt_fullband_cell_state;
  std::vector<float> gt_real_output, gt_imag_output;
  std::vector<float> gt_prev_fb_out_5, gt_noisy_mag_5_new;
  data.LoadDataFromBin("1_noisy_real_in.dump", real);
  data.LoadDataFromBin("1_noisy_imag_in.dump", imag);
  data.LoadDataFromBin("1_fb_h_new_fast_24kHz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_new_fast_24kHz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_new_fast_24kHz.dump", gt_prev_fb_out_5);
  data.LoadDataFromBin("1_noisy_real_out.dump", gt_real_output);
  data.LoadDataFromBin("1_noisy_imag_out.dump", gt_imag_output);
  data.LoadDataFromBin("1_noisy_mag_5_new.dump", gt_noisy_mag_5_new);

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>);
  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));

  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
                           denoise_task.m_data.enhanced_wav);

  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->fullband_hidden, 0.00001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->fullband_cell_state, 0.00001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->prev_fb_out, 0.00001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.new_spectrogram.real, 0.00001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.new_spectrogram.imag, 0.00001);
}

TEST_CASE("SUCCEED Run licensed encrypted model w/o lic check (24 kHz) [tflite runner]") {
  auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_lic_encrypted_24khz.yaml",
                  "qwerty", ""));
  std::valarray<float> real, imag;
  std::vector<float> gt_fullband_hidden, gt_fullband_cell_state;
  std::vector<float> gt_real_output, gt_imag_output;
  std::vector<float> gt_prev_fb_out_5, gt_noisy_mag_5_new;
  data.LoadDataFromBin("1_noisy_real_in.dump", real);
  data.LoadDataFromBin("1_noisy_imag_in.dump", imag);
  data.LoadDataFromBin("1_fb_h_new_fast_24kHz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_new_fast_24kHz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_new_fast_24kHz.dump", gt_prev_fb_out_5);
  data.LoadDataFromBin("1_noisy_real_out.dump", gt_real_output);
  data.LoadDataFromBin("1_noisy_imag_out.dump", gt_imag_output);
  data.LoadDataFromBin("1_noisy_mag_5_new.dump", gt_noisy_mag_5_new);

  spench::SpenchTask denoise_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>);
  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));

  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
                           denoise_task.m_data.enhanced_wav);

  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->prev_fb_out);
  denoise_task.m_data.context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
  data.denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->fullband_hidden, 0.00001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->fullband_cell_state, 0.00001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->prev_fb_out, 0.00001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.new_spectrogram.real, 0.00001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.new_spectrogram.imag, 0.00001);
}
#endif

TEST_CASE("SUCCEED Run dummy multi inference for a model 48kHz and with different number of threads [tflite]") {
  const std::size_t nattempts = 100;

  for (uint16_t num_threads : {1, 2, 4, 8}) {
    auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_best_quality_tflite_48khz.yaml", "", "", num_threads));

    std::vector<size_t> timings(nattempts);
    std::vector<std::vector<float>*> inputs;
    for (auto s : data.denoiser_runner->m_model_input_size) {
      inputs.push_back(new std::vector<float>(s, static_cast<float>(0.)));
    }

    auto ts_1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < nattempts; ++i) {
      auto start = std::chrono::steady_clock::now();
      data.denoiser_runner->_TestRun<float>(inputs);
      auto stop = std::chrono::steady_clock::now();
      timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
    }
    auto ts_2 = std::chrono::steady_clock::now();

    auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();

    spdlog::info("48kHz quality model infer with {} threads completed: {} | {} ms per infer",
               num_threads,
               tot,
               static_cast<double>(tot) / nattempts);

    for (uint16_t i = 0; i < data.denoiser_runner->m_model_input_size.size(); i++) {
      delete inputs[i];
    }
  }
}

TEST_CASE("SUCCEED Run dummy multi inference for a fast model 24kHz and with different number of threads [tflite]") {
  const std::size_t nattempts = 100;

  for (uint16_t num_threads : {1, 2, 4, 8}) {
    auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_24khz.yaml", "", "", num_threads));

    std::vector<size_t> timings(nattempts);
    std::vector<std::vector<float>*> inputs;
    for (auto s : data.denoiser_runner->m_model_input_size) {
      inputs.push_back(new std::vector<float>(s, static_cast<float>(0.)));
    }

    auto ts_1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < nattempts; ++i) {
      auto start = std::chrono::steady_clock::now();
      data.denoiser_runner->_TestRun<float>(inputs);
      auto stop = std::chrono::steady_clock::now();
      timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
    }
    auto ts_2 = std::chrono::steady_clock::now();

    auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();

    spdlog::info("24kHz fast model infer with {} threads completed: {} | {} ms per infer",
               num_threads,
               tot,
               static_cast<double>(tot) / nattempts);

    for (uint16_t i = 0; i < data.denoiser_runner->m_model_input_size.size(); i++) {
      delete inputs[i];
    }
  }
}

// TEST_CASE(
//  "SUCCEED Run inference via the best quality model and check network outputs "
//  "(audio 16kHz) [tflite runner]")
// {
//   auto data = std::move(TestTfliteRunner()
//       .InitRunner("default_configuration_best_quality_tflite_16khz.yaml"));

//   std::valarray<float> real;
//   std::valarray<float> imag;
//   std::vector<float> gt_real;
//   std::vector<float> gt_imag;
//   std::vector<float> gt_fullband_hidden;
//   std::vector<float> gt_fullband_cell_state;
//   std::vector<float> gt_subband_hidden;
//   std::vector<float> gt_subband_cell_state;
//   data.LoadDataFromBin("1_real_wav_stft.dump", real);
//   data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
//   data.LoadDataFromBin("1_real_network_output_for_zeroed_hiddens_16khz.dump", gt_real);
//   data.LoadDataFromBin("1_imag_network_output_for_zeroed_hiddens_16khz.dump", gt_imag);
//   data.LoadDataFromBin("1_predicted_full_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_fullband_hidden);
//   data.LoadDataFromBin("1_predicted_full_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_fullband_cell_state);
//   data.LoadDataFromBin("1_predicted_sub_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_subband_hidden);
//   data.LoadDataFromBin("1_predicted_sub_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_subband_cell_state);

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
//   ctx.get()->fullband_hidden.resize(data.denoiser_runner->m_model_input_size[1], 0);
//   ctx.get()->fullband_cell_state.resize(data.denoiser_runner->m_model_input_size[2], 0);
//   ctx.get()->subband_hidden.resize(data.denoiser_runner->m_model_input_size[3], 0);
//   ctx.get()->subband_cell_state.resize(data.denoiser_runner->m_model_input_size[4], 0);

//   denoise_task.m_data.context = ctx.get();
//   denoise_task.m_data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));
//   denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;

//   data.denoiser_runner->Infer(&denoise_task);

//   REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK);
//   IsVectorsClose(gt_real, denoise_task.m_data.new_spectrogram.real, 0.000001);

//   IsVectorsClose(gt_imag, denoise_task.m_data.new_spectrogram.imag, 0.000001);
//   CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.m_data.enhanced_wav);
//   IsVectorsClose(gt_fullband_hidden, ctx.get()->fullband_hidden, 0.000001);
//   IsVectorsClose(gt_fullband_cell_state, ctx.get()->fullband_cell_state, 0.000001);
//   IsVectorsClose(gt_subband_hidden, ctx.get()->subband_hidden, 0.00001);
//   IsVectorsClose(gt_subband_cell_state, ctx.get()->subband_cell_state, 0.00001);
// }

// TEST_CASE("SUCCEED Run four frames with the best quality model and check states (audio 16kHz) [tflite runner]") {
//   auto data = std::move(TestTfliteRunner()
//       .InitRunner("default_configuration_best_quality_tflite_16khz.yaml"));

//   spench::SpenchTask denoise_task;
//   std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
//   ctx.get()->fullband_hidden.resize(data.denoiser_runner->m_model_input_size[1], 0);
//   ctx.get()->fullband_cell_state.resize(data.denoiser_runner->m_model_input_size[2], 0);
//   ctx.get()->subband_hidden.resize(data.denoiser_runner->m_model_input_size[3], 0);
//   ctx.get()->subband_cell_state.resize(data.denoiser_runner->m_model_input_size[4], 0);
//   denoise_task.m_data.context = ctx.get();

//   std::valarray<float> real;
//   std::valarray<float> imag;
//   data.LoadDataFromBin("1_real_wav_stft.dump", real);
//   data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
//   denoise_task.m_data.input_spectrogram.push_back(spench::ComplexNumber<float>(real, imag));
//   denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
//   uint64_t hop_size = static_cast<uint64_t>(data.sample_rate * data.frame_size_ms / 1000);
//   denoise_task.m_data.enhanced_wav.assign(hop_size, 1.0);

//   data.denoiser_runner->Infer(&denoise_task);
//   REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK);

//   CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.m_data.enhanced_wav);
//   CHECK(std::vector<float>(ctx.get()->fullband_hidden.size(), 0.0) !=
//                                                  ctx.get()->fullband_hidden);
//   CHECK(std::vector<float>(ctx.get()->fullband_cell_state.size(), 0.0) !=
//                                                  ctx.get()->fullband_cell_state);
//   CHECK(std::vector<float>(ctx.get()->subband_hidden.size(), 0.0) !=
//                                                  ctx.get()->subband_hidden);
//   CHECK(std::vector<float>(ctx.get()->subband_cell_state.size(), 0.0) !=
//                                                  ctx.get()->subband_cell_state);

//   denoise_task.m_data.enhanced_wav.assign(hop_size, 1.0);
//   denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;

//   data.denoiser_runner->Infer(&denoise_task);
//   REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_CHUNK);
//   CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.m_data.enhanced_wav);
//   denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
//   denoise_task.m_data.enhanced_wav.assign(hop_size, 1.0);

//   data.denoiser_runner->Infer(&denoise_task);
//   REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
//   CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 0.0) ==
//                                                  denoise_task.m_data.enhanced_wav);

//   denoise_task.m_stage = spench::PipelineStage::FOURIER_TRANSFORMED_SPEC;
//   denoise_task.m_data.enhanced_wav.assign(hop_size, 1.0);

//   data.denoiser_runner->Infer(&denoise_task);
//   REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTED_MASK);
//   CHECK(std::vector<float>(denoise_task.m_data.enhanced_wav.size(), 1.0) ==
//                                                  denoise_task.m_data.enhanced_wav);
// }

// TEST_CASE(
//  "SUCCEED Run dummy multi inference for a quality model 16kHz and with "
//  "different number of threads [tflite]")
// {
//   const std::size_t nattempts = 50;

//   for (uint16_t num_threads : {1, 2, 4, 8}) {
//     auto data = std::move(TestTfliteRunner()
//       .InitRunner("default_configuration_best_quality_tflite_16khz.yaml", "", "", num_threads));

//     std::vector<size_t> timings(nattempts);
//     std::vector<std::vector<float>*> inputs;
//     for (auto s : data.denoiser_runner->m_model_input_size) {
//       inputs.push_back(new std::vector<float>(s, static_cast<float>(0.)));
//     }

//     auto ts_1 = std::chrono::steady_clock::now();
//     for (size_t i = 0; i < nattempts; ++i) {
//       auto start = std::chrono::steady_clock::now();
//       data.denoiser_runner->_TestRun<float>(inputs);
//       auto stop = std::chrono::steady_clock::now();
//       timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
//     }
//     auto ts_2 = std::chrono::steady_clock::now();

//     auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
//     spdlog::info("24kHz quality model infer with  {} threads completed: {} | {} ms per infer",
//                num_threads,
//                tot,
//                static_cast<double>(tot) / nattempts);

//     for (uint16_t i = 0; i < data.denoiser_runner->m_model_input_size.size(); i++) {
//       delete inputs[i];
//     }
//   }
// }
