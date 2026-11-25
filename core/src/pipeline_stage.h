#pragma once

#include <cstdint>

namespace giga_edge::enhance {

enum class ChunkOutcome : uint8_t {
  enhanced,
  warming_up,
};

enum class PipelineStage : uint8_t {
  waveform_ready,
  input_spectrum_ready,
  model_output_ready,
  enhanced_ready,
};

const char* to_string(PipelineStage stage);
const char* to_string(ChunkOutcome outcome);

bool is_transition_allowed(PipelineStage from, PipelineStage to);

class TaskState {
 public:
  [[nodiscard]] PipelineStage stage() const { return stage_; }
  [[nodiscard]] ChunkOutcome outcome() const { return outcome_; }

  [[nodiscard]] bool is(PipelineStage expected) const { return stage_ == expected; }
  [[nodiscard]] bool is(ChunkOutcome expected) const { return outcome_ == expected; }

  void set_outcome(ChunkOutcome outcome) { outcome_ = outcome; }
  void advance_to(PipelineStage next);
  void require(PipelineStage expected) const;

  void restart() {
    stage_ = PipelineStage::waveform_ready;
    outcome_ = ChunkOutcome::enhanced;
  }

 private:
  PipelineStage stage_ = PipelineStage::waveform_ready;
  ChunkOutcome outcome_ = ChunkOutcome::enhanced;
};

}  // namespace giga_edge::enhance
