#ifdef EMSCRIPTEN
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "util/onnx/onnx_env.h"

#include <memory>
#include <mutex>  // NOLINT

namespace util {
namespace onnx {

static std::mutex ort_mutex;

static std::unique_ptr<Ort::SessionOptions> ort_sess_options_cpu_ptr;
static std::unique_ptr<Ort::SessionOptions> ort_sess_options_gpu_ptr;
static std::unique_ptr<Ort::MemoryInfo> ort_memory_info_cpu_ptr;
static std::unique_ptr<Ort::MemoryInfo> ort_memory_info_gpu_ptr;
static std::unique_ptr<std::mutex> ort_cpu_session_mutex_ptr;
static std::unique_ptr<std::mutex> ort_gpu_session_mutex_ptr;

std::mutex& get_cpu_session_mutex() {
  std::lock_guard<std::mutex> lock(ort_mutex);
  if (!ort_cpu_session_mutex_ptr) {
    ort_cpu_session_mutex_ptr = std::make_unique<std::mutex>();
  }
  return *ort_cpu_session_mutex_ptr;
}
std::mutex& get_gpu_session_mutex() {
  std::lock_guard<std::mutex> lock(ort_mutex);
  if (!ort_gpu_session_mutex_ptr) {
    ort_gpu_session_mutex_ptr = std::make_unique<std::mutex>();
  }
  return *ort_gpu_session_mutex_ptr;
}

void create_default_session_options(std::unique_ptr<Ort::SessionOptions>& options_ptr, uint32_t num_threads, bool gpu) {
  std::lock_guard<std::mutex> lock(ort_mutex);
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
    cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearch::OrtCudnnConvAlgoSearchDefault;
    options_ptr->AppendExecutionProvider_CUDA(cuda_options);
  }
}

void create_default_memory_info(std::unique_ptr<Ort::MemoryInfo>& memory_info_ptr, bool gpu) {
  std::lock_guard<std::mutex> lock(ort_mutex);
  if (memory_info_ptr) {
    return;
  }
  (void)(gpu);
  if (!gpu) {
    memory_info_ptr =
        std::make_unique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault));
  } else {
    memory_info_ptr = std::make_unique<Ort::MemoryInfo>("Cuda", OrtDeviceAllocator, 0, OrtMemTypeDefault);
  }
}

Ort::Env& get_ort_env() {
  // Keep the environment for the process lifetime: ReleaseEnv during static
  // destruction can lock an ORT mutex that has already been destroyed.
  // Function-local initialization also serializes concurrent first use.
  static Ort::Env* const env = [] {
    const Ort::ThreadingOptions threading_options;
    auto env = std::make_unique<Ort::Env>(threading_options, ORT_LOGGING_LEVEL_WARNING, "gigaedge-enhance-onnx");
    env->DisableTelemetryEvents();
    return env.release();
  }();
  return *env;
}

const Ort::SessionOptions& get_ort_session_options_cpu(const uint32_t num_threads, bool restart_session) {
  if (restart_session) {
    ort_sess_options_cpu_ptr.release();
  }
  if (!ort_sess_options_cpu_ptr) {
    create_default_session_options(ort_sess_options_cpu_ptr, num_threads, false);
  }
  return *ort_sess_options_cpu_ptr;
}

const Ort::SessionOptions& get_ort_session_options_gpu(const uint32_t num_threads, bool restart_session) {
  if (restart_session) {
    ort_sess_options_cpu_ptr.release();
  }
  if (!ort_sess_options_gpu_ptr) {
    create_default_session_options(ort_sess_options_gpu_ptr, num_threads, true);
  }
  return *ort_sess_options_gpu_ptr;
}

const Ort::MemoryInfo& get_memory_info_cpu() {
  if (!ort_memory_info_cpu_ptr) {
    create_default_memory_info(ort_memory_info_cpu_ptr, false);
  }
  return *ort_memory_info_cpu_ptr;
}

const Ort::MemoryInfo& get_memory_info_gpu() {
  if (!ort_memory_info_gpu_ptr) {
    create_default_memory_info(ort_memory_info_gpu_ptr, true);
  }
  return *ort_memory_info_gpu_ptr;
}

}  // namespace onnx

}  // namespace util
