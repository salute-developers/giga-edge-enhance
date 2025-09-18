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

class ConfigReader {
 public:
  virtual ~ConfigReader() = default;
  virtual void load() = 0;
  inline const YAML::Node& params() const noexcept { return m_params; }
  void mergeYaml(YAML::Node& dest, const YAML::Node& src);

 protected:
  YAML::Node m_params;
};

//  Loads the entire document from a file path
class FileConfigReader final : public ConfigReader {
 public:
  explicit FileConfigReader(std::string path);
  void load() override;

 private:
  std::string m_path;
};

//  Loads from an in-memory YAML string
class StringConfigReader final : public ConfigReader {
 public:
  explicit StringConfigReader(std::string yaml);
  StringConfigReader(const char* default_yaml, std::string override_yaml);
  void load() override;

 private:
  const char* m_default = nullptr;
  std::string m_yaml;
};

template<typename Derived>
class PipelineConfigReader : public ConfigReader {
 public:
  explicit PipelineConfigReader(std::unique_ptr<ConfigReader> base_reader)
      : m_base(std::move(base_reader))
  {}

  void load() final {
    m_base->load();
    m_params = m_base->params();
    validate();
  }

 protected:
  virtual void validate() const = 0;
  std::unique_ptr<ConfigReader> m_base;
};

class NetworkConfigReader : public PipelineConfigReader<NetworkConfigReader> {
 public:
  using PipelineConfigReader<NetworkConfigReader>::PipelineConfigReader;

  void validate() const override;
  std::string               modelPath()     const;
  std::vector<std::string>  modelInputs()   const;
  std::vector<std::string>  modelOutputs()  const;
  uint32_t                  samplingFreq()  const;
  uint32_t                  frameSizeMs()   const;
  uint32_t                  frameStrideMs() const;
  std::string               windowType()    const;
  bool                      useGpu()        const;
  uint32_t                  numThreads()    const;
  std::string               modelType()     const;

 protected:
  virtual std::vector<std::string> defaultModelInputs()  const = 0;
  virtual std::vector<std::string> defaultModelOutputs() const = 0;
};

// -------------------- Конкретные пайплайны --------------------
class DenoiserConfigReader final : public NetworkConfigReader {
 public:
  using NetworkConfigReader::NetworkConfigReader;
  void validate() const override {
    NetworkConfigReader::validate();
  }

 protected:
  std::vector<std::string> defaultModelInputs()  const override;
  std::vector<std::string> defaultModelOutputs() const override;
};

class DereverbConfigReader final : public NetworkConfigReader {
 public:
  using NetworkConfigReader::NetworkConfigReader;
  void validate() const override {
    NetworkConfigReader::validate();
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

template<typename T>
T as(const YAML::Node& node);

YAML::Node lookup(const YAML::Node& root,
                  const std::initializer_list<std::string>& path);

template<typename Derived>
class PipelineConfigReader;

class AudioProcessorConfigReader final : public PipelineConfigReader<AudioProcessorConfigReader> {
 public:
  explicit AudioProcessorConfigReader(std::unique_ptr<ConfigReader> base_reader)
      : PipelineConfigReader<AudioProcessorConfigReader>(std::move(base_reader)) {}

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

class AecConfigReader final : public PipelineConfigReader<AecConfigReader> {
 public:
    explicit AecConfigReader(std::unique_ptr<ConfigReader> base_reader)
    : PipelineConfigReader<AecConfigReader>(std::move(base_reader)) {}
    /// Ensure the `echo_cancellation` block exists and `type` is valid
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
