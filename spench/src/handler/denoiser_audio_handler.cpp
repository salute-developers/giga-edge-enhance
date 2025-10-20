#include "spench/src/handler/denoiser_audio_handler.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include "spench/src/spench_logger.h"
#include "spench/src/denoiser_model_adviser.h"

namespace spench {

DenoiserAudioHandler::DenoiserAudioHandler(std::shared_ptr<DenoiserConfig> config):
                                  DenoiserAudioHandler(std::move(config), "", "") {}

DenoiserAudioHandler::DenoiserAudioHandler(std::shared_ptr<DenoiserConfig> config,
                                          const std::string& password,
                                          const std::string& checksum):
                                          NeuralNetworkAudioHandler(std::move(config), password, checksum),
                                          m_adviser(nullptr) {
  float frame_shift_ms = dynamic_cast<spench::DenoiserConfig*>(m_config.get())->frameStrideMs();
  static constexpr float window_duration_ms = 1000 * 5;
  m_adviser = std::make_unique<DenoiserModelAdviser<float>>(window_duration_ms, frame_shift_ms);
}

DenoiserAudioHandler::~DenoiserAudioHandler() = default;

SpenchContext<float>* DenoiserAudioHandler::CreateContext(uint32_t input_sample_rate,
                                                          uint32_t output_sample_rate) {
  auto ctx = (m_inference_runner_ptr->GetModelType() == InferenceRunner::ModelType::DENOISER_BEST_QUALITY) ?
                                    dynamic_cast<SpenchContext<float>*>(new DenoiseContextQ<float>()):
                                    dynamic_cast<SpenchContext<float>*>(new DenoiseContextF<float>());
  if (input_sample_rate == 0) {
    input_sample_rate = dynamic_cast<spench::DenoiserConfig*>(m_config.get())->samplingFreq();
  }
  if (output_sample_rate == 0) {
    output_sample_rate = dynamic_cast<spench::DenoiserConfig*>(m_config.get())->samplingFreq();
  }

  SpenchRetCode ret_code = InitAudioStreams(ctx, input_sample_rate, output_sample_rate);
  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Due to errors a context could not be created!");
    throw("Due to errors a context could not be created!");
  }
  spdlog::info("New denoising context created! Input sr {} Output sr {}", input_sample_rate, output_sample_rate);
  return ctx;
}

SpenchRetCode DenoiserAudioHandler::InitAudioStreams(SpenchContext<float>* ctx,
                                                     uint32_t input_sample_rate,
                                                     uint32_t output_sample_rate) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (ctx == nullptr) {
    return SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }

  if (ret_code == SpenchRetCode::SUCCESS) {
    try {
      if (!ctx->input_sound_stream_ptr) {
        ctx->input_sound_stream_ptr =
                  util::sound::BuildSoundStream<float>(input_sample_rate,
                              dynamic_cast<spench::DenoiserConfig*>(m_config.get())->samplingFreq(),
                              dynamic_cast<spench::DenoiserConfig*>(m_config.get())->frameStrideMs(),
                                                       util::sound::TemplateType::INPUT);
      }
      if (!ctx->output_sound_stream_ptr) {
        ctx->output_sound_stream_ptr =
          util::sound::BuildSoundStream<float>(
                                dynamic_cast<spench::DenoiserConfig*>(m_config.get())->samplingFreq(),
                                output_sample_rate,
                                dynamic_cast<spench::DenoiserConfig*>(m_config.get())->frameStrideMs(),
                                util::sound::TemplateType::OUTPUT);
      }
    } catch(const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
  }
  return ret_code;
}

SpenchRetCode DenoiserAudioHandler::DestroyAudioStreams(SpenchContext<float>* ctx) {
  if (ctx == nullptr) {
    return SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }

  if (ctx->input_sound_stream_ptr) {
    ctx->input_sound_stream_ptr.reset();
  }
  if (ctx->output_sound_stream_ptr) {
    ctx->output_sound_stream_ptr.reset();
  }
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode DenoiserAudioHandler::InferFrame(std::unique_ptr<spench::SpenchTask>& task_ptr) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;

  if (task_ptr->m_data.context == nullptr) {
    ret_code = SpenchRetCode::ERR_INFERENCE_PROBLEM;
  }

  if (ret_code == SpenchRetCode::SUCCESS) {
    try {
      m_feature_extractor_runner_ptr->Run(task_ptr.get());
    } catch(const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = SpenchRetCode::ERR_MEL_SPEC_CALC_PROBLEM;
    }
  }

  if (ret_code == SpenchRetCode::SUCCESS) {
    try {
      m_inference_runner_ptr->Infer(task_ptr.get());
    } catch(const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = SpenchRetCode::ERR_INFERENCE_PROBLEM;
    }
  }

  if (ret_code == SpenchRetCode::SUCCESS) {
    try {
      m_feature_extractor_runner_ptr->Run(task_ptr.get());
      if (task_ptr->m_stage != PipelineStage::ENHANCED_CHUNK) {
        ret_code = SpenchRetCode::CONTEXT_ACCUMULATION;
      }
    } catch(const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = SpenchRetCode::ERR_MASK_CALC_PROBLEM;
    }
  }
  return ret_code;
}

void DenoiserAudioHandler::FillInDataForProcessing(std::unique_ptr<spench::SpenchTask>& task_ptr,
                                               std::vector<float>& input ) {
  task_ptr->m_data.waveform = task_ptr->m_data.context->prev_input_chunk;
  task_ptr->m_data.waveform.insert(task_ptr->m_data.waveform.end(),
                                     input.begin(),
                                     input.end());
  task_ptr->m_data.context->prev_input_chunk = input;
}

SpenchRetCode DenoiserAudioHandler::ProcessFrame(struct spench::SpenchContext<float>* ctx,
                                    const std::vector<float>& input_audio_frame,
                                    std::vector<float>& enhanced_audio_frame) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  std::unique_ptr<spench::SpenchTask> task_ptr(new spench::SpenchTask());
  task_ptr->m_data.context = ctx;

  if (input_audio_frame.empty()) {
    enhanced_audio_frame.clear();
    return ret_code;
  }

  std::vector<float> resampled_input_result;
  try {
    resampled_input_result =
          task_ptr->m_data.context->input_sound_stream_ptr->ReadChunk(input_audio_frame);
    if (resampled_input_result.size() == 0) {
      spdlog::debug("Buffer accumulation for input resampling");
      ret_code = SpenchRetCode::RESAMPLING_INPUT_BUFFER_ACCUMULATION;
    }
  } catch(const std::exception& e) {
    spdlog::error("{}", e.what());
    ret_code = SpenchRetCode::ERR_INPUT_AUDIO_RESAMPLE_PROBLEM;
  }

  if (ret_code == SpenchRetCode::SUCCESS) {
    if (task_ptr->m_data.context->prev_input_chunk.size() == 0) {
      task_ptr->m_data.context->prev_input_chunk = resampled_input_result;

      tools::PrepareFirstInputFrame(resampled_input_result,
                                     task_ptr->m_data.waveform);
    } else {
      FillInDataForProcessing(task_ptr, resampled_input_result);
    }
  }

  task_ptr->m_stage = PipelineStage::ORIG_CHUNK;

  if (ret_code == SpenchRetCode::SUCCESS) {
    if (resampled_input_result.size() !=
                      m_feature_extractor_runner_ptr->GetFrameHopSizeSamples()) {
      ret_code = SpenchRetCode::ERR_INVALID_FRAME_SIZE;
      spdlog::error("Frame with invalid size {} provided", resampled_input_result.size());
    } else {
      ret_code = InferFrame(task_ptr);
    }
  }

  if (ret_code >= SpenchRetCode::SUCCESS) {
    try {
      auto result =
            task_ptr->m_data.context->output_sound_stream_ptr->ReadChunk(task_ptr->m_data.enhanced_wav);

      if (result.size() != 0) {
        enhanced_audio_frame.resize(0);
        std::copy(result.begin(), result.end(), back_inserter(enhanced_audio_frame));
      } else {
        spdlog::debug("Buffer accumulation for output resampling");
        ret_code = SpenchRetCode::RESAMPLING_OUTPUT_BUFFER_ACCUMULATION;
      }
    } catch(const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = SpenchRetCode::ERR_INPUT_AUDIO_RESAMPLE_PROBLEM;
    }
  }

  return ret_code;
}

SpenchRetCode DenoiserAudioHandler::FinalizeInference(struct spench::SpenchContext<float>* ctx,
                                                  std::vector<float>& enhanced_audio_frames) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;

  const std::vector<float>
              zero_input_audio_frame(ctx->input_sound_stream_ptr->m_in.FrameSizeSamples(), 0.0);
  uint32_t number_of_zero_contexts = m_inference_runner_ptr->GetModelFutureContextLen() + 1;
  // flush model context
  for (uint32_t i = 0; i < number_of_zero_contexts; ++i) {
    std::vector<float> output_audio;
    ret_code = ProcessFrame(ctx, zero_input_audio_frame, output_audio);
    if (ret_code != SpenchRetCode::SUCCESS && ret_code != SpenchRetCode::CONTEXT_ACCUMULATION) {
      break;
    }
    enhanced_audio_frames.insert(enhanced_audio_frames.end(),
                                output_audio.begin(),
                                output_audio.end());
  }
  // end flush model context

  std::vector<float> last_audio_frames_for_upsample;
  // flush input resampler
  {
    std::vector<float> last_input_resampled_data =
                              ctx->input_sound_stream_ptr->FinalizeStream();

    // we left last number_of_zero_contexts chunks in last_input_resampled_data as it got from zeroed vector
    for (uint32_t i = 0; last_input_resampled_data.size() >
                        (i + number_of_zero_contexts) *
                                          m_feature_extractor_runner_ptr->GetFrameHopSizeSamples(); ++i) {
      std::unique_ptr<spench::SpenchTask> task_ptr(new spench::SpenchTask());
      task_ptr->m_data.context = ctx;
      std::vector<float>
                  input_audio_frame(last_input_resampled_data.begin() +
                                    m_feature_extractor_runner_ptr->GetFrameHopSizeSamples() * i,
                                    last_input_resampled_data.begin() +
                                    m_feature_extractor_runner_ptr->GetFrameHopSizeSamples() * (i + 1));

      FillInDataForProcessing(task_ptr, input_audio_frame);
      task_ptr->m_stage = PipelineStage::ORIG_CHUNK;
      ret_code = InferFrame(task_ptr);

      if (task_ptr->m_stage != PipelineStage::ENHANCED_CHUNK ||
          (ret_code != SpenchRetCode::SUCCESS && ret_code != SpenchRetCode::CONTEXT_ACCUMULATION)) {
        spdlog::warn("Unexpected case happend");
        break;
      }

      last_audio_frames_for_upsample.insert(last_audio_frames_for_upsample.end(),
                                            task_ptr->m_data.enhanced_wav.begin(),
                                            task_ptr->m_data.enhanced_wav.end());
    }

    std::vector<float> output_resampled_data =
                              ctx->output_sound_stream_ptr->ReadChunk(last_audio_frames_for_upsample);
    enhanced_audio_frames.insert(enhanced_audio_frames.end(),
                                output_resampled_data.begin(),
                                output_resampled_data.end());
  }
  // end flush input resampler

  // flush output resampler
  {
    std::vector<float> flushed_upsampled_data =
                              ctx->output_sound_stream_ptr->FinalizeStream();
    enhanced_audio_frames.insert(enhanced_audio_frames.end(),
                                  flushed_upsampled_data.begin(),
                                  flushed_upsampled_data.end());
  }
  // end flush output resampler
  return ret_code;
}

StatInfo DenoiserAudioHandler::GetStatInfo(const struct spench::SpenchContext<float>* ctx) {
  if (ctx != nullptr) {
      return ctx->stat;
  } else {
    return StatInfo();
  }
}
}  // namespace spench
