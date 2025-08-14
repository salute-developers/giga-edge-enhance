#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>
#include "spench/src/spench_config_reader.h"

#include "spench/util/echo/echo_canceler.h"

namespace spench {

void ConfigReader::mergeYaml(YAML::Node& dest, const YAML::Node &src) {
  if (!src.IsMap()) {
    return;
  }
  for (auto kv : src) {
      const auto key = kv.first.as<std::string>();
      const auto& val = kv.second;

      YAML::Node child = dest[key];
      if (child && child.IsMap() && val.IsMap()) {
          mergeYaml(child, val);
      } else {
          child = val;
      }
  }
}

FileConfigReader::FileConfigReader(std::string path)
: m_path(std::move(path)) {}

void FileConfigReader::load() {
  try {
    m_params = YAML::LoadFile(m_path);
  } catch (const YAML::Exception& e) {
    spdlog::critical("FileConfigReader: cannot load or parse '{}': {}", m_path, e.what());
    throw std::runtime_error(std::string("Config file load error: ") + e.what());
  }
}

// — StringConfigReader —
StringConfigReader::StringConfigReader(std::string yaml)
  : StringConfigReader(DefaultDenoiserConfigYAML, std::move(yaml))
{}

StringConfigReader::StringConfigReader(const char* default_yaml, std::string override_yaml)
  : m_default(default_yaml), m_yaml(std::move(override_yaml))
{}

void StringConfigReader::load() {
  // 1) load the chosen default blob
  m_params = YAML::Load(m_default);
  // 2) parse & merge the m_yaml
  auto userNode = YAML::Load(m_yaml);
  // 3) overlay it
  mergeYaml(m_params, userNode);
}

void DenoiserConfigReader::validate() const {
  spdlog::info("Yaml file reading: ");

  // Ensure we have a 'model' map
  const auto& model = lookup(m_params, {"model"});
  if (!model || !model.IsMap()) {
    spdlog::critical("denoiser: missing required 'model' section");
    throw std::runtime_error("Missing model section");
  }
  // --- model.model_path (required, scalar string) ---
  const auto& n_path = model["model_path"];

  if (!n_path || n_path.Type() != YAML::NodeType::Scalar) {
    spdlog::critical("Config reading error. Model path parameter is not a single string!");
    throw std::runtime_error("Config reading error. Model path parameter is not a single string!");
  }
  if (n_path.as<std::string>().empty()) {
    spdlog::critical("denoiser: missing required 'model.model_path'");
    throw std::runtime_error("Missing model.model_path");
  }

  spdlog::info("Model path {}", n_path.as<std::string>());
  // --- model.model_inputs (optional, must be a sequence of scalars) ---
  const auto& n_inputs = model["model_inputs"];
  if (n_inputs && !n_inputs.IsNull()) {
    if (!n_inputs.IsSequence()) {
      spdlog::critical("Config reading error. Model inputs parameter is not a sequence!");
      throw std::runtime_error("Config reading error. Model inputs parameter is not a sequence!");
    }
    for (const auto& item : n_inputs) {
      if (!item || !item.IsScalar()) {
        spdlog::critical("Config reading error. Model inputs parameter must be a sequence of scalars!");
        throw std::runtime_error("Config reading error. Model inputs parameter must be a sequence of scalars!");
      }
    }
    if (n_inputs.size() == 0) {
      spdlog::warn("denoiser: empty 'model.model_inputs'; using defaults");
    }
  } else {
    spdlog::warn("denoiser: missing 'model.model_inputs'; using defaults");
  }

  // --- model.model_outputs (required, must be a sequence of scalars) ---
  const auto& n_outputs = model["model_outputs"];
  if (n_outputs && !n_outputs.IsNull()) {
    if (!n_outputs.IsSequence()) {
      spdlog::critical("Config reading error. Model outputs parameter is not a sequence!");
      throw std::runtime_error("Config reading error. Model outputs parameter is not a sequence!");
    }
    for (const auto& item : n_outputs) {
      if (!item || !item.IsScalar()) {
        spdlog::critical("Config reading error. Model outputs parameter must be a sequence of scalars!");
        throw std::runtime_error("Config reading error. Model outputs parameter must be a sequence of scalars!");
      }
    }
    if (n_outputs.size() == 0) {
      spdlog::warn("denoiser: empty 'model.model_outputs'; using defaults");
    }
  } else {
    spdlog::warn("denoiser: missing 'model.model_outputs'; using defaults");
  }

  // --- model_input_ctx_len (optional; support both nested and legacy top-level) ---
  const auto& n_ctx = model["model_input_ctx_len"];
  if (n_ctx && !n_ctx.IsNull()) {
    if (n_ctx.Type() != YAML::NodeType::Scalar) {
      spdlog::critical("Config reading error. Model context length is not a scalar!");
      throw std::runtime_error("Config reading error. Model context length is not a scalar!");
    }
  }
}


std::string DenoiserConfigReader::modelPath() const {
  // safe after validate()
  return as<std::string>(lookup(m_params, {"model", "model_path"}));
}

std::vector<std::string> DenoiserConfigReader::modelInputs() const {
  static const std::vector<std::string> kDefault{
  #ifdef BUILD_TFLITE_ENGINE
      "noisy_mag", "fb_h", "fb_c", "prev_fb_out"
  #else
      "noisy_mag_5", "fb_h.1", "fb_c.1", "prev_fb_out_5"
  #endif
  };

  auto n = lookup(m_params, {"model", "model_inputs"});
  if (n && n.IsSequence() && n.size() > 0) {
    std::vector<std::string> out;
    out.reserve(n.size());
    for (const auto& item : n) {
      if (!item || !item.IsScalar())
        return kDefault;  // bad element → use defaults
      out.push_back(item.as<std::string>());
    }
    return out;
  }
  return kDefault;  // missing/null/empty/not a sequence
}

std::vector<std::string> DenoiserConfigReader::modelOutputs() const {
  static const std::vector<std::string> kDefault{
  #ifdef BUILD_TFLITE_ENGINE
      "fb_h", "fb_c", "updated_fb_out", "out_mask"
  #else
      "fb_h", "fb_c", "new_fb_out_5", "model_out"
  #endif
  };
  auto n = lookup(m_params, {"model", "model_outputs"});
  if (n && n.IsSequence() && n.size() > 0) {
    std::vector<std::string> out;
    out.reserve(n.size());
    for (const auto& item : n) {
      if (!item || !item.IsScalar())
        return kDefault;  // bad element → use defaults
      out.push_back(item.as<std::string>());
    }
    return out;
  }
  return kDefault;  // missing/null/empty/not a sequence
}

uint32_t DenoiserConfigReader::samplingFreq() const {
  return as<uint32_t>(lookup(m_params, {"feature_params", "sampling_freq"}));
}

uint32_t DenoiserConfigReader::frameSizeMs() const {
  return as<uint32_t>(lookup(m_params, {"feature_params", "frame_size_ms"}));
}

uint32_t DenoiserConfigReader::frameStrideMs() const {
  return as<uint32_t>(lookup(m_params, {"feature_params", "frame_stride_ms"}));
}

std::string DenoiserConfigReader::windowType() const {
  return as<std::string>(lookup(m_params, {"feature_params", "window_type"}));
}

bool DenoiserConfigReader::useGpu() const {
  return as<bool>(lookup(m_params, {"inference", "gpu"}));
}

uint32_t DenoiserConfigReader::numThreads() const {
  return as<uint32_t>(lookup(m_params, {"inference", "engine_num_threads"}));
}

std::string DenoiserConfigReader::modelType() const {
  return as<std::string>(lookup(m_params, {"inference", "model_type"}));
}

void AecConfigReader::validate() const {
  auto echo = lookup(m_params, {"echo_cancellation"});
  if (!echo || !echo.IsMap()) {
    spdlog::critical("AEC: missing 'echo_cancellation' section");
    throw std::runtime_error("Missing echo_cancellation section");
  }
  // Validate type
  auto tnode = lookup(m_params, {"echo_cancellation", "type"});
  auto type_str = asOr(tnode, std::string("speex"));
  if (type_str != "speex") {
    spdlog::critical("AEC: invalid type '{}'", type_str);
    throw std::runtime_error("Invalid echo canceler type: " + type_str);
  }
}

uint32_t AecConfigReader::sampleRate() const {
  return as<uint32_t>(lookup(m_params, {"echo_cancellation", "sample_rate"}));
}

uint16_t AecConfigReader::filterLen() const {
  auto node = lookup(m_params, {"echo_cancellation", "filter_len"});
  return as<uint16_t>(lookup(m_params, {"echo_cancellation", "filter_len"}));
}

bool AecConfigReader::residualEchoSuppressionEnabled() const {
  return as<bool>(lookup(m_params, {"echo_cancellation", "residual_echo_suppression_enabled"}));
}

bool AecConfigReader::delayCompensatorEnabled() const {
  return as<bool>(lookup(m_params, {"echo_cancellation", "delay_compensator_enabled"}));
}

uint16_t AecConfigReader::frameLen() const {
  // frame_len = round(sample_rate/1000 * chunk_len_ms)
  uint32_t sr = sampleRate();
  auto node     = lookup(m_params, {"echo_cancellation", "chunk_len_ms"});
  auto chunk_ms = asOr(node, static_cast<uint16_t>(16));
  float ppm     = sr / 1000.0f;
  return static_cast<uint16_t>(std::round(ppm * chunk_ms));
}

util::echo::EchoCancelerType AecConfigReader::type() const {
  // we know validate() guaranteed "speex"
  return util::echo::EchoCancelerType::SPEEX;
}

}  // namespace spench
