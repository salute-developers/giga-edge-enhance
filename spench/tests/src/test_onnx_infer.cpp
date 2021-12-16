#include <memory>
#include <unordered_map>
#include <catch.hpp>
#include <iostream>

#include <contrib/onnxruntime/cmake/external/onnxruntime-extensions/includes/onnxruntime/onnxruntime_cxx_api.h>
void CreateConstInputsWithGivenShapes(const std::vector<std::vector<int64_t> >& input_shapes,
                                                        std::vector<Ort::Value>& tensors) {
  for (auto& shape : input_shapes) {
    auto length = std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<int64_t>());
    const auto kMemoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    std::vector<float> const_value_data(length, 1.0);
    tensors.emplace_back(Ort::Value::CreateTensor<float>(kMemoryInfo,
                                                            const_value_data.data(),
                                                            const_value_data.size(),
                                                            shape.data(),
                                                            shape.size()));
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

  TestOnnx& InitModel(const std::string& model_path) {
    std::string full_model_path = m_kBase + "models/" + model_path;

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

TEST_CASE("Run single dummy inference for onnx model", "[spench]") {
  auto data = std::move(TestOnnx()
      .InitModel("our_fsn_std_16khz_1952.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<const char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<const char*> output_names;
  std::vector<std::vector<int64_t> > output_shapes;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputName(i, allocator));
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputName(i, allocator));
    output_shapes.push_back(data.m_denoiser->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  CreateConstInputsWithGivenShapes(input_shapes, inputs);

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
}


TEST_CASE("Run dummy multi inference for onnx model and with one thread", "[spench]") {
  auto data = std::move(TestOnnx()
      .InitModel("our_fsn_std_16khz_1952.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<const char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<const char*> output_names;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputName(i, allocator));
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputName(i, allocator));
  }

  CreateConstInputsWithGivenShapes(input_shapes, inputs);

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
  std::cout << "infer with 1 thread completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts << std::endl;
}

TEST_CASE("Run dummy multi inference for onnx model and with two threads", "[spench]") {
  auto data = std::move(TestOnnx().SetThreadNum(2)
      .InitModel("our_fsn_std_16khz_1952.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<const char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<const char*> output_names;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputName(i, allocator));
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputName(i, allocator));
  }

  CreateConstInputsWithGivenShapes(input_shapes, inputs);

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
  std::cout << "infer with 2 threads completed: " <<
                                      tot << " | "
                                      << static_cast<double>(tot) / data.m_nattempts << std::endl;
}

TEST_CASE("Run dummy multi inference for onnx model and with four threads", "[spench]") {
  auto data = std::move(TestOnnx().SetThreadNum(4)
      .InitModel("our_fsn_std_16khz_1952.onnx"));

  std::vector<Ort::Value> inputs;
  std::vector<const char*> input_names;
  std::vector<std::vector<int64_t> > input_shapes;
  std::vector<const char*> output_names;

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < data.m_denoiser->GetInputCount(); ++i) {
    // print input node names
    input_names.push_back(data.m_denoiser->GetInputName(i, allocator));
    input_shapes.push_back(data.m_denoiser->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
  }

  for (size_t i = 0; i < data.m_denoiser->GetOutputCount(); ++i) {
    // print output node names
    output_names.push_back(data.m_denoiser->GetOutputName(i, allocator));
  }

  CreateConstInputsWithGivenShapes(input_shapes, inputs);

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
  std::cout << "infer with 4 threads completed: " <<
                                        tot <<
                                        " | " << static_cast<double>(tot) / data.m_nattempts << std::endl;
}

