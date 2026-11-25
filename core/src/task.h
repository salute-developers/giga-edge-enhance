#pragma once

#include <yaml-cpp/yaml.h>

#include <chrono>  // NOLINT
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "src/context.h"
#include "src/pipeline_stage.h"

namespace giga_edge::enhance {

class Task;

using TaskPtr = std::shared_ptr<giga_edge::enhance::Task>;

struct TaskData {
  TaskData() : context(nullptr) {}
  Context<float>* context;
  std::vector<ComplexNumber<float>> input_spectrogram;
  ComplexNumber<float> new_spectrogram;
  std::vector<float> waveform;
  std::vector<float> enhanced_wav;
  // std::string                               error_text;
};

class DenoiseModelParams {
 public:
  std::string model_path;
  uint32_t network_input_ctx_len = 0;

  DenoiseModelParams() = default;

  void read_model_node(const YAML::Node& yaml_config);
};

class Task {
 public:
  TaskData data;
  std::string session_id;
  std::string message_id;
  TaskState state;
};

// float Frame2Seconds(int frames, const TaskParams& params);

std::string get_now_date();

std::string get_now_time();

namespace tools {
/**
 * \brief: Convert float32 to PCM16.
 * \param: A const vector of input float32 data.
 * \param: A vector of output PCM16 data.
 * \return: None
 * \ingroup group_cpp
 */
#ifdef _MSC_VER
__declspec(dllexport)
#endif
void convert_float32_to_pcm16(const std::vector<float>&, std::vector<int16_t>&);

/**
 * \brief: Convert PCM16 to float32.
 * \param: A const vector of input PCM16 data.
 * \param: A vector of output float32 data.
 * \return: None
 * \ingroup group_cpp
 */
#ifdef _MSC_VER
__declspec(dllexport)
#endif
void convert_pcm16_to_float32(const std::vector<int16_t>&, std::vector<float>&);
}  // namespace tools
}  // namespace giga_edge::enhance
