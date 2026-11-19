#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "enhance/packed_model.h"
#include "src/default_configurations.h"

// Forward declarations to avoid exposing yaml-cpp in the public header.
// NOLINTBEGIN(readability-identifier-naming)
namespace YAML {
class Node;
class ParserException;
}  // namespace YAML
// NOLINTEND(readability-identifier-naming)

namespace util {
namespace echo {
enum class EchoCancelerType;
}  // namespace echo
}  // namespace util

namespace gigaedge::enhance {

// Declared here, implemented in the .cpp to keep yaml-cpp private.
YAML::Node lookup(const YAML::Node& root, const std::initializer_list<std::string>& path);
void merge_yaml(YAML::Node& dest, const YAML::Node& src);

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
  inline const YAML::Node& params() const noexcept { return *params_; }

 protected:
  // Pointer to an opaque YAML::Node to avoid including yaml-cpp in this header.
  std::unique_ptr<YAML::Node> params_;
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
  explicit PipelineConfig(const ConfigReader& base_reader, std::string default_config);

 protected:
  virtual void validate() const = 0;
};

// -----------------------------------------------------------------------------

class NetworkConfig : public PipelineConfig {
 public:
  explicit NetworkConfig(const ConfigReader& base_reader, std::string default_config = "")
      : PipelineConfig(base_reader, std::move(default_config)) {
    validate();
  }

  void validate() const override;
  std::string model_path() const;
  uint32_t sampling_freq() const;
  uint32_t frame_size_ms() const;
  uint32_t frame_stride_ms() const;
  std::string window_type() const;
  bool use_gpu() const;
  bool skip_zeroed_chunks() const;
  uint32_t num_threads() const;
  std::string model_type() const;
  bool is_model_bundled() const;
  const std::vector<uint8_t>& get_model() const;

 protected:
  NetworkConfig(std::shared_ptr<gigaedge::enhance::PackedModelReader> bundle, std::string default_config)
      : PipelineConfig(StringConfigReader(bundle->config()), std::move(default_config)), bundle_(std::move(bundle)) {}

  std::shared_ptr<gigaedge::enhance::PackedModelReader> bundle_ = nullptr;
};

// -----------------------------------------------------------------------------

class DenoiserConfig final : public NetworkConfig {
 public:
  explicit DenoiserConfig(const ConfigReader& base_reader)
      : NetworkConfig(base_reader, gigaedge::enhance::default_denoiser_config_yaml) {}

  explicit DenoiserConfig(std::shared_ptr<gigaedge::enhance::PackedModelReader> bundle)
      : NetworkConfig(std::move(bundle), gigaedge::enhance::default_denoiser_config_yaml) {}
};

// -----------------------------------------------------------------------------

class AudioProcessorConfigYamlBlock {
 public:
  enum class Kind { scalar, sequence, map };

  struct Field {
    const char* key;
    Kind kind;
    bool required;
  };

  static void validate_section(const YAML::Node& section, const std::string& section_name,
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
      : PipelineConfig(base_reader, gigaedge::enhance::default_audio_processing_config_yaml) {
    validate();
  }

  void validate() const override;

  // Universal typed getter (implemented + explicitly instantiated in .cpp).
  template <typename T>
  T get(const char* block, const char* key) const;

  // Optional helper to get a view on a whole block.
  ParamBlockView block(const std::string& name) const { return ParamBlockView(params(), "sound_processing", name); }
};

// -----------------------------------------------------------------------------

class AecConfig final : public PipelineConfig {
 public:
  explicit AecConfig() : AecConfig(StringConfigReader("")) {}

  explicit AecConfig(const ConfigReader& base_reader)
      : PipelineConfig(base_reader, gigaedge::enhance::default_aec_config_yaml) {
    validate();
  }

  void validate() const override;

  // typed getters (with code-baked defaults):
  uint32_t sample_rate() const;
  uint16_t filter_len() const;
  bool residual_echo_suppression_enabled() const;
  bool delay_compensator_enabled() const;
  uint16_t frame_len() const;
  uint16_t echo_premute() const;
  ::util::echo::EchoCancelerType type() const;
};

}  // namespace gigaedge::enhance
