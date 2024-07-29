#include "spench/util/onnx/denoise_onnx_runner.h"
#include "contrib/yaml-cpp/yaml.h"
#include "tests/src/utils.h"
#include <memory>
#include <unordered_map>
#include <catch.hpp>
#include <map>
#include <iostream>
#include "spench/src/denoise_context.h"

#include <contrib/onnxruntime/include/onnxruntime/core/session/onnxruntime_cxx_api.h>

void CreateConstInputsWithGivenShapes(const std::vector<std::vector<int64_t> >& input_shapes,
                                      std::vector<std::vector<float>>& const_allocated_data,
                                      std::vector<Ort::Value>& tensors) {
  for (auto& shape : input_shapes) {
    auto length = std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<int64_t>());
    const auto kMemoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    const_allocated_data.emplace_back(std::vector<float>(length, 1.0));
    tensors.emplace_back(Ort::Value::CreateTensor<float>(kMemoryInfo,
                                                            const_allocated_data.back().data(),
                                                            const_allocated_data.back().size(),
                                                            shape.data(),
                                                            shape.size()));
  }
}

void FreeNames(Ort::AllocatorWithDefaultOptions& allocator,
               std::vector<char*>& tensors) {
  for (uint16_t i = 0; i < tensors.size(); ++i) {
    allocator.Free(tensors[i]);
  }
}

struct TestOnnx {
  std::unique_ptr<Ort::Session> m_denoiser;

  const std::string m_kBase = "data/";

  const std::size_t m_nattempts = 100;

 private:
  Ort::SessionOptions m_session_options;

  Ort::Env m_session_env{ORT_LOGGING_LEVEL_WARNING, "Default"};

 public:
  TestOnnx() {
    m_session_options.SetIntraOpNumThreads(1);
    m_session_options.SetInterOpNumThreads(1);
    m_session_options.SetExecutionMode(ExecutionMode::ORT_PARALLEL);
    m_session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  }

  TestOnnx& InitModel(const std::string& model_name) {
    std::string full_model_path = m_kBase + "models/" + model_name;

    m_denoiser = std::unique_ptr<Ort::Session>(new Ort::Session(m_session_env,
                                                              full_model_path.c_str(),
                                                              m_session_options));
    return *this;
  }

  TestOnnx& SetThreadNum(int16_t th) {
    m_session_options.SetIntraOpNumThreads(th);
    m_session_options.SetInterOpNumThreads(th);
    return *this;
  }
};

struct TestOnnxRunner {
  std::unique_ptr<util::onnx::BaseOnnxRunner> m_denoiser_runner;

  const std::string m_kBase = "data/";

 public:
  uint16_t m_frame_size_ms = 16;
  uint64_t m_sample_rate = 16000;
  TestOnnxRunner() = default;

  TestOnnxRunner& InitRunner(const std::string& yaml_config_name,
                             const std::string& password = "",
                             const std::string& checksum = "") {
    std::string file_path = m_kBase + "config/" + yaml_config_name;
    YAML::Node node = YAML::LoadFile(file_path);
    bool on_gpu = node["inference"]["gpu"].as<bool>(false);
    std::string model_type = node["inference"]["model_type"].as<std::string>("best_quality");

    m_denoiser_runner = (model_type == "best_quality") ?
                    std::unique_ptr<util::onnx::BaseOnnxRunner>(
                                                new util::onnx::SpenchQOnnxRunner(node["model"],
                                                                                  password,
                                                                                  checksum,
                                                                                  on_gpu)):
                    std::unique_ptr<util::onnx::BaseOnnxRunner>(
                                                new util::onnx::SpenchFOnnxRunner(node["model"],
                                                                                  password,
                                                                                  checksum,
                                                                                  on_gpu));
    m_frame_size_ms = node["feature_params"]["frame_size_ms"].as<uint16_t>(16);
    m_sample_rate = node["feature_params"]["sampling_freq"].as<uint64_t>(16000);
    return *this;
  }

  TestOnnxRunner& LoadDataFromBin(const std::string& dump,
                        std::vector<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestOnnxRunner& LoadDataFromBin(const std::string& dump,
                        std::valarray<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
};

TEST_CASE("SUCCEED Run single dummy inference [onnx]") {
  auto data = std::move(TestOnnx()
      .InitModel("quality_model_with_layerNorm_16khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<int64_t> > output_shapes;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
    output_shapes.push_back(data.m_denoiser->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  Ort::RunOptions opt;
  auto outputs = data.m_denoiser->Run(opt,
                                    input_names.data(),
                                    inputs.data(),
                                    inputs.size(),
                                    output_names.data(),
                                    output_names.size());

  REQUIRE(output_shapes.size() == outputs.size());
  for (size_t i = 0; i < output_shapes.size(); ++i) {
    auto type_info = outputs[i].GetTensorTypeAndShapeInfo();
    REQUIRE(type_info.GetShape() == output_shapes[i]);
  }

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 16kHz and with one thread [onnx]") {
  auto data = std::move(TestOnnx()
      .InitModel("quality_model_with_layerNorm_16khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();

  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "16kHz best quality model infer with 1 thread completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 16kHz and with two threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(2)
      .InitModel("quality_model_with_layerNorm_16khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "16kHz best quality model infer with 2 threads completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 16kHz and with four threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(4)
      .InitModel("quality_model_with_layerNorm_16khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "16kHz best quality model infer with 4 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 16kHz and with eight threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(8)
      .InitModel("quality_model_with_layerNorm_16khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "16kHz best quality model infer with 8 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 24kHz and with one thread [onnx]") {
  auto data = std::move(TestOnnx()
      .InitModel("quality_model_with_layerNorm_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();

  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz best quality model infer with 1 thread completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 24kHz and with two threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(2)
      .InitModel("quality_model_with_layerNorm_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz best quality model infer with 2 threads completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 24kHz and with four threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(4)
      .InitModel("quality_model_with_layerNorm_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz best quality model infer with 4 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a model 24kHz and with eight threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(8)
      .InitModel("quality_model_with_layerNorm_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz best quality model infer with 8 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a fast model 24kHz and with one thread [onnx]") {
  auto data = std::move(TestOnnx()
      .InitModel("fast_model_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();

  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz fast model infer with 1 thread completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a fast model 24kHz and with two threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(2)
      .InitModel("fast_model_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz fast model infer with 2 threads completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a fast model 24kHz and with four threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(4)
      .InitModel("fast_model_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz fast model infer with 4 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("SUCCEED Run dummy multi inference for a fast model 24kHz and with eight threads [onnx]") {
  auto data = std::move(TestOnnx().SetThreadNum(8)
      .InitModel("fast_model_24khz.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<char*> output_names;
  std::vector<std::vector<float>> const_allocated_data;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputNameAllocated(i, allocator).release());
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputNameAllocated(i, allocator).release());
  }

  CreateConstInputsWithGivenShapes(input_shapes, const_allocated_data, inputs);

  std::vector<size_t> timings(data.m_nattempts);
  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < data.m_nattempts; ++i) {
    auto start = std::chrono::steady_clock::now();
    Ort::RunOptions opt;
    auto outputs = data.m_denoiser->Run(opt,
                                      input_names.data(),
                                      inputs.data(),
                                      inputs.size(),
                                      output_names.data(),
                                      output_names.size());
    auto stop = std::chrono::steady_clock::now();
    timings.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();
  std::cout << "24kHz fast model infer with 8 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts <<
                                        " ms per infer" << std::endl;

  FreeNames(allocator, input_names);
  FreeNames(allocator, output_names);
}

TEST_CASE("FAIL Read config for Inference Runner with incorrect model path [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("test_configuration_with_incorrect_model_path.yaml"),
                                            "Config reading error. Model path parameter is not a single string!");
}

TEST_CASE("FAIL Read config for Inference Runner with incorrect model inputs [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("test_configuration_with_incorrect_model_inputs.yaml"),
                                            "Config reading error. Model inputs parameter is not a sequence!");
}

TEST_CASE("FAIL Read config for Inference Runner with incorrect model outputs [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("test_configuration_with_incorrect_model_outputs.yaml"),
                                            "Config reading error. Model outputs parameter is not a sequence!");
}

TEST_CASE("FAIL Read config for Inference Runner with incorrect model context length [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("test_configuration_with_incorrect_model_ctx_len.yaml"),
                                            "Config reading error. Model context length is not a scalar!");
}

TEST_CASE("FAIL Read config for Inference Runner with not existed model path [onnx runner]") {
  std::string error_message = std::string("Model reading error. Detailes: Load model from ") +
                              std::string("data/models/not_existed_model.onnx failed:Load model ") +
                              std::string("data/models/not_existed_model.onnx failed. ") +
                              std::string("File doesn't exist");

  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("test_configuration_with_not_existed_model_path.yaml"), error_message.c_str());
}

TEST_CASE("FAIL Run cpu onnx runner with null pointer task and zeroed hidden layer [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_16khz.yaml"));
  CHECK_THROWS_WITH(data.m_denoiser_runner->Infer(nullptr),
                                            "Task data or context are not initialized!");
}

TEST_CASE("FAIL Run cpu onnx runner with null pointer context and zeroed hidden layer [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_16khz.yaml"));
  spench::DenoiseTask denoise_task;
  CHECK_THROWS_WITH(data.m_denoiser_runner->Infer(&denoise_task),
                                            "Task data or context are not initialized!");
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("FAIL Read config with incorrect encrypted model name [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("incorrect_configuration_best_quality_onnx_encrypted_16khz.yaml", "test"),
                                            "Model reading error. Detailes: file doesn't contain header");
}

TEST_CASE("FAIL Pass incorrect password for encrypted model [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_encrypted_16khz.yaml", "test1"),
                                            "Model reading error. Detailes: chunk is corrupted");
}

TEST_CASE("FAIL Pass no password for encrypted model [onnx runner]") {
  std::string error_message = std::string("Model reading error. Detailes: Load model from ") +
                              std::string("data/models/quality_model_with_layerNorm_16khz.onnx.crypt ") +
                              std::string("failed:Protobuf parsing failed.");

  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_encrypted_16khz.yaml", ""), error_message.c_str());
}

TEST_CASE("FAIL Pass incorrect password for licensed encrypted model [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_lic_encrypted_24khz.yaml", "qwerty1"),
                                            "Model reading error. Detailes: chunk is corrupted");
}

TEST_CASE("FAIL Pass incorrect checksum for licensed encrypted model [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_lic_encrypted_24khz.yaml",
                  "qwerty",
                  "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmp1="),
                                            "Model reading error. Detailes: error deserializing base64 checksum");
}
#endif

TEST_CASE("SUCCEED Run the best quality model and check network outputs (audio 16kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_16khz.yaml"));

  std::valarray<float> real;
  std::valarray<float> imag;
  std::vector<float> gt_real;
  std::vector<float> gt_imag;
  std::vector<float> gt_fullband_hidden;
  std::vector<float> gt_fullband_cell_state;
  std::vector<float> gt_subband_hidden;
  std::vector<float> gt_subband_cell_state;
  data.LoadDataFromBin("1_real_wav_stft.dump", real);
  data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
  data.LoadDataFromBin("1_real_network_output_for_zeroed_hiddens_16khz.dump", gt_real);
  data.LoadDataFromBin("1_imag_network_output_for_zeroed_hiddens_16khz.dump", gt_imag);
  data.LoadDataFromBin("1_predicted_full_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_predicted_full_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_predicted_sub_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_subband_hidden);
  data.LoadDataFromBin("1_predicted_sub_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_subband_cell_state);

  spench::DenoiseTask denoise_task;
  std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_data.m_input_spec.push_back(spench::ComplexNumber<float>(real, imag));
  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;

  data.m_denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_WAV);
  IsVectorsClose(gt_real, denoise_task.m_data.m_network_output.real, 0.000001);
  IsVectorsClose(gt_imag, denoise_task.m_data.m_network_output.imag, 0.000001);
  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->m_fullband_hidden, 0.000001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->m_fullband_cell_state, 0.000001);
  IsVectorsClose(gt_subband_hidden, ctx.get()->m_subband_hidden, 0.000001);
  IsVectorsClose(gt_subband_cell_state, ctx.get()->m_subband_cell_state, 0.000001);
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("SUCCEED Run encrypted the best quality model and check network outputs (audio 16kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_encrypted_16khz.yaml", "test"));

  std::valarray<float> real;
  std::valarray<float> imag;
  std::vector<float> gt_real;
  std::vector<float> gt_imag;
  std::vector<float> gt_fullband_hidden;
  std::vector<float> gt_fullband_cell_state;
  std::vector<float> gt_subband_hidden;
  std::vector<float> gt_subband_cell_state;
  data.LoadDataFromBin("1_real_wav_stft.dump", real);
  data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
  data.LoadDataFromBin("1_real_network_output_for_zeroed_hiddens_16khz.dump", gt_real);
  data.LoadDataFromBin("1_imag_network_output_for_zeroed_hiddens_16khz.dump", gt_imag);
  data.LoadDataFromBin("1_predicted_full_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_predicted_full_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_predicted_sub_band_hidden_state_for_zeroed_hiddens_16khz.dump", gt_subband_hidden);
  data.LoadDataFromBin("1_predicted_sub_band_cell_state_for_zeroed_hiddens_16khz.dump", gt_subband_cell_state);

  spench::DenoiseTask denoise_task;
  std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
  denoise_task.m_data.m_context = ctx.get();
  denoise_task.m_data.m_input_spec.push_back(spench::ComplexNumber<float>(real, imag));
  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;

  data.m_denoiser_runner->Infer(&denoise_task);

  REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_WAV);
  IsVectorsClose(gt_real, denoise_task.m_data.m_network_output.real, 0.000001);
  IsVectorsClose(gt_imag, denoise_task.m_data.m_network_output.imag, 0.000001);
  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);
  IsVectorsClose(gt_fullband_hidden, ctx.get()->m_fullband_hidden, 0.000001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->m_fullband_cell_state, 0.000001);
  IsVectorsClose(gt_subband_hidden, ctx.get()->m_subband_hidden, 0.000001);
  IsVectorsClose(gt_subband_cell_state, ctx.get()->m_subband_cell_state, 0.000001);
}
#endif

TEST_CASE("SUCCEED Run four frames with the best quality model and check states (audio 16kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_best_quality_onnx_16khz.yaml"));

  spench::DenoiseTask denoise_task;
  std::unique_ptr<spench::DenoiseContextQ<float>> ctx(new spench::DenoiseContextQ<float>);
  denoise_task.m_data.m_context = ctx.get();

  std::valarray<float> real;
  std::valarray<float> imag;
  data.LoadDataFromBin("1_real_wav_stft.dump", real);
  data.LoadDataFromBin("1_imag_wav_stft.dump", imag);
  denoise_task.m_data.m_input_spec.push_back(spench::ComplexNumber<float>(real, imag));
  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;
  uint64_t hop_size = static_cast<uint64_t>(data.m_sample_rate * data.m_frame_size_ms / 1000);
  denoise_task.m_data.m_enhanced_wav.assign(hop_size, 1.0);

  data.m_denoiser_runner->Infer(&denoise_task);
  REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_WAV);

  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);
  CHECK(std::vector<float>(ctx.get()->m_fullband_hidden.size(), 0.0) !=
                                                 ctx.get()->m_fullband_hidden);
  CHECK(std::vector<float>(ctx.get()->m_fullband_cell_state.size(), 0.0) !=
                                                 ctx.get()->m_fullband_cell_state);
  CHECK(std::vector<float>(ctx.get()->m_subband_hidden.size(), 0.0) !=
                                                 ctx.get()->m_subband_hidden);
  CHECK(std::vector<float>(ctx.get()->m_subband_cell_state.size(), 0.0) !=
                                                 ctx.get()->m_subband_cell_state);

  denoise_task.m_data.m_enhanced_wav.assign(hop_size, 1.0);
  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;

  data.m_denoiser_runner->Infer(&denoise_task);
  REQUIRE(denoise_task.m_stage == spench::PipelineStage::SKIPPED_WAV);
  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);
  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;
  denoise_task.m_data.m_enhanced_wav.assign(hop_size, 1.0);

  data.m_denoiser_runner->Infer(&denoise_task);
  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTIONS);
  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 0.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);

  denoise_task.m_stage = spench::PipelineStage::SPECTROGRAM;
  denoise_task.m_data.m_enhanced_wav.assign(hop_size, 1.0);

  data.m_denoiser_runner->Infer(&denoise_task);
  REQUIRE(denoise_task.m_stage == spench::PipelineStage::PREDICTIONS);
  CHECK(std::vector<float>(denoise_task.m_data.m_enhanced_wav.size(), 1.0) ==
                                                 denoise_task.m_data.m_enhanced_wav);
}

TEST_CASE("FAIL Read config for the best speed model 24kHz with error in inputs [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner().InitRunner("test_configuration_with_error_in_inputs.yaml"),
                     "The model has inputs inconsisted with provided ones");
}

TEST_CASE("FAIL Read config for the best speed model 24kHz with error number of outputs [onnx runner]") {
  CHECK_THROWS_WITH(TestOnnxRunner().InitRunner("test_configuration_with_error_in_outputs.yaml"),
                    "The model has 4 outputs, but you provided 3");
}

TEST_CASE("SUCCEED Read default config for the best speed model 24kHz [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_24khz.yaml"));
}

TEST_CASE("SUCCEED Run PrepareInputInPlace on one real frame with fast onnx model (audio 24 kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_24khz.yaml"));
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
  data.m_denoiser_runner->PrepareInputInPlace(noisy_mag_1_new, inputs, denoise_contex);

  IsVectorsClose(ctx.get()->m_noisy_mag, noisy_mag_5_new, 0.000001);
}

TEST_CASE("SUCCEED Run fast onnx model with real data and check first two iterations (audio 24 kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_24khz.yaml"));

  std::valarray<float> real, imag;
  std::vector<float> gt_fullband_hidden, gt_fullband_cell_state;
  std::vector<float> gt_real_output, gt_imag_output;
  std::vector<float> gt_prev_fb_out_5;
  data.LoadDataFromBin("1_noisy_real_in.dump", real);
  data.LoadDataFromBin("1_noisy_imag_in.dump", imag);
  data.LoadDataFromBin("1_fb_h_new_fast_24kHz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_new_fast_24kHz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_new_fast_24kHz.dump", gt_prev_fb_out_5);
  data.LoadDataFromBin("1_noisy_real_out.dump", gt_real_output);
  data.LoadDataFromBin("1_noisy_imag_out.dump", gt_imag_output);

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
  IsVectorsClose(gt_fullband_hidden, ctx.get()->m_fullband_hidden, 0.000001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->m_fullband_cell_state, 0.000001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->m_prev_fb_out, 0.000001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.m_network_output.real, 0.000001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.m_network_output.imag, 0.000001);
}

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
TEST_CASE("SUCCEED Run encrypted onnx model and check first two iterations (audio 24 kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_encrypted_24khz.yaml", "test"));

  std::valarray<float> real, imag;
  std::vector<float> gt_fullband_hidden, gt_fullband_cell_state;
  std::vector<float> gt_real_output, gt_imag_output;
  std::vector<float> gt_prev_fb_out_5;
  data.LoadDataFromBin("1_noisy_real_in.dump", real);
  data.LoadDataFromBin("1_noisy_imag_in.dump", imag);
  data.LoadDataFromBin("1_fb_h_new_fast_24kHz.dump", gt_fullband_hidden);
  data.LoadDataFromBin("1_fb_c_new_fast_24kHz.dump", gt_fullband_cell_state);
  data.LoadDataFromBin("1_prev_fb_out_5_new_fast_24kHz.dump", gt_prev_fb_out_5);
  data.LoadDataFromBin("1_noisy_real_out.dump", gt_real_output);
  data.LoadDataFromBin("1_noisy_imag_out.dump", gt_imag_output);

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
  IsVectorsClose(gt_fullband_hidden, ctx.get()->m_fullband_hidden, 0.000001);
  IsVectorsClose(gt_fullband_cell_state, ctx.get()->m_fullband_cell_state, 0.000001);
  IsVectorsClose(gt_prev_fb_out_5, ctx.get()->m_prev_fb_out, 0.000001);
  IsVectorsClose(gt_real_output, denoise_task.m_data.m_network_output.real, 0.000001);
  IsVectorsClose(gt_imag_output, denoise_task.m_data.m_network_output.imag, 0.000001);
}

TEST_CASE("SUCCEED Run licensed encrypted model and check first two iterations (24 kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_lic_encrypted_24khz.yaml",
                  "qwerty", "gGQdBounxW0kIBEURpYXmNxSAuWprxcMIvoMVQLRmpU="));
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

TEST_CASE("SUCCEED Run licensed encrypted model w/o lic check (24 kHz) [onnx runner]") {
  auto data = std::move(TestOnnxRunner()
      .InitRunner("default_configuration_fast_onnx_lic_encrypted_24khz.yaml",
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
#endif
