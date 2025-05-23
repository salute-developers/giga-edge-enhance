#include "spench/src/spench_task.h"
#include <spdlog/spdlog.h>
#include "spench/src/spench_logger.h"

namespace spench {

#if defined(_MSC_VER)
// msvc-specific workaround. shamelessly stolen from stackoverflow
#define localtime_r(T, Tm) (localtime_s(Tm, T) ? NULL : Tm)
#endif

void DenoiseModelParams::ReadModelNode(const YAML::Node& yaml_config) {
  spdlog::info("Yaml file reading:");
  if (yaml_config["model_path"] && !yaml_config["model_path"].IsNull()) {
    if (yaml_config["model_path"].Type() != YAML::NodeType::Scalar) {
      throw("Config reading error. Model path parameter is not a single string!");
    }
    m_model_path = yaml_config["model_path"].as<std::string>();
    spdlog::info("Model path {}", m_model_path);
  }

  if (yaml_config["model_inputs"] && !yaml_config["model_inputs"].IsNull()) {
    if (yaml_config["model_inputs"].Type() != YAML::NodeType::Sequence) {
      throw("Config reading error. Model inputs parameter is not a sequence!");
    }
    for (auto& item : yaml_config["model_inputs"]) {
      m_model_inputs.push_back(item.as<std::string>());
    }
  }

  if (yaml_config["model_outputs"] && !yaml_config["model_outputs"].IsNull()) {
    if (yaml_config["model_outputs"].Type() != YAML::NodeType::Sequence) {
      throw("Config reading error. Model outputs parameter is not a sequence!");
    }
    for (auto& item : yaml_config["model_outputs"]) {
      m_model_outputs.push_back(item.as<std::string>());
    }
  }

  if (yaml_config["model_input_ctx_len"] && !yaml_config["model_input_ctx_len"].IsNull()) {
    if (yaml_config["model_input_ctx_len"].Type() != YAML::NodeType::Scalar) {
      throw("Config reading error. Model context length is not a scalar!");
    }
    m_network_input_ctx_len = yaml_config["model_input_ctx_len"].as<uint32_t>();
  }
}

std::string GetNowDate() {
  time_t now = time(nullptr);
  struct tm tmbuf;
  struct tm* tstruct;
  char buf[80];
  tstruct = localtime_r(&now, &tmbuf);

  strftime(buf, sizeof(buf), "%Y%m%d", tstruct);
  return buf;
}

std::string GetNowTime() {
  time_t now = time(nullptr);
  struct tm tmbuf;
  struct tm* tstruct;
  char buf[80];
  tstruct = localtime_r(&now, &tmbuf);

  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tstruct);
  return buf;
}

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
void convertFloat32ToPCM16(const std::vector<float>& input, std::vector<int16_t>& output) {
    output.resize(input.size());
    constexpr float scale = static_cast<float>(std::numeric_limits<int16_t>::max());
    for (size_t i = 0; i < input.size(); ++i) {
        float scaled = input[i] * scale;
        // Clamp the value to the range of int16_t
        if (scaled > scale) scaled = scale;
        if (scaled < -scale) scaled = -scale;
        output[i] = static_cast<int16_t>(std::round(scaled));
    }
}

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
void convertPCM16ToFloat32(const std::vector<int16_t>& input, std::vector<float>& output) {
    output.resize(input.size());
    constexpr float inv_scale = 1.0f / std::numeric_limits<int16_t>::max();
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] * inv_scale;
    }
}
}  // namespace tools

}  // namespace spench
