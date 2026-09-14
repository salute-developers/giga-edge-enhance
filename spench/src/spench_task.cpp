#include "spench/src/spench_task.h"

#include <spdlog/spdlog.h>

#include <stdexcept>

#include "spench/src/spench_logger.h"

namespace spench {

#if defined(_MSC_VER)
// msvc-specific workaround. shamelessly stolen from stackoverflow
#define localtime_r(T, Tm) (localtime_s(Tm, T) ? NULL : Tm)
#endif

void DenoiseModelParams::read_model_node(const YAML::Node& yaml_config) {
  // Just read as already verified
  try {
    model_path = yaml_config["model_path"].as<std::string>();

    for (auto& item : yaml_config["model_inputs"]) {
      model_inputs.push_back(item.as<std::string>());
    }

    for (auto& item : yaml_config["model_outputs"]) {
      model_outputs.push_back(item.as<std::string>());
    }

    network_input_ctx_len = yaml_config["model_input_ctx_len"].as<uint32_t>(0);
  } catch(...) {
    throw std::runtime_error("Config reading error!");
  }
}

std::string get_now_date() {
  time_t now = time(nullptr);
  struct tm tmbuf;
  struct tm* tstruct;
  char buf[80];
  tstruct = localtime_r(&now, &tmbuf);

  strftime(buf, sizeof(buf), "%Y%m%d", tstruct);
  return buf;
}

std::string get_now_time() {
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
void convert_float32_to_pcm16(const std::vector<float>& input, std::vector<int16_t>& output) {
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
void convert_pcm16_to_float32(const std::vector<int16_t>& input, std::vector<float>& output) {
    output.resize(input.size());
    constexpr float inv_scale = 1.0f / std::numeric_limits<int16_t>::max();
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] * inv_scale;
    }
}
}  // namespace tools

}  // namespace spench
