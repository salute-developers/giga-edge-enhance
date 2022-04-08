#include "tests/src/utils.h"
#include <catch.hpp>
#include <vector>
#include <string>

#include "spench/util/tflite/tflite_runner.h"

struct TestTfliteRunner {
  std::unique_ptr<util::tf_lite::BaseTfliteRunner> m_denoiser_runner;

  const std::string m_kBase = "data/";

 public:
  uint16_t m_frame_size_ms = 16;
  uint64_t m_sample_rate = 16000;
  TestTfliteRunner() = default;

  TestTfliteRunner& InitRunner(const std::string& yaml_config_name, uint16_t num_threads = 1) {
    std::string file_path = m_kBase + "config/" + yaml_config_name;
    YAML::Node node = YAML::LoadFile(file_path);
    // bool on_gpu = node["inference"]["gpu"].as<bool>(false);
    std::string model_type = node["inference"]["model_type"].as<std::string>("best_quality");

    m_denoiser_runner = std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
                                                new util::tf_lite::SpenchFTfliteRunner(node["model"],
                                                                                       num_threads));

    // m_denoiser_runner = (model_type == "best_quality") ?
    //                 std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
    //                                             new util::tf_lite::SpenchQTfliteRunner(node["model"],
    //                                                                               on_gpu)):
    //                 std::unique_ptr<util::tf_lite::BaseTfliteRunner>(
    //                                             new util::tf_lite::SpenchFTfliteRunner(node["model"],
    //                                                                               on_gpu));
    m_frame_size_ms = node["feature_params"]["frame_size_ms"].as<uint16_t>(16);
    m_sample_rate = node["feature_params"]["sampling_freq"].as<uint64_t>(16000);
    return *this;
  }

  TestTfliteRunner& LoadDataFromBin(const std::string& dump,
                        std::vector<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestTfliteRunner& LoadDataFromBin(const std::string& dump,
                        std::valarray<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
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

TEST_CASE("SUCCEED Run PrepareInputInPlace on one real frame with fast onnx model (audio 24 kHz) [tflite runner]") {
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
  spench::DenoiseContext<float>* denoise_contex = ctx.get();
  ctx.get()->m_noisy_mag = noisy_mag_5_old;
  IsVectorsClose(ctx.get()->m_noisy_mag, noisy_mag_5_old, 0.000001);
  data.m_denoiser_runner->PrepareInputInPlace(noisy_mag_1_new, inputs, denoise_contex);

  IsVectorsClose(ctx.get()->m_noisy_mag, noisy_mag_5_new, 0.000001);
}

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

  spench::DenoiseTask denoise_task;
  std::unique_ptr<spench::DenoiseContextF<float>> ctx(new spench::DenoiseContextF<float>);
  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->m_noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->m_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->m_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->m_prev_fb_out);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_data.m_input_spec.push_back(spench::ComplexNumber<float>(real, imag));

  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;
  data.m_denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTIONS);
  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                           denoise_task.m_data.m_enhanced_wav);

  data.LoadDataFromBin("1_noisy_mag_5_old.dump", ctx.get()->m_noisy_mag);
  data.LoadDataFromBin("1_fb_h_old_fast_24kHz.dump", ctx.get()->m_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_old_fast_24kHz.dump", ctx.get()->m_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_old_fast_24kHz.dump", ctx.get()->m_prev_fb_out);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;
  data.m_denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTIONS);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->m_fullband_hidden, 0.00001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->m_fullband_cell_state, 0.00001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->m_prev_fb_out, 0.00001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.m_network_output.real, 0.00001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.m_network_output.imag, 0.00001);
}

TEST_CASE("SUCCEED Run dummy multi inference for a fast model 24kHz and with different number of threads [tflite]") {
  const std::size_t nattempts = 50;

  for (uint16_t num_threads : {1, 2, 4, 8}) {
    auto data = std::move(TestTfliteRunner()
      .InitRunner("default_configuration_fast_tflite_24khz.yaml", num_threads));

    std::vector<size_t> timings(nattempts);
    std::vector<std::vector<float>*> inputs;
    for (auto s : data.m_denoiser_runner->m_model_input_size) {
      inputs.push_back(new std::vector<float>(s, static_cast<float>(0.)));
    }

    auto ts_1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < nattempts; ++i) {
      auto start = std::chrono::steady_clock::now();
      data.m_denoiser_runner->_TestRun<float>(inputs);
      auto stop = std::chrono::steady_clock::now();
      timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
    }
    auto ts_2 = std::chrono::steady_clock::now();

    auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
    std::cout << "24kHz fast model infer with " << num_threads << " threads completed: " <<
                                          tot <<
                                          " | " << static_cast<double>(tot) / nattempts <<
                                          " ms per infer" << std::endl;

    for (uint16_t i = 0; i < data.m_denoiser_runner->m_model_input_size.size(); i++) {
      delete inputs[i];
    }
  }
}
