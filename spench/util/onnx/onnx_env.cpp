#ifdef EMSCRIPTEN
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "spench/util/onnx/onnx_env.h"

#include <memory>
#include <mutex>  // NOLINT
#include <iostream>

namespace util {
namespace onnx {

const OrtApi* OrtApiPtr = OrtGetApiBase()->GetApi(ORT_API_VERSION);
static std::mutex OrtMutex;

void CheckStatus(OrtStatus* status) {
  if (status != NULL) {
    const char* msg = OrtApiPtr->GetErrorMessage(status);
    std::cerr << msg << std::endl;
    // VOICE_ERROR() << msg;
    // Logging::Terminate();
  }
}

static std::unique_ptr<Ort::Env> OrtEnvPtr;
static std::unique_ptr<Ort::SessionOptions> OrtSessOptionsCpuPtr;
static std::unique_ptr<Ort::SessionOptions> OrtSessOptionsGpuPtr;
static std::unique_ptr<Ort::MemoryInfo> OrtMemoryInfoCpuPtr;
static std::unique_ptr<Ort::MemoryInfo> OrtMemoryInfoGpuPtr;
static std::unique_ptr<std::mutex> OrtCpuSessionMutexPtr;
static std::unique_ptr<std::mutex> OrtGpuSessionMutexPtr;

std::mutex& GetCpuSessionMutex() {
  std::lock_guard<std::mutex> lock(OrtMutex);
  if (!OrtCpuSessionMutexPtr) {
    OrtCpuSessionMutexPtr = std::make_unique<std::mutex>();
  }
  return *OrtCpuSessionMutexPtr;
}
std::mutex& GetGpuSessionMutex() {
  std::lock_guard<std::mutex> lock(OrtMutex);
  if (!OrtGpuSessionMutexPtr) {
    OrtGpuSessionMutexPtr = std::make_unique<std::mutex>();
  }
  return *OrtGpuSessionMutexPtr;
}

void CreateDefaultEnv() {
  std::lock_guard<std::mutex> lock(OrtMutex);
  if (OrtEnvPtr) {
    return;
  }
  OrtThreadingOptions* threadingOptions = nullptr;
  CheckStatus(OrtApiPtr->CreateThreadingOptions(&threadingOptions));
  OrtEnvPtr = std::make_unique<Ort::Env>(threadingOptions, ORT_LOGGING_LEVEL_WARNING, "spench-onnx");
  OrtApiPtr->ReleaseThreadingOptions(threadingOptions);
  OrtEnvPtr->DisableTelemetryEvents();
}

void CreateDefaultSessionOptions(std::unique_ptr<Ort::SessionOptions>& options_ptr, uint32_t num_threads, bool gpu) {
  std::lock_guard<std::mutex> lock(OrtMutex);
  if (options_ptr) {
    return;
  }
  options_ptr = std::make_unique<Ort::SessionOptions>();
#ifdef EMSCRIPTEN
  options_ptr->SetIntraOpNumThreads(1);
  options_ptr->SetInterOpNumThreads(1);
  options_ptr->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
#else
  options_ptr->SetIntraOpNumThreads(num_threads);
  options_ptr->SetInterOpNumThreads(num_threads);
  options_ptr->SetExecutionMode(ExecutionMode::ORT_PARALLEL);
#endif
  options_ptr->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  options_ptr->DisableProfiling();
  options_ptr->DisableCpuMemArena();
  options_ptr->DisableMemPattern();
  if (gpu) {
    // Only single gpu currently working
    OrtCUDAProviderOptions cuda_options;
    cuda_options.device_id = 0;
    cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearch::DEFAULT;
    options_ptr->AppendExecutionProvider_CUDA(cuda_options);
  }
}

void CreateDefaultMemoryInfo(std::unique_ptr<Ort::MemoryInfo>& memory_info_ptr, bool gpu) {
  std::lock_guard<std::mutex> lock(OrtMutex);
  if (memory_info_ptr) {
    return;
  }
  (void)(gpu);
  if (!gpu) {
    memory_info_ptr = std::make_unique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator,
                                                                                   OrtMemTypeDefault));
  } else {
    memory_info_ptr = std::make_unique<Ort::MemoryInfo>("Cuda", OrtDeviceAllocator, 0, OrtMemTypeDefault);
  }
}

Ort::Env& GetOrtEnv() {
  if (!OrtEnvPtr) {
    CreateDefaultEnv();
  }
  return *OrtEnvPtr;
}

const Ort::SessionOptions& GetOrtSessionOptionsCpu(const uint32_t num_threads, bool restart_session) {
  if (restart_session) {
    OrtSessOptionsCpuPtr.release();
  }
  if (!OrtSessOptionsCpuPtr) {
    CreateDefaultSessionOptions(OrtSessOptionsCpuPtr, num_threads, false);
  }
  return *OrtSessOptionsCpuPtr;
}

const Ort::SessionOptions& GetOrtSessionOptionsGpu(const uint32_t num_threads, bool restart_session) {
  if (restart_session) {
    OrtSessOptionsCpuPtr.release();
  }
  if (!OrtSessOptionsGpuPtr) {
    CreateDefaultSessionOptions(OrtSessOptionsGpuPtr, num_threads, true);
  }
  return *OrtSessOptionsGpuPtr;
}

const Ort::MemoryInfo& GetMemoryInfoCpu() {
  if (!OrtMemoryInfoCpuPtr) {
    CreateDefaultMemoryInfo(OrtMemoryInfoCpuPtr, false);
  }
  return *OrtMemoryInfoCpuPtr;
}

const Ort::MemoryInfo& GetMemoryInfoGpu() {
  if (!OrtMemoryInfoGpuPtr) {
    CreateDefaultMemoryInfo(OrtMemoryInfoGpuPtr, true);
  }
  return *OrtMemoryInfoGpuPtr;
}

}  // namespace onnx

}  // namespace util
