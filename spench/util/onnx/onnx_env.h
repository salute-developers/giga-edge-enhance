#pragma once

#include <mutex>  // NOLINT
#include <contrib/onnxruntime/include/onnxruntime/core/session/onnxruntime_cxx_api.h>

namespace util {

namespace onnx {

std::mutex& GetCpuSessionMutex();
std::mutex& GetGpuSessionMutex();
Ort::Env& GetOrtEnv();
const Ort::SessionOptions& GetOrtSessionOptionsCpu(const uint32_t num_threads = 1,
                                                   bool restart_session = false);
const Ort::SessionOptions& GetOrtSessionOptionsGpu(const uint32_t num_threads = 1,
                                                   bool restart_session = false);
const Ort::MemoryInfo& GetMemoryInfoCpu();
const Ort::MemoryInfo& GetMemoryInfoGpu();

}  // namespace onnx
}  // namespace util
