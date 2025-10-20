#pragma once

#include <string>
#include <memory>
#include <utility>
#include <vector>
#include "spench/src/spench_default_configurations.h"

namespace YAML { class Node; class ParserException; }

namespace util {
namespace echo {
enum class EchoCancelerType;
}  // echo
}  // util

namespace spench {

inline YAML::Node lookup(const YAML::Node& root,
                         const std::initializer_list<std::string>& path) {
  YAML::Node node = YAML::Clone(root);
  for (const auto& key : path) {
    if (!node[key])    // missing or null
      return YAML::Node{};
    node = node[key];
  }
  return node;
}

/// Return node.as<T>() if present, otherwise fallback.
template<typename T>
inline T asOr(const YAML::Node& node, const T& fallback) {
  return (node && !node.IsNull()) ? node.as<T>() : fallback;
}

/// Return node.as<T>() if present.
template<typename T>
inline T as(const YAML::Node& node) {
  return node.as<T>();
}

static void mergeYaml(YAML::Node& dest, const YAML::Node &src) {
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

class ConfigReader {
 public:
  ConfigReader(const ConfigReader&) = delete;
  ConfigReader& operator=(const ConfigReader&) = delete;
  ConfigReader(ConfigReader&&) = default;
  ConfigReader& operator=(ConfigReader&&) = default;
  ConfigReader() = default;
  virtual ~ConfigReader() = default;
  inline const YAML::Node& params() const noexcept { return m_params; }

 protected:
  YAML::Node m_params;
};

//  Loads the entire document from a file path
class FileConfigReader final : public ConfigReader {
 public:
  explicit FileConfigReader(std::string path);
};

//  Loads from an in-memory YAML string
class StringConfigReader final : public ConfigReader {
 public:
  explicit StringConfigReader(std::string yaml);
};

class PipelineConfig : public ConfigReader {
 public:
  explicit PipelineConfig(const ConfigReader& base_reader, std::string default_config) {
    if (default_config.length()) {
      m_params = YAML::Load(default_config);
    }

    mergeYaml(m_params, base_reader.params());
  }

 protected:
  virtual void validate() const = 0;
};

class NetworkConfig : public PipelineConfig {
 public:
  explicit NetworkConfig(const ConfigReader& base_reader, std::string default_config = "")
      : PipelineConfig(base_reader, std::move(default_config)) {
    validate();
  }

  void validate() const override;
  std::string                 modelPath()       const;
  std::vector<std::string>    modelInputs()     const;
  std::vector<std::string>    modelOutputs()    const;
  uint32_t                    samplingFreq()    const;
  uint32_t                    frameSizeMs()     const;
  uint32_t                    frameStrideMs()   const;
  std::string                 windowType()      const;
  bool                        useGpu()          const;
  uint32_t                    numThreads()      const;
  std::string                 modelType()       const;

 protected:
  virtual std::vector<std::string> defaultModelInputs()  const = 0;
  virtual std::vector<std::string> defaultModelOutputs() const = 0;
};

class DenoiserConfig final : public NetworkConfig {
 public:
  explicit DenoiserConfig(const ConfigReader& base_reader)
      : NetworkConfig(base_reader, spench::DefaultDenoiserConfigYAML) {
  }

 protected:
  std::vector<std::string> defaultModelInputs()  const override;
  std::vector<std::string> defaultModelOutputs() const override;
};

class DereverbConfig final : public NetworkConfig {
 public:
  explicit DereverbConfig(const ConfigReader& base_reader)
      : NetworkConfig(base_reader, spench::DefaultDereverbConfigYAML) {
  }

 protected:
  std::vector<std::string> defaultModelInputs()  const override;
  std::vector<std::string> defaultModelOutputs() const override;
};

class AudioProcessorConfigYamlBlock {
 public:
  enum class Kind { Scalar, Sequence, Map };

  struct Field {
    const char* key;
    Kind        kind;
    bool        required;
  };

  static void validateSection(const YAML::Node& section,
                              const std::string& section_name,
                              std::initializer_list<Field> fields);
};

class ParamBlockView {
 public:
  ParamBlockView(const YAML::Node& root, std::string base, std::string block)
      : m_root(root), m_base(std::move(base)), m_block(std::move(block)) {}

  template<typename T>
  T get(const char* key) const {
    return as<T>(lookup(m_root, {m_base, m_block, key}));
  }

  YAML::Node node() const {
    return lookup(m_root, {m_base, m_block});
  }

 private:
  const YAML::Node& m_root;
  std::string m_base;
  std::string m_block;
};

class AudioProcessorConfig final : public PipelineConfig {
 public:
  explicit AudioProcessorConfig()
      : AudioProcessorConfig(StringConfigReader("")) {
  }

  explicit AudioProcessorConfig(const ConfigReader& base_reader)
      : PipelineConfig(base_reader, spench::DefaultAudioProcessingConfigYAML) {
    validate();
  }

  void validate() const override;

  // Universal typed getter
  template<typename T>
  T get(const char* block, const char* key) const {
    return as<T>(lookup(m_params, {"sound_processing", block, key}));
  }

  // Optional helper to get a view on a whole block
  ParamBlockView block(const std::string& name) const {
    return ParamBlockView(m_params, "sound_processing", name);
  }
};

class AecConfig final : public PipelineConfig {
 public:
    explicit AecConfig()
        : AecConfig(StringConfigReader("")) {
    }

    explicit AecConfig(const ConfigReader& base_reader)
        : PipelineConfig(base_reader, spench::DefaultAecConfigYAML) {
      validate();
    }

    void validate() const override;

    // typed getters (with code-baked defaults):
    uint32_t                     sampleRate()                      const;
    uint16_t                     filterLen()                       const;
    bool                         residualEchoSuppressionEnabled()  const;
    bool                         delayCompensatorEnabled()         const;
    uint16_t                     frameLen()                        const;
    util::echo::EchoCancelerType type()                            const;
};
}  // namespace spench
