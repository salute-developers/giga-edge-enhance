#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace util::sound {

class IVad {
 public:
  virtual ~IVad() = default;

  struct State {
    bool speech{false};
    float score{0};
  };

  virtual State process(const std::vector<float>& x) = 0;

  [[nodiscard]] virtual State state() const noexcept = 0;

  [[nodiscard]] virtual uint32_t frame_len() const noexcept { return 0; }

  [[nodiscard]] virtual uint32_t sample_rate() const noexcept { return 0; }

  [[nodiscard]] virtual uint32_t delay() const noexcept { return 0; }
};

enum class VadType { simple, rnnoise };

struct VadConfig {
  VadType type{VadType::simple};
  uint32_t sample_rate{16000};
  float threshold{0.5};
  std::optional<uint32_t> frame_len;
  std::optional<std::string> model_file;
};

class VadBuilder {
 public:
  VadBuilder() = default;
  explicit VadBuilder(const VadConfig& config);
  VadBuilder& set_sample_rate(uint32_t sample_rate);
  VadBuilder& set_type(VadType type);
  VadBuilder& set_threshold(float threshold);
  VadBuilder& set_frame_len(uint32_t frame_len);
  VadBuilder& set_model_file(const std::string& model_file);
  std::shared_ptr<IVad> create();

 private:
  VadConfig cfg_;
};

}  // namespace util::sound
