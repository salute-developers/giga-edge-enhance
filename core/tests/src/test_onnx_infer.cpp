#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <catch.hpp>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>

#include "src/context.h"
#include "tests/src/utils.h"
#include "util/onnx/denoise_onnx_runner.h"
#include "util/onnx/onnx_env.h"

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
    const auto full_model_path = std::filesystem::u8path(data_root + "models/" + model_name);

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
  std::unique_ptr<util::onnx::BaseOnnxRunner> enhance_model_runner;

  const std::string data_root = "data/";

 public:
  uint16_t frame_size_ms = 16;
  uint64_t sample_rate = 16000;
  TestOnnxRunner() = default;

  TestOnnxRunner& InitRunner(const std::string& yaml_config_name) {
    std::string file_path = data_root + "config/" + yaml_config_name;
    auto reader = giga_edge::enhance::FileConfigReader(file_path);
    bool on_gpu = reader.params()["inference"]["gpu"].as<bool>(false);
    std::string model_type = reader.params()["inference"]["model_type"].as<std::string>("denoiser_discr_web_model");

    if (model_type == "denoiser_discr_hq_model") {
      auto config = std::make_shared<giga_edge::enhance::DenoiserConfig>(reader);
      enhance_model_runner = std::unique_ptr<util::onnx::BaseOnnxRunner>(
          new util::onnx::DenoiserDiscriminativeNbOnnxRunner(config, 1, on_gpu));
    } else if (model_type == "denoiser_discr_web_model") {
      auto config = std::make_shared<giga_edge::enhance::DenoiserConfig>(reader);
      enhance_model_runner = std::unique_ptr<util::onnx::BaseOnnxRunner>(
          new util::onnx::DenoiserDiscriminativeHqOnnxRunner(config, 1, on_gpu));
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
  auto data = std::move(TestOnnxRunner().InitRunner("default_configuration_discr_web_onnx_48khz.yaml"));
  CHECK_THROWS_WITH(data.enhance_model_runner->infer(nullptr), "Task data or context are not initialized!");
}

TEST_CASE("FAIL Run cpu onnx runner with null pointer context and zeroed hidden layer [onnx runner]") {
  auto data = std::move(TestOnnxRunner().InitRunner("default_configuration_discr_web_onnx_48khz.yaml"));
  giga_edge::enhance::Task denoise_task;
  CHECK_THROWS_WITH(data.enhance_model_runner->infer(&denoise_task), "Task data or context are not initialized!");
}
