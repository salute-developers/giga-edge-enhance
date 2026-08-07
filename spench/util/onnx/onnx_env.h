#pragma once

#include <mutex>  // NOLINT
#include <onnxruntime/include/onnxruntime/core/session/onnxruntime_cxx_api.h>

namespace util {

namespace onnx {

std::mutex& get_cpu_session_mutex();
std::mutex& get_gpu_session_mutex();
Ort::Env& get_ort_env();
const Ort::SessionOptions& get_ort_session_options_cpu(const uint32_t num_threads = 1,
                                                   bool restart_session = false);
const Ort::SessionOptions& get_ort_session_options_gpu(const uint32_t num_threads = 1,
                                                   bool restart_session = false);
const Ort::MemoryInfo& get_memory_info_cpu();
const Ort::MemoryInfo& get_memory_info_gpu();

}  // namespace onnx
}  // namespace util
