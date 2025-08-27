#pragma once

#include <chrono>  // NOLINT
#include <memory>
#include <string>
#include <vector>
#include <limits>
#include "contrib/yaml-cpp/yaml.h"
#include "spench/src/spench_context.h"

namespace spench {

class SpenchTask;

using TaskPtr = std::shared_ptr<spench::SpenchTask>;

enum PipelineStage {
  ORIG_CHUNK = 0,
  FOURIER_TRANSFORMED_SPEC = 1,
  PREDICTED_MASK = 2,
  PREDICTED_SPEC = 3,
  ENHANCED_CHUNK = 4,
  SKIPPED_CHUNK = 5
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
  std::string m_model_path;
  std::vector<std::string> m_model_inputs;
  std::vector<std::string> m_model_outputs;
  uint32_t m_network_input_ctx_len = 0;

  DenoiseModelParams() = default;

  void ReadModelNode(const YAML::Node& yaml_config);
};

class SpenchTask {
 public:
  SpenchTaskData   m_data;
  std::string       m_session_id;
  std::string       m_message_id;
  PipelineStage     m_stage;
  SpenchTask(): m_stage(ORIG_CHUNK) {}
};

// float Frame2Seconds(int frames, const SpenchTaskParams& params);

std::string GetNowDate();

std::string GetNowTime();

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
void                              convertFloat32ToPCM16(const std::vector<float>&,
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
void                              convertPCM16ToFloat32(const std::vector<int16_t>&,
                                                              std::vector<float>&);
}  // namespace tools
}  // namespace spench
