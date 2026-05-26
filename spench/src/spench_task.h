#pragma once

#include <chrono>  // NOLINT
#include <memory>
#include <string>
#include <vector>
#include <limits>
#include <yaml-cpp/yaml.h>
#include "spench/src/spench_context.h"

namespace spench {

class SpenchTask;

using TaskPtr = std::shared_ptr<spench::SpenchTask>;

enum PipelineStage {
  orig_chunk = 0,
  fourier_transformed_spec = 1,
  predicted_mask = 2,
  predicted_spec = 3,
  enhanced_chunk = 4,
  skipped_chunk = 5
};

struct SpenchTaskData {
  SpenchTaskData() : context(nullptr) {}
  SpenchContext<float>*             context;
  std::vector<ComplexNumber<float>> input_spectrogram;
  ComplexNumber<float>              new_spectrogram;
  std::vector<float>                waveform;
  std::vector<float>                enhanced_wav;
  // std::string                               error_text;
};

class DenoiseModelParams {
 public:
  std::string model_path;
  std::vector<std::string> model_inputs;
  std::vector<std::string> model_outputs;
  uint32_t network_input_ctx_len = 0;

  DenoiseModelParams() = default;

  void read_model_node(const YAML::Node& yaml_config);
};

class SpenchTask {
 public:
  SpenchTaskData   data;
  std::string       session_id;
  std::string       message_id;
  PipelineStage     stage;
  SpenchTask(): stage(orig_chunk) {}
};

// float Frame2Seconds(int frames, const SpenchTaskParams& params);

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
void                              convert_float32_to_pcm16(const std::vector<float>&,
                                                              std::vector<int16_t>&);

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
void                              convert_pcm16_to_float32(const std::vector<int16_t>&,
                                                              std::vector<float>&);
}  // namespace tools
}  // namespace spench
