#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "spench/packed_model.h"
#include "spench/src/spench_default_configurations.h"

// Forward declarations to avoid exposing yaml-cpp in the public header.
namespace YAML {
class Node;
class ParserException;
}  // namespace YAML

namespace util {
namespace echo {
enum class EchoCancelerType;
}  // namespace echo
}  // namespace util

namespace spench {

// Declared here, implemented in the .cpp to keep yaml-cpp private.
YAML::Node lookup(const YAML::Node& root,
                  const std::initializer_list<std::string>& path);
void mergeYaml(YAML::Node& dest, const YAML::Node& src);

// -----------------------------------------------------------------------------

class ConfigReader {
 public:
  ConfigReader(const ConfigReader&) = delete;
  ConfigReader& operator=(const ConfigReader&) = delete;
  ConfigReader(ConfigReader&&) = default;
  ConfigReader& operator=(ConfigReader&&) = default;

  // Defined in .cpp to keep YAML complete at the point of destruction.
  ConfigReader();
  virtual ~ConfigReader();

  // Accessor does not require a complete YAML::Node type.
  inline const YAML::Node& params() const noexcept { return *m_params; }

 protected:
  // Pointer to an opaque YAML::Node to avoid including yaml-cpp in this header.
  std::unique_ptr<YAML::Node> m_params;
};

// Loads the entire document from a file path.
class FileConfigReader final : public ConfigReader {
 public:
  explicit FileConfigReader(std::string path);
};

// Loads from an in-memory YAML string.
class StringConfigReader final : public ConfigReader {
 public:
  explicit StringConfigReader(std::string yaml);
};

// -----------------------------------------------------------------------------

class PipelineConfig : public ConfigReader {
 public:
  // Defined in .cpp to avoid requiring yaml-cpp in this header.
  explicit PipelineConfig(const ConfigReader& base_reader,
                          std::string default_config);

 protected:
  virtual void validate() const = 0;
};

// -----------------------------------------------------------------------------

class NetworkConfig : public PipelineConfig {
 public:
  explicit NetworkConfig(const ConfigReader& base_reader,
                         std::string default_config = "")
      : PipelineConfig(base_reader, std::move(default_config)) {
    validate();
  }

  void validate() const override;
  std::string                 modelPath()        const;
  std::vector<std::string>    modelInputs()      const;
  std::vector<std::string>    modelOutputs()     const;
  uint32_t                    samplingFreq()     const;
  uint32_t                    frameSizeMs()      const;
  uint32_t                    frameStrideMs()    const;
  std::string                 windowType()       const;
  bool                        useGpu()           const;
  bool                        skipZeroedChunks() const;
  uint32_t                    numThreads()       const;
  std::string                 modelType()        const;
  bool                        isModelBundled()   const;
  const std::vector<uint8_t>& getModel()         const;

 protected:
  NetworkConfig(std::shared_ptr<spench::PackedModelReader> bundle,
                std::string default_config)
      : PipelineConfig(StringConfigReader(bundle->config()),
                       std::move(default_config)),
        bundle_(std::move(bundle)) {}

  virtual std::vector<std::string> defaultModelInputs() const = 0;
  virtual std::vector<std::string> defaultModelOutputs() const = 0;

  std::shared_ptr<spench::PackedModelReader> bundle_ = nullptr;
};

// -----------------------------------------------------------------------------

class DenoiserConfig final : public NetworkConfig {
 public:
  explicit DenoiserConfig(const ConfigReader& base_reader)
      : NetworkConfig(base_reader, spench::DefaultDenoiserConfigYAML) {}

  explicit DenoiserConfig(std::shared_ptr<spench::PackedModelReader> bundle)
      : NetworkConfig(std::move(bundle), spench::DefaultDenoiserConfigYAML) {}

 protected:
  std::vector<std::string> defaultModelInputs() const override;
  std::vector<std::string> defaultModelOutputs() const override;
};

// -----------------------------------------------------------------------------

class DereverbConfig final : public NetworkConfig {
 public:
  explicit DereverbConfig(const ConfigReader& base_reader)
      : NetworkConfig(base_reader, spench::DefaultDereverbConfigYAML) {}

  explicit DereverbConfig(std::shared_ptr<spench::PackedModelReader> bundle)
      : NetworkConfig(std::move(bundle), spench::DefaultDereverbConfigYAML) {}

 protected:
  std::vector<std::string> defaultModelInputs() const override;
  std::vector<std::string> defaultModelOutputs() const override;
};

// -----------------------------------------------------------------------------

class AudioProcessorConfigYamlBlock {
 public:
  enum class Kind { kScalar, kSequence, kMap };

  struct Field {
    const char* key;
    Kind kind;
    bool required;
  };

  static void validateSection(const YAML::Node& section,
                              const std::string& section_name,
                              std::initializer_list<Field> fields);
};

// -----------------------------------------------------------------------------

class ParamBlockView {
 public:
  ParamBlockView(const YAML::Node& root, std::string base, std::string block)
      : root_(root), base_(std::move(base)), block_(std::move(block)) {}

  template <typename T>
  T get(const char* key) const;  // Implemented + explicitly instantiated in .cpp.

  YAML::Node node() const;  // Implemented in .cpp.

 private:
  const YAML::Node& root_;
  std::string base_;
  std::string block_;
};

// -----------------------------------------------------------------------------

class AudioProcessorConfig final : public PipelineConfig {
 public:
  explicit AudioProcessorConfig() : AudioProcessorConfig(StringConfigReader("")) {}

  explicit AudioProcessorConfig(const ConfigReader& base_reader)
      : PipelineConfig(base_reader, spench::DefaultAudioProcessingConfigYAML) {
    validate();
  }

  void validate() const override;

  // Universal typed getter (implemented + explicitly instantiated in .cpp).
  template <typename T>
  T get(const char* block, const char* key) const;

  // Optional helper to get a view on a whole block.
  ParamBlockView block(const std::string& name) const {
    return ParamBlockView(params(), "sound_processing", name);
  }
};

// -----------------------------------------------------------------------------

class AecConfig final : public PipelineConfig {
 public:
  explicit AecConfig() : AecConfig(StringConfigReader("")) {}

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
    uint16_t                     echoPremute()                     const;
    util::echo::EchoCancelerType type()                            const;
};

}  // namespace spench
