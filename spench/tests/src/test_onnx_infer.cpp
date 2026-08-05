#include <contrib/onnxruntime/include/onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <catch.hpp>
#include <iostream>
#include <map>
#include <memory>

#include "spench/src/spench_context.h"
#include "spench/util/onnx/denoise_onnx_runner.h"
#include "tests/src/utils.h"

void CreateConstInputsWithGivenShapes(const std::vector<std::vector<int64_t>>& input_shapes,
                                      std::vector<std::vector<float>>& const_allocated_data,
                                      std::vector<Ort::Value>& tensors) {
  uint32_t i = 0;
  for (auto& shape : input_shapes) {
    auto length = std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<int64_t>());
    const auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    // protection against dynamic axes
    std::vector<int64_t> shape_copy = shape;
    if (length < 0) {
      length = -length;
      std::transform(shape_copy.begin(), shape_copy.end(), shape_copy.begin(),
                     [](int64_t x) { return x < 0 ? -x : x; });
    }
    const_allocated_data.emplace_back(std::vector<float>(length, 1.0));
    tensors.emplace_back(Ort::Value::CreateTensor(
        memory_info, const_allocated_data.back().data(), const_allocated_data.back().size() * sizeof(float),
        shape_copy.data(), shape_copy.size(), ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT));
    ++i;
  }
}

void FreeNames(Ort::AllocatorWithDefaultOptions& allocator, std::vector<char*>& tensors) {
  for (uint16_t i = 0; i < tensors.size(); ++i) {
    allocator.Free(tensors[i]);
  }
}

struct TestOnnx {
  std::unique_ptr<Ort::Session> denoiser;

  const std::string data_root = "data/";

  const std::size_t nattempts = 100;

 private:
  Ort::SessionOptions session_options;

  Ort::Env session_env{ORT_LOGGING_LEVEL_WARNING, "Default"};

 public:
  TestOnnx() {
    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);
    session_options.SetExecutionMode(ExecutionMode::ORT_PARALLEL);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  }

  TestOnnx& init_model(const std::string& model_name) {
    std::string full_model_path = data_root + "models/" + model_name;

    denoiser = std::unique_ptr<Ort::Session>(new Ort::Session(session_env, full_model_path.c_str(), session_options));
    return *this;
  }

  TestOnnx& SetThreadNum(int16_t th) {
    session_options.SetIntraOpNumThreads(th);
    session_options.SetInterOpNumThreads(th);
    return *this;
  }
};

struct TestOnnxRunner {
  std::unique_ptr<util::onnx::BaseOnnxRunner> spench_model_runner;

  const std::string data_root = "data/";

 public:
  uint16_t frame_size_ms = 16;
  uint64_t sample_rate = 16000;
  TestOnnxRunner() = default;

  TestOnnxRunner& InitRunner(const std::string& yaml_config_name, const std::string& password = "",
                             const std::string& checksum = "") {
    std::string file_path = data_root + "config/" + yaml_config_name;
    auto reader = spench::FileConfigReader(file_path);
    bool on_gpu = reader.params()["inference"]["gpu"].as<bool>(false);
    std::string model_type = reader.params()["inference"]["model_type"].as<std::string>("best_speed");

    if (model_type == "best_quality") {
      auto config = std::make_shared<spench::DenoiserConfig>(reader);
      spench_model_runner = std::unique_ptr<util::onnx::BaseOnnxRunner>(
          new util::onnx::SpenchQOnnxRunner(config, password, checksum, on_gpu));
    } else if (model_type == "best_speed") {
      auto config = std::make_shared<spench::DenoiserConfig>(reader);
      spench_model_runner = std::unique_ptr<util::onnx::BaseOnnxRunner>(
          new util::onnx::SpenchFOnnxRunner(config, password, checksum, on_gpu));
    }
    frame_size_ms = reader.params()["feature_params"]["frame_size_ms"].as<uint16_t>(16);
    sample_rate = reader.params()["feature_params"]["sampling_freq"].as<uint64_t>(16000);
    return *this;
  }

  TestOnnxRunner& LoadDataFromBin(const std::string& dump, std::vector<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestOnnxRunner& LoadDataFromBin(const std::string& dump, std::valarray<float>& output) {
    std::string full_wav_path = data_root + "binary/" + dump;
    LoadBin(full_wav_path.c_str(), output);
    return *this;
  }
};

TEST_CASE("SUCCEED Run single dummy inference [onnx]") {
  auto data = std::move(TestOnnx().init_model("quality_model_48khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<int64_t>> output_shapes;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.denoiser->GetOutputNameAllocated(i, allocator).release());
    output_shapes.push_back(data.denoiser->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  Ort::RunOptions opt;
  auto outputs = data.denoiser->Run(opt, input_names.data(), inputs.data(), inputs.size(), output_names.data(),
                                    output_names.size());

  REQUIRE(output_shapes.size() == outputs.size());
  for (size_t i = 0; i < output_shapes.size(); ++i) {
    auto type_info = outputs[i].GetTensorTypeAndShapeInfo();
    auto actual_shape = type_info.GetShape();
    auto& expected_shape = output_shapes[i];
    REQUIRE(actual_shape.size() == expected_shape.size());
    for (size_t d = 0; d < actual_shape.size(); ++d) {
      if (expected_shape[d] <= 0) {
        REQUIRE(actual_shape[d] > 0);
      } else {
        REQUIRE(actual_shape[d] == expected_shape[d]);
      }
    }
  }

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a quality model 48kHz and with one thread [onnx]") {
  auto data = std::move(TestOnnx().init_model("quality_model_48khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();

    Ort::RunOptions opt;
    auto outputs = data.denoiser->Run(opt, input_names.data(), inputs.data(), inputs.size(), output_names.data(),
                                      output_names.size());
    auto& out = outputs[0];
    auto out_shape = out.GetTensorTypeAndShapeInfo().GetShape();

    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();

  auto tot = std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "48kHz quality model infer with 1 thread completed: " << tot << " | "
            << static_cast<double>(tot) / data.nattempts << " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a quality model 48kHz and with two threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(2).init_model("quality_model_48khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.denoiser->Run(opt, input_names.data(), inputs.data(), inputs.size(), output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot = std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "48kHz quality model infer with 2 threads completed: " << tot << " | "
            << static_cast<double>(tot) / data.nattempts << " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a quality model 48kHz and with four threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(4).init_model("quality_model_48khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.denoiser->Run(opt, input_names.data(), inputs.data(), inputs.size(), output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot = std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "48kHz quality model infer with 4 threads completed: " << tot << " | "
            << static_cast<double>(tot) / data.nattempts << " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a quality model 48kHz and with eight threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(8).init_model("quality_model_48khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.denoiser->Run(opt, input_names.data(), inputs.data(), inputs.size(), output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot = std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "48kHz quality model infer with 8 threads completed: " << tot << " | "
            << static_cast<double>(tot) / data.nattempts << " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("FAIL Read config for Inference Runner with incorrect model path [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner().InitRunner("test_configuration_with_incorrect_model_path.yaml"),
                    "Config reading error. Model path parameter is not a single string!");
}

TEST_CASE("FAIL Read config for Inference Runner with not existed model path [onnx runner]") {
  std::string error_message = std::string("Model reading error. Detailes: Load model from ") +
                              std::string("data/models/not_existed_model.onnx failed:Load model ") +
                              std::string("data/models/not_existed_model.onnx failed. ") +
                              std::string("File doesn't exist");

  CHECK_THROWS_WITH(TestOnnxRunner().InitRunner("test_configuration_with_not_existed_model_path.yaml"),
                    error_message.c_str());
}

TEST_CASE("FAIL Run cpu onnx runner with null pointer task and zeroed hidden layer [onnx runner]") {
  auto data = std::move(TestOnnxRunner().InitRunner("default_configuration_best_quality_onnx_48khz.yaml"));
  CHECK_THROWS_WITH(data.spench_model_runner->infer(nullptr), "Task data or context are not initialized!");
}

TEST_CASE("FAIL Run cpu onnx runner with null pointer context and zeroed hidden layer [onnx runner]") {
  auto data = std::move(TestOnnxRunner().InitRunner("default_configuration_best_quality_onnx_48khz.yaml"));
  spench::SpenchTask denoise_task;
  CHECK_THROWS_WITH(data.spench_model_runner->infer(&denoise_task), "Task data or context are not initialized!");
}

TEST_CASE("SUCCEED Run quality onnx model with real data and check first two iterations (audio 48 kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner().InitRunner("default_configuration_best_quality_onnx_48khz.yaml"));

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
  std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>());

  data.LoadDataFromBin("step1_prev_fb_hidden_in_48khz.dump", ctx->fullband_hidden);
  data.LoadDataFromBin("step1_prev_fb_snr_noise_psd_in_48khz.dump", ctx->fb_snr_noise_psd);
  data.LoadDataFromBin("step1_prev_sb_snr_noise_psd_in_48khz.dump", ctx->sb_snr_noise_psd);
  data.LoadDataFromBin("step1_prev_fb_snr_noise_pg_in_48khz.dump", ctx->fb_snr_noise_pg);
  data.LoadDataFromBin("step1_prev_sb_snr_noise_pg_in_48khz.dump", ctx->sb_snr_noise_pg);

  REQUIRE(ctx->fullband_hidden.size() == 2 * 256);

  denoise_task.data.context = ctx.get();
  denoise_task.data.input_spectrogram.clear();
  denoise_task.data.input_spectrogram.emplace_back(noisy_real_0, noisy_imag_0);
  denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;

  data.LoadDataFromBin("step1_prev_fb_conv1_buf_in_48khz.dump", ctx->fb_conv1_buf);
  data.LoadDataFromBin("step1_prev_fb_conv3_buf_in_48khz.dump", ctx->fb_conv3_buf);
  REQUIRE(ctx->fb_conv1_buf.size() == gt_b1_in_step1.size());
  REQUIRE(ctx->fb_conv3_buf.size() == gt_b3_in_step1.size());
  IsVectorsClose(gt_b1_in_step1, ctx->fb_conv1_buf, 1e-5f);
  IsVectorsClose(gt_b3_in_step1, ctx->fb_conv3_buf, 1e-5f);

  data.spench_model_runner->infer(&denoise_task);
  REQUIRE(denoise_task.stage == spench::PipelineStage::predicted_mask);
  IsVectorsClose(gt_fb_hidden_step1, ctx->fullband_hidden, 1e-5f);
  IsVectorsClose(gt_fb_snr_noise_psd_step1, ctx->fb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_sb_snr_noise_psd_step1, ctx->sb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_b1_out_step1, ctx->fb_conv1_buf, 1e-5f);
  IsVectorsClose(gt_b3_out_step1, ctx->fb_conv3_buf, 1e-5f);
  IsVectorsClose(gt_fb_snr_noise_pg_step1, ctx->fb_snr_noise_pg, 1e-5f);
  IsVectorsClose(gt_sb_snr_noise_pg_step1, ctx->sb_snr_noise_pg, 1e-5f);

  denoise_task.data.input_spectrogram.clear();
  denoise_task.data.input_spectrogram.emplace_back(noisy_real_1, noisy_imag_1);
  denoise_task.stage = spench::PipelineStage::fourier_transformed_spec;

  data.spench_model_runner->infer(&denoise_task);
  REQUIRE(denoise_task.stage == spench::PipelineStage::predicted_mask);
  IsVectorsClose(gt_fb_hidden_step2, ctx->fullband_hidden, 1e-5f);
  IsVectorsClose(gt_fb_snr_noise_psd_step2, ctx->fb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_sb_snr_noise_psd_step2, ctx->sb_snr_noise_psd, 1e-5f);
  IsVectorsClose(gt_b1_out_step2, ctx->fb_conv1_buf, 1e-5f);
  IsVectorsClose(gt_b3_out_step2, ctx->fb_conv3_buf, 1e-5f);
}
