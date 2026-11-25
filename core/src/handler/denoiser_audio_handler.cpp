#include "src/handler/denoiser_audio_handler.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "src/denoiser_model_adviser.h"
#include "src/logger.h"

namespace giga_edge::enhance {

DenoiserAudioHandler::DenoiserAudioHandler(std::shared_ptr<DenoiserConfig> config)
    : NeuralNetworkAudioHandler(std::move(config)), adviser_(nullptr) {
  // Construction guarantees DenoiserConfig; avoid private final-type RTTI across shared libraries.
  float frame_shift_ms = static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->frame_stride_ms();
  static constexpr float window_duration_ms = 1000 * 5;
  adviser_ = std::make_unique<DenoiserModelAdviser<float>>(window_duration_ms, frame_shift_ms);
}

DenoiserAudioHandler::~DenoiserAudioHandler() = default;

Context<float>* DenoiserAudioHandler::create_context(uint32_t input_sample_rate, uint32_t output_sample_rate) {
  Context<float>* ctx = nullptr;
  if (inference_runner_ptr_->get_model_type() == InferenceRunner::ModelType::denoiser_discr_web_model) {
    ctx = dynamic_cast<Context<float>*>(new DenoiseContextDiscriminativeHq<float>());
  } else if (inference_runner_ptr_->get_model_type() == InferenceRunner::ModelType::denoiser_hybrid_web_model) {
    ctx = new DenoiseContextHybrid<float>();
  } else if (inference_runner_ptr_->get_model_type() == InferenceRunner::ModelType::denoiser_discr_hq_model) {
    ctx = dynamic_cast<Context<float>*>(new DenoiseContextDiscriminativeNb<float>());
  } else {
    throw std::invalid_argument("Unsupported denoiser model type");
  }

  ctx->snr_adviser = std::make_unique<DenoiserModelAdviser<float>>(*adviser_);
  ctx->snr_adviser->reset();

  if (input_sample_rate == 0) {
    input_sample_rate = static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->sampling_freq();
  }
  if (output_sample_rate == 0) {
    output_sample_rate = static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->sampling_freq();
  }

  GigaEdgeEnhanceRetCode ret_code = init_audio_streams(ctx, input_sample_rate, output_sample_rate);
  if (ret_code != GigaEdgeEnhanceRetCode::SUCCESS) {
    spdlog::error("Due to errors a context could not be created!");
    throw std::runtime_error("Due to errors a context could not be created!");
  }
  spdlog::info("New denoising context created! Input sr {} Output sr {}", input_sample_rate, output_sample_rate);
  return ctx;
}

GigaEdgeEnhanceRetCode DenoiserAudioHandler::init_audio_streams(Context<float>* ctx, uint32_t input_sample_rate,
                                                                uint32_t output_sample_rate) {
  GigaEdgeEnhanceRetCode ret_code = GigaEdgeEnhanceRetCode::SUCCESS;
  if (ctx == nullptr) {
    return GigaEdgeEnhanceRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }

  if (ret_code == GigaEdgeEnhanceRetCode::SUCCESS) {
    try {
      if (!ctx->input_sound_stream_ptr) {
        ctx->input_sound_stream_ptr = util::sound::build_sound_stream(
            input_sample_rate, static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->sampling_freq(),
            static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->frame_stride_ms(),
            util::sound::TemplateType::input);
      }
      if (!ctx->output_sound_stream_ptr) {
        ctx->output_sound_stream_ptr = util::sound::build_sound_stream(
            static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->sampling_freq(), output_sample_rate,
            static_cast<giga_edge::enhance::DenoiserConfig*>(config_.get())->frame_stride_ms(),
            util::sound::TemplateType::output);
      }
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
  }
  return ret_code;
}

GigaEdgeEnhanceRetCode DenoiserAudioHandler::destroy_audio_streams(Context<float>* ctx) {
  if (ctx == nullptr) {
    return GigaEdgeEnhanceRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }

  if (ctx->input_sound_stream_ptr) {
    ctx->input_sound_stream_ptr.reset();
  }
  if (ctx->output_sound_stream_ptr) {
    ctx->output_sound_stream_ptr.reset();
  }
  return GigaEdgeEnhanceRetCode::SUCCESS;
}

GigaEdgeEnhanceRetCode DenoiserAudioHandler::infer_frame(std::unique_ptr<giga_edge::enhance::Task>& task_ptr) {
  GigaEdgeEnhanceRetCode ret_code = GigaEdgeEnhanceRetCode::SUCCESS;

  if (task_ptr->data.context == nullptr) {
    ret_code = GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM;
  }

  if (ret_code == GigaEdgeEnhanceRetCode::SUCCESS) {
    try {
      feature_extractor_runner_ptr_->run(task_ptr.get());
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = GigaEdgeEnhanceRetCode::ERR_MEL_SPEC_CALC_PROBLEM;
    }
  }

  if (ret_code == GigaEdgeEnhanceRetCode::SUCCESS) {
    try {
      inference_runner_ptr_->infer(task_ptr.get());
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM;
    }
  }

  if (ret_code == GigaEdgeEnhanceRetCode::SUCCESS) {
    try {
      feature_extractor_runner_ptr_->run(task_ptr.get());
      if (task_ptr->state.is(ChunkOutcome::warming_up)) {
        ret_code = GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION;
      } else if (!task_ptr->data.input_spectrogram.empty() && !task_ptr->data.context->prev_spectrogram.empty()) {
        const float snr = task_ptr->data.context->snr_adviser->process_frame(task_ptr->data.input_spectrogram.back(),
                                                                             task_ptr->data.context->prev_spectrogram,
                                                                             task_ptr->data.context->metrics);
        if (std::isfinite(snr)) {
          task_ptr->data.context->stat.snr = snr;
        }
      }
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = GigaEdgeEnhanceRetCode::ERR_MASK_CALC_PROBLEM;
    }
  }
  return ret_code;
}

void DenoiserAudioHandler::fill_in_data_for_processing(std::unique_ptr<giga_edge::enhance::Task>& task_ptr,
                                                       std::vector<float>& input) {
  auto& ctx = *task_ptr->data.context;
  auto& waveform = task_ptr->data.waveform;
  uint32_t chunk_size = get_input_frame_size();
  // Build waveform = previous tail + current input.
  waveform.clear();
  waveform.reserve(ctx.prev_input_chunk.size() + input.size());
  waveform.insert(waveform.end(), ctx.prev_input_chunk.begin(), ctx.prev_input_chunk.end());
  waveform.insert(waveform.end(), input.begin(), input.end());

  // Save the last chunk_size samples from input for the next call.
  const std::size_t keep = std::min<std::size_t>(chunk_size, input.size());
  ctx.prev_input_chunk.assign(input.end() - keep, input.end());
}

GigaEdgeEnhanceRetCode DenoiserAudioHandler::process_frame(struct giga_edge::enhance::Context<float>* ctx,
                                                           const std::vector<float>& input_audio_frame,
                                                           std::vector<float>& enhanced_audio_frame) {
  return process_frame_internal(ctx, input_audio_frame, enhanced_audio_frame, true);
}

GigaEdgeEnhanceRetCode DenoiserAudioHandler::process_frame_internal(Context<float>* ctx,
                                                                    const std::vector<float>& input_audio_frame,
                                                                    std::vector<float>& enhanced_audio_frame,
                                                                    bool allow_zero_shortcut) {
  enhanced_audio_frame.clear();
  GigaEdgeEnhanceRetCode ret_code = GigaEdgeEnhanceRetCode::SUCCESS;
  std::unique_ptr<giga_edge::enhance::Task> task_ptr(new giga_edge::enhance::Task());
  task_ptr->data.context = ctx;
  static constexpr double delay_warn_ms = 300.0;
  static constexpr double warn_interval_ms = 30000.0;  // 30 seconds
  if (input_audio_frame.empty()) {
    return ret_code;
  }
  const bool skip_inference = allow_zero_shortcut && skip_zero_chunk_ && is_zeroed_vector(input_audio_frame);

  std::vector<float> resampled_input_result;
  try {
    resampled_input_result = task_ptr->data.context->input_sound_stream_ptr->read_chunk(input_audio_frame);
    // --- NEW: warn on input-stream accumulated delay ---
    double in_delay_ms = 0.0;
    if (task_ptr->data.context->input_sound_stream_ptr->should_warn_delay(delay_warn_ms, warn_interval_ms,
                                                                          &in_delay_ms)) {
      spdlog::warn("Input sound_stream accumulated delay {:.2f} ms exceeds {:.0f} ms", in_delay_ms, delay_warn_ms);
    }
    if (resampled_input_result.size() == 0) {
      spdlog::debug("Buffer accumulation for input resampling");
      ret_code = GigaEdgeEnhanceRetCode::RESAMPLING_INPUT_BUFFER_ACCUMULATION;
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    ret_code = GigaEdgeEnhanceRetCode::ERR_INPUT_AUDIO_RESAMPLE_PROBLEM;
  }

  if (ret_code == GigaEdgeEnhanceRetCode::SUCCESS) {
    const auto minimal_chunk_size = get_input_frame_size();
    const auto hop = feature_extractor_runner_ptr_->get_frame_hop_size_samples();

    if (resampled_input_result.size() < minimal_chunk_size) {
      spdlog::error("Frame too short: got {}, need at least {}", resampled_input_result.size(), minimal_chunk_size);
      return GigaEdgeEnhanceRetCode::ERR_INVALID_FRAME_SIZE;
    }

    if (resampled_input_result.size() % hop != 0) {
      spdlog::error("Frame with invalid size {} provided, need multiple of {}", resampled_input_result.size(),
                    get_input_frame_size());
      return GigaEdgeEnhanceRetCode::ERR_INVALID_FRAME_SIZE;
    }

    if (resampled_input_result.size() / hop >= max_number_of_time_frames_for_single_input) {
      spdlog::error("Maximum frame length exceeded {}. Maximum is {} time frames (~32 min) of audio",
                    resampled_input_result.size(), max_number_of_time_frames_for_single_input);
      return GigaEdgeEnhanceRetCode::ERR_INVALID_FRAME_SIZE;
    }

    if (task_ptr->data.context->prev_input_chunk.empty()) {
      task_ptr->data.context->prev_input_chunk.insert(task_ptr->data.context->prev_input_chunk.end(),
                                                      resampled_input_result.end() - minimal_chunk_size,
                                                      resampled_input_result.end());

      tools::prepare_first_input_frame(resampled_input_result, task_ptr->data.waveform, minimal_chunk_size);
    } else {
      fill_in_data_for_processing(task_ptr, resampled_input_result);
    }
  }

  if (ret_code == GigaEdgeEnhanceRetCode::SUCCESS) {
    if (skip_inference) {
      task_ptr->data.enhanced_wav = resampled_input_result;
    } else {
      ret_code = infer_frame(task_ptr);
    }
  }

  if (ret_code >= GigaEdgeEnhanceRetCode::SUCCESS) {
    try {
      auto result = task_ptr->data.context->output_sound_stream_ptr->read_chunk(task_ptr->data.enhanced_wav);
      // --- NEW: warn on output-stream accumulated delay ---
      double out_delay_ms = 0.0;
      if (task_ptr->data.context->output_sound_stream_ptr->should_warn_delay(delay_warn_ms, warn_interval_ms,
                                                                             &out_delay_ms)) {
        spdlog::warn("Output sound_stream accumulated delay {:.2f} ms exceeds {:.0f} ms", out_delay_ms, delay_warn_ms);
      }
      if (result.size() != 0) {
        enhanced_audio_frame.resize(0);
        std::copy(result.begin(), result.end(), back_inserter(enhanced_audio_frame));
      } else {
        spdlog::debug("Buffer accumulation for output resampling");
        ret_code = GigaEdgeEnhanceRetCode::RESAMPLING_OUTPUT_BUFFER_ACCUMULATION;
      }
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      ret_code = GigaEdgeEnhanceRetCode::ERR_INPUT_AUDIO_RESAMPLE_PROBLEM;
    }
  }

  return ret_code;
}

GigaEdgeEnhanceRetCode DenoiserAudioHandler::finalize_inference(struct giga_edge::enhance::Context<float>* ctx,
                                                                std::vector<float>& enhanced_audio_frames) {
  GigaEdgeEnhanceRetCode ret_code = GigaEdgeEnhanceRetCode::SUCCESS;

  const std::vector<float> zero_input_audio_frame(ctx->input_sound_stream_ptr->input.frame_size_samples(), 0.0);
  uint32_t number_of_zero_contexts = inference_runner_ptr_->get_model_future_context_len() + 1;
  // flush model context
  for (uint32_t i = 0; i < number_of_zero_contexts; ++i) {
    std::vector<float> output_audio;
    ret_code = process_frame_internal(ctx, zero_input_audio_frame, output_audio, false);
    if (ret_code != GigaEdgeEnhanceRetCode::SUCCESS && ret_code != GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION) {
      break;
    }
    enhanced_audio_frames.insert(enhanced_audio_frames.end(), output_audio.begin(), output_audio.end());
  }
  // end flush model context

  std::vector<float> last_audio_frames_for_upsample;
  // flush input resampler
  {
    std::vector<float> last_input_resampled_data = ctx->input_sound_stream_ptr->finalize_stream();

    // we left last number_of_zero_contexts chunks in last_input_resampled_data as it got from zeroed vector
    for (uint32_t i = 0; last_input_resampled_data.size() >
                         (i + number_of_zero_contexts) * feature_extractor_runner_ptr_->get_frame_hop_size_samples();
         ++i) {
      std::unique_ptr<giga_edge::enhance::Task> task_ptr(new giga_edge::enhance::Task());
      task_ptr->data.context = ctx;
      std::vector<float> input_audio_frame(
          last_input_resampled_data.begin() + feature_extractor_runner_ptr_->get_frame_hop_size_samples() * i,
          last_input_resampled_data.begin() + feature_extractor_runner_ptr_->get_frame_hop_size_samples() * (i + 1));

      fill_in_data_for_processing(task_ptr, input_audio_frame);
      ret_code = infer_frame(task_ptr);

      if (!task_ptr->state.is(PipelineStage::enhanced_ready) ||
          (ret_code != GigaEdgeEnhanceRetCode::SUCCESS && ret_code != GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION)) {
        spdlog::warn("Unexpected case happend");
        break;
      }

      last_audio_frames_for_upsample.insert(last_audio_frames_for_upsample.end(), task_ptr->data.enhanced_wav.begin(),
                                            task_ptr->data.enhanced_wav.end());
    }

    std::vector<float> output_resampled_data = ctx->output_sound_stream_ptr->read_chunk(last_audio_frames_for_upsample);
    enhanced_audio_frames.insert(enhanced_audio_frames.end(), output_resampled_data.begin(),
                                 output_resampled_data.end());
  }
  // end flush input resampler

  // flush output resampler
  {
    std::vector<float> flushed_upsampled_data = ctx->output_sound_stream_ptr->finalize_stream();
    enhanced_audio_frames.insert(enhanced_audio_frames.end(), flushed_upsampled_data.begin(),
                                 flushed_upsampled_data.end());
  }
  // end flush output resampler
  return ret_code;
}

StatInfo DenoiserAudioHandler::get_stat_info(const struct giga_edge::enhance::Context<float>* ctx) {
  if (ctx != nullptr) {
    return ctx->stat;
  } else {
    return StatInfo();
  }
}

}  // namespace giga_edge::enhance
