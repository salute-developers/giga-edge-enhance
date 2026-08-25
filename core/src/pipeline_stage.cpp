#include "src/pipeline_stage.h"

#include <stdexcept>
#include <string>

namespace gigaedge::denoiser {

const char* to_string(PipelineStage stage) {
  switch (stage) {
    case PipelineStage::waveform_ready:
      return "waveform_ready";
    case PipelineStage::input_spectrum_ready:
      return "input_spectrum_ready";
    case PipelineStage::model_output_ready:
      return "model_output_ready";
    case PipelineStage::enhanced_ready:
      return "enhanced_ready";
  }
  return "unknown";
}

const char* to_string(ChunkOutcome outcome) {
  switch (outcome) {
    case ChunkOutcome::enhanced:
      return "enhanced";
    case ChunkOutcome::warming_up:
      return "warming_up";
  }
  return "unknown";
}

bool is_transition_allowed(PipelineStage from, PipelineStage to) {
  switch (from) {
    case PipelineStage::waveform_ready:
      return to == PipelineStage::input_spectrum_ready;
    case PipelineStage::input_spectrum_ready:
      return to == PipelineStage::model_output_ready;
    case PipelineStage::model_output_ready:
      return to == PipelineStage::enhanced_ready;
    case PipelineStage::enhanced_ready:
      return false;
  }
  return false;
}

void TaskState::advance_to(PipelineStage next) {
  if (!is_transition_allowed(stage_, next)) {
    throw std::runtime_error(std::string("Pipeline stage transition '") + to_string(stage_) + "' -> '" +
                             to_string(next) + "' is not allowed");
  }
  stage_ = next;
}

void TaskState::require(PipelineStage expected) const {
  if (stage_ != expected) {
    throw std::runtime_error(std::string("Pipeline stage is '") + to_string(stage_) + "' but '" + to_string(expected) +
                             "' is required");
  }
}

}  // namespace gigaedge::denoiser
