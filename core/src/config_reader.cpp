#include "src/config_reader.h"  // IWYU: keep own header first

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "util/echo/echo_canceler.h"

namespace gigaedge::denoiser {
namespace {

// Local helpers kept in this TU to avoid exposing yaml-cpp in the header.
template <typename T>
T as(const YAML::Node& node) {
  return node.as<T>();
}

template <typename T>
T as_or(const YAML::Node& node, const T& fallback) {
  if (node && !node.IsNull()) {
    return node.as<T>();
  } else {
    return fallback;
  }
}

// Validates optional sequence-of-scalars node with exact logging texts used in tests.
inline void validate_optional_seq_of_scalars(const YAML::Node& node,
                                             const char* noun,        // e.g. "Model inputs"
                                             const char* fq_field) {  // e.g. "model.model_inputs"
  if (node && !node.IsNull()) {
    if (!node.IsSequence()) {
      spdlog::critical("Config reading error. {} parameter is not a sequence!", noun);
      throw std::runtime_error("Config reading error. " + std::string(noun) + " parameter is not a sequence!");
    }
    for (const auto& item : node) {
      if (!item || !item.IsScalar()) {
        spdlog::critical("Config reading error. {} parameter must be a sequence of scalars!", noun);
        throw std::runtime_error("Config reading error. " + std::string(noun) +
                                 " parameter must be a sequence of scalars!");
      }
    }
    if (node.size() == 0) {
      spdlog::warn("network: empty '{}'; using defaults", fq_field);
    }
  } else {
    spdlog::warn("network: missing '{}'; using defaults", fq_field);
  }
}

// Requires a scalar node; logs and throws with the provided exact message.
inline void require_scalar_node(const YAML::Node& node, const char* err_msg) {
  if (!node || node.Type() != YAML::NodeType::Scalar) {
    spdlog::critical(err_msg);
    throw std::runtime_error(err_msg);
  }
}

// If present, requires scalar; logs and throws with the provided exact message.
inline void require_if_present_scalar_node(const YAML::Node& node, const char* err_msg) {
  if (node && !node.IsNull()) {
    if (node.Type() != YAML::NodeType::Scalar) {
      spdlog::critical(err_msg);
      throw std::runtime_error(err_msg);
    }
  }
}

}  // namespace

// Free functions declared in the header.
YAML::Node lookup(const YAML::Node& root, const std::initializer_list<std::string>& path) {
  YAML::Node node = YAML::Clone(root);
  for (const auto& key : path) {
    if (!node[key]) {
      return YAML::Node{};
    }
    node = node[key];
  }
  return node;
}

void merge_yaml(YAML::Node& dest, const YAML::Node& src) {
  if (!src.IsMap()) {
    return;
  }
  for (auto kv : src) {
    const auto key = kv.first.as<std::string>();
    const auto& val = kv.second;

    YAML::Node child = dest[key];
    if (child && child.IsMap() && val.IsMap()) {
      merge_yaml(child, val);
    } else {
      child = val;
    }
  }
}

// -----------------------------------------------------------------------------

ConfigReader::ConfigReader() : params_(new YAML::Node{}) {}

ConfigReader::~ConfigReader() = default;

// -----------------------------------------------------------------------------

PipelineConfig::PipelineConfig(const ConfigReader& base_reader, std::string default_config) {
  params_.reset(new YAML::Node{});
  if (!default_config.empty()) {
    *params_ = YAML::Load(default_config);
  }
  merge_yaml(*params_, base_reader.params());
}

// -----------------------------------------------------------------------------

FileConfigReader::FileConfigReader(std::string path) {
  try {
    params_.reset(new YAML::Node{YAML::LoadFile(path)});
  } catch (const YAML::Exception& e) {
    spdlog::critical("FileConfigReader: cannot load or parse '{}': {}", path, e.what());
    throw std::runtime_error(std::string("Config file load error: ") + e.what());
  }
}

// -----------------------------------------------------------------------------

StringConfigReader::StringConfigReader(std::string yaml) {
  try {
    params_.reset(new YAML::Node{YAML::Load(yaml)});
  } catch (const YAML::Exception& e) {
    spdlog::critical("StringConfigReader: cannot parse string: {}", e.what());
    throw std::runtime_error(std::string("Config file load error: ") + e.what());
  }
}

// -----------------------------------------------------------------------------

// ParamBlockView moved-from-header.
YAML::Node ParamBlockView::node() const { return lookup(root_, {base_, block_}); }

template <typename T>
T ParamBlockView::get(const char* key) const {
  return as<T>(lookup(root_, {base_, block_, key}));
}

// Explicit instantiations to avoid including yaml-cpp at the call sites.
template uint32_t ParamBlockView::get<uint32_t>(const char*) const;
template uint16_t ParamBlockView::get<uint16_t>(const char*) const;
template double ParamBlockView::get<double>(const char*) const;
template float ParamBlockView::get<float>(const char*) const;
template bool ParamBlockView::get<bool>(const char*) const;
template std::string ParamBlockView::get<std::string>(const char*) const;

// -----------------------------------------------------------------------------

// AudioProcessorConfig::get moved-from-header.
template <typename T>
T AudioProcessorConfig::get(const char* block, const char* key) const {
  return as<T>(lookup(params(), {"sound_processing", block, key}));
}

// Explicit instantiations to avoid including yaml-cpp at the call sites.
template uint32_t AudioProcessorConfig::get<uint32_t>(const char*, const char*) const;
template uint16_t AudioProcessorConfig::get<uint16_t>(const char*, const char*) const;
template double AudioProcessorConfig::get<double>(const char*, const char*) const;
template float AudioProcessorConfig::get<float>(const char*, const char*) const;
template bool AudioProcessorConfig::get<bool>(const char*, const char*) const;
template std::string AudioProcessorConfig::get<std::string>(const char*, const char*) const;

// -----------------------------------------------------------------------------

void NetworkConfig::validate() const {
  spdlog::info("Yaml file reading: ");

  // Ensure we have a 'model' map.
  const auto& model = lookup(params(), {"model"});
  if (!model || !model.IsMap()) {
    spdlog::critical("network: missing required 'model' section");
    throw std::runtime_error("Missing model section");
  }

  // model.model_path (required if not bundled).
  const auto& n_path = model["model_path"];
  const bool model_bundled = is_model_bundled();
  if (!model_bundled) {
    if (!n_path || n_path.Type() != YAML::NodeType::Scalar) {
      spdlog::critical("Config reading error. Model path parameter is not a single string!");
      throw std::runtime_error("Config reading error. Model path parameter is not a single string!");
    }
    if (n_path.as<std::string>().empty()) {
      spdlog::critical("denoiser: missing required 'model.model_path'");
      throw std::runtime_error("Missing model.model_path");
    }
  }

  if (model_bundled) {
    spdlog::info("Model is bundled");
  } else {
    spdlog::info("Model path {}", n_path.as<std::string>());
  }

  // model.model_inputs (optional sequence of scalars).
  const auto& n_inputs = model["model_inputs"];
  validate_optional_seq_of_scalars(n_inputs, "Model inputs", "model.model_inputs");

  // model.model_outputs (optional sequence of scalars).
  const auto& n_outputs = model["model_outputs"];
  validate_optional_seq_of_scalars(n_outputs, "Model outputs", "model.model_outputs");

  // model_input_ctx_len (optional scalar).
  const auto& n_ctx = model["model_input_ctx_len"];
  require_if_present_scalar_node(n_ctx, "Config reading error. Model context length is not a scalar!");

  // feature_params section.
  const auto& features = lookup(params(), {"feature_params"});
  if (!features || !features.IsMap()) {
    spdlog::critical("network: missing required 'feature_params' section");
    throw std::runtime_error("Missing feature_params section");
  }

  // Required scalars inside feature_params.
  struct ScalarCheck {
    const char* key;
    const char* err;
  };
  static const ScalarCheck checks[] = {
      {"sampling_freq", "Config reading error. Sampling frequency is not a scalar!"},
      {"frame_size_ms", "Config reading error. Frame size is not a scalar!"},
      {"frame_stride_ms", "Config reading error. Frame size stide is not a scalar!"},
      {"window_type", "Config reading error. Window type is not a single string"},
  };
  for (const auto& c : checks) {
    require_scalar_node(features[c.key], c.err);
  }
}

std::string NetworkConfig::model_path() const {
  if (is_model_bundled()) {
    return "";
  }
  return as<std::string>(lookup(params(), {"model", "model_path"}));
}

std::vector<std::string> NetworkConfig::model_inputs() const {
  auto n = lookup(params(), {"model", "model_inputs"});
  if (n && n.IsSequence() && n.size() > 0) {
    std::vector<std::string> out;
    out.reserve(n.size());
    for (const auto& item : n) {
      if (!item || !item.IsScalar()) {
        return default_model_inputs();
      }
      out.push_back(item.as<std::string>());
    }
    return out;
  }
  return default_model_inputs();
}

std::vector<std::string> NetworkConfig::model_outputs() const {
  auto n = lookup(params(), {"model", "model_outputs"});
  if (n && n.IsSequence() && n.size() > 0) {
    std::vector<std::string> out;
    out.reserve(n.size());
    for (const auto& item : n) {
      if (!item || !item.IsScalar()) {
        return default_model_outputs();
      }
      out.push_back(item.as<std::string>());
    }
    return out;
  }
  return default_model_outputs();
}

uint32_t NetworkConfig::sampling_freq() const {
  return as<uint32_t>(lookup(params(), {"feature_params", "sampling_freq"}));
}

uint32_t NetworkConfig::frame_size_ms() const {
  return as<uint32_t>(lookup(params(), {"feature_params", "frame_size_ms"}));
}

uint32_t NetworkConfig::frame_stride_ms() const {
  return as<uint32_t>(lookup(params(), {"feature_params", "frame_stride_ms"}));
}

std::string NetworkConfig::window_type() const {
  return as<std::string>(lookup(params(), {"feature_params", "window_type"}));
}

bool NetworkConfig::use_gpu() const { return as<bool>(lookup(params(), {"inference", "gpu"})); }

bool NetworkConfig::skip_zeroed_chunks() const {
  auto zeroed_chunks = lookup(params(), {"inference", "skip_zeroed_chunks"});
  bool res = as_or(zeroed_chunks, static_cast<bool>(false));
  return res;
}

uint32_t NetworkConfig::num_threads() const {
  return as<uint32_t>(lookup(params(), {"inference", "engine_num_threads"}));
}

std::string NetworkConfig::model_type() const { return as<std::string>(lookup(params(), {"inference", "model_type"})); }

bool NetworkConfig::is_model_bundled() const {
  const auto& model = lookup(params(), {"model"});
  return model["model_bundled"] && model["model_bundled"].as<bool>();
}

const std::vector<uint8_t>& NetworkConfig::get_model() const {
  if (!is_model_bundled() || bundle_ == nullptr) {
    throw std::runtime_error("the model is not bundled or not present");
  }
  return bundle_->model();
}

// -----------------------------------------------------------------------------

std::vector<std::string> DenoiserConfig::default_model_inputs() const {
#ifdef BUILD_TFLITE_ENGINE
  return {"noisy_mag", "fb_h", "fb_c", "prev_fb_out"};
#else
  return {"noisy_mag_5", "fb_h.1", "fb_c.1", "prev_fb_out_5"};
#endif
}

std::vector<std::string> DenoiserConfig::default_model_outputs() const {
#ifdef BUILD_TFLITE_ENGINE
  return {"fb_h", "fb_c", "updated_fb_out", "out_mask"};
#else
  return {"fb_h", "fb_c", "new_fb_out_5", "model_out"};
#endif
}

// -----------------------------------------------------------------------------

void AudioProcessorConfigYamlBlock::validate_section(const YAML::Node& section, const std::string& section_name,
                                                     std::initializer_list<Field> fields) {
  if (!section || !section.IsMap()) {
    spdlog::warn("{}: missing or not a map", section_name);
    return;
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

    if (f.kind == Kind::scalar) {
      // Treat as scalar: numbers, bools, strings. Some yaml-cpp versions report bool/float as
      // !IsScalar() while they are still single leaf values — reject only structured types.
      if (n.IsMap() || n.IsSequence()) {
        spdlog::critical("Config reading error. {} is not a scalar (expected leaf value, got map/sequence)", fq);
        throw std::runtime_error("Config reading error. " + fq +
                                 " is not a scalar (expected leaf value, got map/sequence)!");
      }
    } else if (f.kind == Kind::sequence) {
      if (!n.IsSequence()) {
        spdlog::critical("Config reading error. {} is not a sequence!", fq);
        throw std::runtime_error("Config reading error. " + fq + " is not a sequence!");
      }
    } else if (f.kind == Kind::map) {
      if (!n.IsMap()) {
        spdlog::critical("Config reading error. {} is not a map!", fq);
        throw std::runtime_error("Config reading error. " + fq + " is not a map!");
      }
    }
  }
}

// -----------------------------------------------------------------------------

void AudioProcessorConfig::validate() const {
  const auto& sp = lookup(params(), {"sound_processing"});
  if (!sp || !sp.IsMap()) {
    spdlog::critical("sound_processing: missing required 'sound_processing' section");
    throw std::runtime_error("Missing sound_processing section");
  }

  AudioProcessorConfigYamlBlock::validate_section(
      sp["range_ctrl"], "sound_processing.range_ctrl",
      {{"sample_rate", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"target_level_db", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"max_gain_db", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"attack_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"release_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"hold_time", AudioProcessorConfigYamlBlock::Kind::scalar, true}});

  AudioProcessorConfigYamlBlock::validate_section(
      sp["auto_gain_ctrl"], "sound_processing.auto_gain_ctrl",
      {{"target_level", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"max_gain", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"average_len", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"step_size_rise", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"step_size_fall", AudioProcessorConfigYamlBlock::Kind::scalar, true}});

  AudioProcessorConfigYamlBlock::validate_section(sp["noise_gate_ctrl"], "sound_processing.noise_gate_ctrl",
                                                  {{"sample_rate", AudioProcessorConfigYamlBlock::Kind::scalar, true},
                                                   {"threshold", AudioProcessorConfigYamlBlock::Kind::scalar, true},
                                                   {"attack_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
                                                   {"release_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
                                                   {"hold_time", AudioProcessorConfigYamlBlock::Kind::scalar, true}});

  AudioProcessorConfigYamlBlock::validate_section(
      sp["dynamic_range_compressor_ctrl"], "sound_processing.dynamic_range_compressor_ctrl",
      {{"sample_rate", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"threshold", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"ratio", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"knee_width", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"attack_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"release_time", AudioProcessorConfigYamlBlock::Kind::scalar, true}});

  AudioProcessorConfigYamlBlock::validate_section(
      sp["loudness_ctrl"], "sound_processing.loudness_ctrl",
      {{"sample_rate", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"target_level_db", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"bypass_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"max_gain_db", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"use_vad", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"post_processing", AudioProcessorConfigYamlBlock::Kind::scalar, false}});

  AudioProcessorConfigYamlBlock::validate_section(
      sp["limiter_ctrl"], "sound_processing.limiter_ctrl",
      {{"sample_rate", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"threshold", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"knee_width", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"attack_time", AudioProcessorConfigYamlBlock::Kind::scalar, true},
       {"release_time", AudioProcessorConfigYamlBlock::Kind::scalar, true}});

  auto finite = [](double v) { return std::isfinite(v); };
  auto ensure = [](bool cond, const std::string& what) {
    if (!cond) {
      spdlog::critical("Invalid value: {}", what);
      throw std::runtime_error("Invalid value: " + what);
    }
  };

  // RC
  if (sp["range_ctrl"]) {
    const auto sec = sp["range_ctrl"];
    const auto sr = sec["sample_rate"].as<uint32_t>();
    const auto tldb = sec["target_level_db"].as<double>();
    const auto maxdb = sec["max_gain_db"].as<double>();
    const auto at = sec["attack_time"].as<double>();
    const auto rt = sec["release_time"].as<double>();
    const auto ht = sec["hold_time"].as<double>();

    ensure(sr > 0 && sr <= 100000, "range_ctrl.sample_rate must be in (0, 100000]");
    ensure(finite(tldb), "range_ctrl.target_level_db must be finite");
    ensure(finite(maxdb) && maxdb >= 0.0, "range_ctrl.max_gain_db must be finite and >= 0");
    ensure(finite(at) && at >= 0.0, "range_ctrl.attack_time must be finite and >= 0");
    ensure(finite(rt) && rt >= 0.0, "range_ctrl.release_time must be finite and >= 0");
    ensure(finite(ht) && ht >= 0.0, "range_ctrl.hold_time must be finite and >= 0");
  }

  // AGC
  if (sp["auto_gain_ctrl"]) {
    const auto sec = sp["auto_gain_ctrl"];
    const auto tl = sec["target_level"].as<double>();
    const auto mg = sec["max_gain"].as<double>();
    const auto al = sec["average_len"].as<uint32_t>();
    const auto tr = sec["step_size_rise"].as<double>();
    const auto tf = sec["step_size_fall"].as<double>();

    ensure(finite(tl) && tl > 0.0, "auto_gain_ctrl.target_level must be finite and > 0");
    ensure(finite(mg) && mg > 0.0, "auto_gain_ctrl.max_gain must be finite and > 0");
    ensure(al > 0u, "auto_gain_ctrl.average_len must be > 0");
    ensure(finite(tr) && tr > 0.0, "auto_gain_ctrl.step_size_rise must be finite and > 0");
    ensure(finite(tf) && tf > 0.0, "auto_gain_ctrl.step_size_fall must be finite and > 0");
  }

  // NG
  if (sp["noise_gate_ctrl"]) {
    const auto sec = sp["noise_gate_ctrl"];
    const auto sr = sec["sample_rate"].as<uint32_t>();
    const auto th = sec["threshold"].as<double>();
    const auto at = sec["attack_time"].as<double>();
    const auto rt = sec["release_time"].as<double>();
    const auto ht = sec["hold_time"].as<double>();

    ensure(sr > 0 && sr <= 100000, "noise_gate_ctrl.sample_rate must be in (0, 100000]");
    ensure(finite(th), "noise_gate_ctrl.threshold must be finite");
    ensure(finite(at) && at > 0.0, "noise_gate_ctrl.attack_time must be finite and > 0");
    ensure(finite(rt) && rt > 0.0, "noise_gate_ctrl.release_time must be finite and > 0");
    ensure(finite(ht) && ht > 0.0, "noise_gate_ctrl.hold_time must be finite and > 0");
  }

  // DRC
  if (sp["dynamic_range_compressor_ctrl"]) {
    const auto sec = sp["dynamic_range_compressor_ctrl"];
    const auto sr = sec["sample_rate"].as<uint32_t>();
    const auto th = sec["threshold"].as<double>();
    const auto ratio = sec["ratio"].as<uint32_t>();
    const auto knee = sec["knee_width"].as<double>();
    const auto at = sec["attack_time"].as<double>();
    const auto rt = sec["release_time"].as<double>();

    ensure(sr > 0 && sr <= 100000, "dynamic_range_compressor_ctrl.sample_rate must be in (0, 100000]");
    ensure(finite(th), "dynamic_range_compressor_ctrl.threshold must be finite");
    ensure(ratio >= 1, "dynamic_range_compressor_ctrl.ratio must be >= 1");
    ensure(finite(knee) && knee >= 0.0, "dynamic_range_compressor_ctrl.knee_width must be finite and >= 0");
    ensure(finite(at) && at >= 0.0, "dynamic_range_compressor_ctrl.attack_time must be finite and >= 0");
    ensure(finite(rt) && rt >= 0.0, "dynamic_range_compressor_ctrl.release_time must be finite and >= 0");
  }

  // LC
  if (sp["loudness_ctrl"]) {
    const auto sec = sp["loudness_ctrl"];
    const auto sr = sec["sample_rate"].as<uint32_t>();
    const auto tldb = sec["target_level_db"].as<double>();
    const auto bt = sec["bypass_time"].as<double>();
    const auto mg = sec["max_gain_db"].as<double>();

    ensure(sr >= 100 && sr <= 100000, "loudness_ctrl.sample_rate must be in [100, 100000]");
    ensure(finite(tldb), "loudness_ctrl.target_level_db must be finite");
    ensure(finite(mg) && mg >= 0.0, "loudness_ctrl.max_gain_db must be finite and >= 0");
    ensure(finite(bt) && bt >= 0.0, "loudness_ctrl.bypass_time must be finite and >= 0");
  }

  // Limiter
  if (sp["limiter_ctrl"]) {
    const auto sec = sp["limiter_ctrl"];
    const auto sr = sec["sample_rate"].as<uint32_t>();
    const auto th = sec["threshold"].as<double>();
    const auto knee = sec["knee_width"].as<double>();
    const auto at = sec["attack_time"].as<double>();
    const auto rt = sec["release_time"].as<double>();

    ensure(sr > 0 && sr <= 100000, "limiter_ctrl.sample_rate must be in (0, 100000]");
    ensure(finite(th), "limiter_ctrl.threshold must be finite");
    ensure(finite(knee) && knee >= 0.0, "limiter_ctrl.knee_width must be finite and >= 0");
    ensure(finite(at) && at >= 0.0, "limiter_ctrl.attack_time must be finite and >= 0");
    ensure(finite(rt) && rt >= 0.0, "limiter_ctrl.release_time must be finite and >= 0");
  }
}

// -----------------------------------------------------------------------------

void AecConfig::validate() const {
  auto echo = lookup(params(), {"echo_cancellation"});
  if (!echo || !echo.IsMap()) {
    spdlog::critical("AEC: missing 'echo_cancellation' section");
    throw std::runtime_error("Missing echo_cancellation section");
  }
  // Validate type
  auto tnode = lookup(params(), {"echo_cancellation", "type"});
  auto type_str = as_or(tnode, std::string("speex"));
  if (type_str != "speex") {
    spdlog::critical("AEC: invalid type '{}'", type_str);
    throw std::runtime_error("Invalid echo canceler type: " + type_str);
  }
}

uint32_t AecConfig::sample_rate() const { return as<uint32_t>(lookup(params(), {"echo_cancellation", "sample_rate"})); }

uint16_t AecConfig::filter_len() const {
  auto node = lookup(params(), {"echo_cancellation", "filter_len"});
  return as<uint16_t>(node);
}

bool AecConfig::residual_echo_suppression_enabled() const {
  return as<bool>(lookup(params(), {"echo_cancellation", "residual_echo_suppression_enabled"}));
}

bool AecConfig::delay_compensator_enabled() const {
  return as<bool>(lookup(params(), {"echo_cancellation", "delay_compensator_enabled"}));
}

uint16_t AecConfig::echo_premute() const {
  return as<uint16_t>(lookup(params(), {"echo_cancellation", "echo_premute_ms"}));
}

uint16_t AecConfig::frame_len() const {
  // frame_len = round(sample_rate/1000 * chunk_len_ms)
  uint32_t sr = sample_rate();
  auto node = lookup(params(), {"echo_cancellation", "chunk_len_ms"});
  auto chunk_ms = as_or(node, static_cast<uint16_t>(16));
  float ppm = sr / 1000.0f;
  return static_cast<uint16_t>(std::round(ppm * chunk_ms));
}

::util::echo::EchoCancelerType AecConfig::type() const {
  // validate() guarantees "speex".
  return ::util::echo::EchoCancelerType::speex;
}

}  // namespace gigaedge::denoiser
