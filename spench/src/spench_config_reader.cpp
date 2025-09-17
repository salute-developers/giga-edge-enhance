#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
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

void NetworkConfigReader::validate() const {
  spdlog::info("Yaml file reading: ");

  // Ensure we have a 'model' map
  const auto& model = lookup(m_params, {"model"});
  if (!model || !model.IsMap()) {
    spdlog::critical("network: missing required 'model' section");
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
      spdlog::warn("network: empty 'model.model_inputs'; using defaults");
    }
  } else {
    spdlog::warn("network: missing 'model.model_inputs'; using defaults");
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
      spdlog::warn("network: empty 'model.model_outputs'; using defaults");
    }
  } else {
    spdlog::warn("network: missing 'model.model_outputs'; using defaults");
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

std::string NetworkConfigReader::modelPath() const {
  // safe after validate()
  return as<std::string>(lookup(m_params, {"model", "model_path"}));
}

std::vector<std::string> NetworkConfigReader::modelInputs() const {
  auto n = lookup(m_params, {"model", "model_inputs"});
  if (n && n.IsSequence() && n.size() > 0) {
    std::vector<std::string> out;
    out.reserve(n.size());
    for (const auto& item : n) {
      if (!item || !item.IsScalar())
        return defaultModelInputs();
      out.push_back(item.as<std::string>());
    }
    return out;
  }
  return defaultModelInputs();
}

std::vector<std::string> NetworkConfigReader::modelOutputs() const {
  auto n = lookup(m_params, {"model", "model_outputs"});
  if (n && n.IsSequence() && n.size() > 0) {
    std::vector<std::string> out;
    out.reserve(n.size());
    for (const auto& item : n) {
      if (!item || !item.IsScalar())
        return defaultModelOutputs();
      out.push_back(item.as<std::string>());
    }
    return out;
  }
  return defaultModelOutputs();
}

uint32_t NetworkConfigReader::samplingFreq() const {
  return as<uint32_t>(lookup(m_params, {"feature_params", "sampling_freq"}));
}

uint32_t NetworkConfigReader::frameSizeMs() const {
  return as<uint32_t>(lookup(m_params, {"feature_params", "frame_size_ms"}));
}

uint32_t NetworkConfigReader::frameStrideMs() const {
  return as<uint32_t>(lookup(m_params, {"feature_params", "frame_stride_ms"}));
}

std::string NetworkConfigReader::windowType() const {
  return as<std::string>(lookup(m_params, {"feature_params", "window_type"}));
}

bool NetworkConfigReader::useGpu() const {
  return as<bool>(lookup(m_params, {"inference", "gpu"}));
}

uint32_t NetworkConfigReader::numThreads() const {
  return as<uint32_t>(lookup(m_params, {"inference", "engine_num_threads"}));
}

std::string NetworkConfigReader::modelType() const {
  return as<std::string>(lookup(m_params, {"inference", "model_type"}));
}

// -------------------- DenoiserConfigReader --------------------
std::vector<std::string> DenoiserConfigReader::defaultModelInputs() const {
#ifdef BUILD_TFLITE_ENGINE
  return {"noisy_mag", "fb_h", "fb_c", "prev_fb_out"};
#else
  return {"noisy_mag_5", "fb_h.1", "fb_c.1", "prev_fb_out_5"};
#endif
}

std::vector<std::string> DenoiserConfigReader::defaultModelOutputs() const {
#ifdef BUILD_TFLITE_ENGINE
  return {"fb_h", "fb_c", "updated_fb_out", "out_mask"};
#else
  return {"fb_h", "fb_c", "new_fb_out_5", "model_out"};
#endif
}

// -------------------- DereverbConfigReader --------------------
std::vector<std::string> DereverbConfigReader::defaultModelInputs() const {
#ifdef BUILD_TFLITE_ENGINE
  return {""};
#else
  return {"noisy_spec_5"};
#endif
}

std::vector<std::string> DereverbConfigReader::defaultModelOutputs() const {
#ifdef BUILD_TFLITE_ENGINE
  return {""};
#else
  return {"model_out", "mu", "logvar"};
#endif
}

void AudioProcessingYamlBlock::validateSection(
    const YAML::Node& section,
    const std::string& section_name,
    std::initializer_list<Field> fields) {
  if (!section || !section.IsMap()) {
    spdlog::critical("{}: missing or not a map", section_name);
    throw std::runtime_error(section_name + ": missing or not a map");
  }

  for (const auto& f : fields) {
    const auto n = section[f.key];
    const std::string fq = section_name + "." + f.key;

    if (!n || n.IsNull()) {
      if (f.required) {
        spdlog::critical("Config reading error. {} is missing", fq);
        throw std::runtime_error("Config reading error. " + fq + " is missing");
      } else {
        continue;
      }
    }

    if (f.kind == Kind::Scalar) {
      if (!n.IsScalar()) {
        spdlog::critical("Config reading error. {} is not a scalar!", fq);
        throw std::runtime_error("Config reading error. " + fq + " is not a scalar!");
      }
    } else if (f.kind == Kind::Sequence) {
      if (!n.IsSequence()) {
        spdlog::critical("Config reading error. {} is not a sequence!", fq);
        throw std::runtime_error("Config reading error. " + fq + " is not a sequence!");
      }
    } else if (f.kind == Kind::Map) {
      if (!n.IsMap()) {
        spdlog::critical("Config reading error. {} is not a map!", fq);
        throw std::runtime_error("Config reading error. " + fq + " is not a map!");
      }
    }
  }
}

void AudioProcessingReader::validate() const {
  const auto& sp = lookup(m_params, {"sound_processing"});
  if (!sp || !sp.IsMap()) {
    spdlog::critical("sound_processing: missing required 'sound_processing' section");
    throw std::runtime_error("Missing sound_processing section");
  }

  AudioProcessingYamlBlock::validateSection(
      sp["range_ctrl"], "sound_processing.range_ctrl",
      {{"sample_rate", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"target_level_db", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"max_gain_db", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"attack_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"release_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"hold_time", AudioProcessingYamlBlock::Kind::Scalar, true}});

  AudioProcessingYamlBlock::validateSection(
      sp["auto_gain_ctrl"], "sound_processing.auto_gain_ctrl",
      {{"target_level", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"max_gain", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"average_len", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"step_size_rise", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"step_size_fall", AudioProcessingYamlBlock::Kind::Scalar, true}});

  AudioProcessingYamlBlock::validateSection(
      sp["noise_gate_ctrl"], "sound_processing.noise_gate_ctrl",
      {{"sample_rate", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"threshold", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"attack_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"release_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"hold_time", AudioProcessingYamlBlock::Kind::Scalar, true}});

  AudioProcessingYamlBlock::validateSection(
      sp["dynamic_range_compressor_ctrl"], "sound_processing.dynamic_range_compressor_ctrl",
      {{"sample_rate", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"threshold", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"ratio", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"knee_width", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"attack_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"release_time", AudioProcessingYamlBlock::Kind::Scalar, true}});

  AudioProcessingYamlBlock::validateSection(
      sp["loudness_ctrl"], "sound_processing.loudness_ctrl",
      {{"sample_rate", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"target_level_db", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"bypass_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"max_gain_db", AudioProcessingYamlBlock::Kind::Scalar, true}});

  AudioProcessingYamlBlock::validateSection(
      sp["limiter_ctrl"], "sound_processing.limiter_ctrl",
      {{"sample_rate", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"threshold", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"knee_width", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"attack_time", AudioProcessingYamlBlock::Kind::Scalar, true},
       {"release_time", AudioProcessingYamlBlock::Kind::Scalar, true}});
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
