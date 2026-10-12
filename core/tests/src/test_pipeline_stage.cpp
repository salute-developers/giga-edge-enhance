#include <catch.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "src/pipeline_stage.h"

namespace {

using gigaedge::spench::PipelineStage;

const std::vector<PipelineStage> all_stages = {
    PipelineStage::waveform_ready,
    PipelineStage::input_spectrum_ready,
    PipelineStage::model_output_ready,
    PipelineStage::enhanced_ready,
};

const std::vector<std::pair<PipelineStage, PipelineStage>> allowed_transitions = {
    {PipelineStage::waveform_ready, PipelineStage::input_spectrum_ready},
    {PipelineStage::input_spectrum_ready, PipelineStage::model_output_ready},
    {PipelineStage::model_output_ready, PipelineStage::enhanced_ready},
};

bool is_in_allowed_table(PipelineStage from, PipelineStage to) {
  for (const auto& transition : allowed_transitions) {
    if (transition.first == from && transition.second == to) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Pipeline transition table accepts exactly the pipeline contract [pipeline stage]") {
  for (const auto from : all_stages) {
    for (const auto to : all_stages) {
      INFO(gigaedge::spench::to_string(from) << " -> " << gigaedge::spench::to_string(to));
      REQUIRE(gigaedge::spench::is_transition_allowed(from, to) == is_in_allowed_table(from, to));
    }
  }
}

TEST_CASE("Task state validates transitions and can restart [pipeline stage]") {
  using gigaedge::spench::ChunkOutcome;
  using gigaedge::spench::TaskState;

  TaskState state;
  REQUIRE(state.is(PipelineStage::waveform_ready));
  REQUIRE(state.is(ChunkOutcome::enhanced));

  state.advance_to(PipelineStage::input_spectrum_ready);
  state.advance_to(PipelineStage::model_output_ready);
  state.set_outcome(ChunkOutcome::warming_up);
  state.advance_to(PipelineStage::enhanced_ready);

  CHECK_THROWS_AS(state.advance_to(PipelineStage::waveform_ready), std::runtime_error);
  state.restart();
  REQUIRE(state.is(PipelineStage::waveform_ready));
  REQUIRE(state.is(ChunkOutcome::enhanced));
}

TEST_CASE("Task state rejects an unexpected stage [pipeline stage]") {
  gigaedge::spench::TaskState state;

  CHECK_THROWS_AS(state.advance_to(PipelineStage::enhanced_ready), std::runtime_error);
  CHECK_THROWS_AS(state.require(PipelineStage::model_output_ready), std::runtime_error);
  REQUIRE_NOTHROW(state.require(PipelineStage::waveform_ready));
}

TEST_CASE("Pipeline errors identify both stages [pipeline stage]") {
  gigaedge::spench::TaskState state;
  try {
    state.advance_to(PipelineStage::enhanced_ready);
    FAIL("advance_to should have thrown");
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    CHECK(message.find("waveform_ready") != std::string::npos);
    CHECK(message.find("enhanced_ready") != std::string::npos);
  }
}
