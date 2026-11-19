#include "enhance/c_api.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#include "include/enhance.h"

namespace ctools {

template <typename Function>
GigaEdgeEnhanceRetCode guard(GigaEdgeEnhanceRetCode exception_code, Function&& function) noexcept {
  try {
    return std::forward<Function>(function)();
  } catch (const std::bad_alloc&) {
    return GigaEdgeEnhanceRetCode::ERR_NOT_ENOUGH_MEMORY;
  } catch (...) {
    return exception_code;
  }
}

template <typename Function>
void guard_void(Function&& function) noexcept {
  try {
    std::forward<Function>(function)();
  } catch (...) {
  }
}

bool initialize_output(uint8_t** output, uint64_t* output_size) {
  if (output == nullptr || output_size == nullptr) {
    return false;
  }
  *output = nullptr;
  *output_size = 0;
  return true;
}

template <typename T>
GigaEdgeEnhanceRetCode bytes_to_vector(const uint8_t* input, uint64_t input_size, std::vector<T>& output) {
  if (input == nullptr) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size % sizeof(T) != 0) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size > std::numeric_limits<size_t>::max()) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  const uint64_t sample_count = input_size / sizeof(T);
  if (sample_count > std::numeric_limits<size_t>::max()) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  output.resize(static_cast<size_t>(sample_count));
  if (input_size != 0) {
    std::memcpy(output.data(), input, static_cast<size_t>(input_size));
  }
  return GigaEdgeEnhanceRetCode::SUCCESS;
}

template <typename T>
GigaEdgeEnhanceRetCode allocate_and_copy(const std::vector<T>& input, uint8_t** output, uint64_t* output_size) {
  if (!initialize_output(output, output_size)) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input.empty()) {
    return GigaEdgeEnhanceRetCode::SUCCESS;
  }
  if (input.size() > std::numeric_limits<uint64_t>::max() / sizeof(T)) {
    return GigaEdgeEnhanceRetCode::ERR_ALLOCATION_PROBLEM;
  }

  const uint64_t bytes = static_cast<uint64_t>(input.size()) * sizeof(T);
  if (bytes > std::numeric_limits<size_t>::max()) {
    return GigaEdgeEnhanceRetCode::ERR_ALLOCATION_PROBLEM;
  }
  *output = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(bytes)));
  if (*output == nullptr) {
    return GigaEdgeEnhanceRetCode::ERR_NOT_ENOUGH_MEMORY;
  }
  std::memcpy(*output, input.data(), static_cast<size_t>(bytes));
  *output_size = bytes;
  return GigaEdgeEnhanceRetCode::SUCCESS;
}

template <typename T, typename Function>
GigaEdgeEnhanceRetCode process_bytes(const uint8_t* input, uint64_t input_size, uint8_t** output, uint64_t* output_size,
                                     Function&& function) {
  if (!initialize_output(output, output_size)) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> samples;
  const GigaEdgeEnhanceRetCode decode_code = bytes_to_vector(input, input_size, samples);
  if (decode_code != GigaEdgeEnhanceRetCode::SUCCESS) {
    return decode_code;
  }

  std::vector<T> result;
  const GigaEdgeEnhanceRetCode code = std::forward<Function>(function)(samples, result);
  if (code != GigaEdgeEnhanceRetCode::SUCCESS && code != GigaEdgeEnhanceRetCode::CONTEXT_ACCUMULATION) {
    return code;
  }
  const GigaEdgeEnhanceRetCode copy_code = allocate_and_copy(result, output, output_size);
  return copy_code == GigaEdgeEnhanceRetCode::SUCCESS ? code : copy_code;
}

template <typename T, typename Function>
GigaEdgeEnhanceRetCode final_bytes(uint8_t** output, uint64_t* output_size, Function&& function) {
  if (!initialize_output(output, output_size)) {
    return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> result;
  const GigaEdgeEnhanceRetCode code = std::forward<Function>(function)(result);
  if (code != GigaEdgeEnhanceRetCode::SUCCESS) {
    return code;
  }
  return allocate_and_copy(result, output, output_size);
}

}  // namespace ctools

extern "C" {

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseInit(const char* config_file, GigaEdgeEnhanceDenoise* model) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new gigaedge::enhance::Denoiser(config_file));
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

void GigaEdgeEnhanceDenoiseFree(GigaEdgeEnhanceDenoise* model) {
  ctools::guard_void([&] {
    if (model != nullptr) {
      delete static_cast<gigaedge::enhance::Denoiser*>(*model);
      *model = nullptr;
    }
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseCreateContext(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext* ctx,
                                                           uint32_t in_audio_sr, uint32_t out_audio_sr) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (ctx == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = nullptr;
    if (model == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = static_cast<gigaedge::enhance::Denoiser*>(model)->CreateContext(in_audio_sr, out_audio_sr);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

void GigaEdgeEnhanceDenoiseDestroyContext(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext* ctx) {
  ctools::guard_void([&] {
    if (model != nullptr && ctx != nullptr && *ctx != nullptr) {
      static_cast<gigaedge::enhance::Denoiser*>(model)->DestroyContext(
          reinterpret_cast<gigaedge::enhance::Context<float>**>(ctx));
    }
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetContextId(GigaEdgeEnhanceDenoise model,
                                                          const GigaEdgeEnhanceContext ctx, uint64_t* id) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (id != nullptr) *id = 0;
    if (model == nullptr || ctx == nullptr || id == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *id = static_cast<gigaedge::enhance::Denoiser*>(model)->GetContextId(
        static_cast<const gigaedge::enhance::Context<float>*>(ctx));
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseSetContextId(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext ctx,
                                                          uint64_t id) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    static_cast<gigaedge::enhance::Denoiser*>(model)->SetContextId(static_cast<gigaedge::enhance::Context<float>*>(ctx),
                                                                   id);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetStreamStat(GigaEdgeEnhanceDenoise model,
                                                           const GigaEdgeEnhanceContext ctx,
                                                           StreamStatStruct* stream_stat) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (stream_stat != nullptr) *stream_stat = {};
    if (model == nullptr || ctx == nullptr || stream_stat == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *stream_stat = static_cast<gigaedge::enhance::Denoiser*>(model)->GetStreamStat(
        static_cast<const gigaedge::enhance::Context<float>*>(ctx));
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetEngineThreads(const GigaEdgeEnhanceDenoise model,
                                                              uint64_t* engine_threads) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (engine_threads != nullptr) *engine_threads = 0;
    if (model == nullptr || engine_threads == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *engine_threads = static_cast<gigaedge::enhance::Denoiser*>(model)->GetEngineThreads();
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetInputFrameSize(const GigaEdgeEnhanceDenoise model,
                                                               uint64_t* input_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (input_size != nullptr) *input_size = 0;
    if (model == nullptr || input_size == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *input_size = static_cast<gigaedge::enhance::Denoiser*>(model)->GetModelInputFrameSize();
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetSampleRate(const GigaEdgeEnhanceDenoise model, uint64_t* sample_rate) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (sample_rate != nullptr) *sample_rate = 0;
    if (model == nullptr || sample_rate == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *sample_rate = static_cast<gigaedge::enhance::Denoiser*>(model)->GetModelSampleRate();
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseProcessChunkFp(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext ctx,
                                                            const uint8_t* input, uint64_t input_size, uint8_t** output,
                                                            uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<gigaedge::enhance::Denoiser*>(model)->ProcessChunk(
          samples, result, static_cast<gigaedge::enhance::Context<float>*>(ctx));
    });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseProcessChunkInt(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext ctx,
                                                             const uint8_t* input, uint64_t input_size,
                                                             uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<int16_t>(input, input_size, output, output_size,
                                          [&](const auto& samples, auto& result) {
                                            return static_cast<gigaedge::enhance::Denoiser*>(model)->ProcessChunk(
                                                samples, result, static_cast<gigaedge::enhance::Context<float>*>(ctx));
                                          });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetFinalChunkFp(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext ctx,
                                                             uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::enhance::Denoiser*>(model)->GetFinalChunk(
          result, static_cast<gigaedge::enhance::Context<float>*>(ctx));
    });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceDenoiseGetFinalChunkInt(GigaEdgeEnhanceDenoise model, GigaEdgeEnhanceContext ctx,
                                                              uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::enhance::Denoiser*>(model)->GetFinalChunk(
          result, static_cast<gigaedge::enhance::Context<float>*>(ctx));
    });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessInit(const char* config_file, GigaEdgeEnhanceAudioProcess* model) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new gigaedge::enhance::AudioProcessor(config_file));
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

void GigaEdgeEnhanceAudioProcessFree(GigaEdgeEnhanceAudioProcess model) {
  ctools::guard_void([&] { delete static_cast<gigaedge::enhance::AudioProcessor*>(model); });
}

#define GEE_AUDIO_PROCESS_ADD(function_name, method_name)                               \
  GigaEdgeEnhanceRetCode function_name(GigaEdgeEnhanceAudioProcess model) {             \
    return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] { \
      if (model == nullptr) {                                                           \
        return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;                 \
      }                                                                                 \
      static_cast<gigaedge::enhance::AudioProcessor*>(model)->method_name();            \
      return GigaEdgeEnhanceRetCode::SUCCESS;                                           \
    });                                                                                 \
  }

GEE_AUDIO_PROCESS_ADD(GigaEdgeEnhanceAudioProcessAddAgc, AddAGC)
GEE_AUDIO_PROCESS_ADD(GigaEdgeEnhanceAudioProcessAddRc, AddRC)
GEE_AUDIO_PROCESS_ADD(GigaEdgeEnhanceAudioProcessAddLc, AddLC)
GEE_AUDIO_PROCESS_ADD(GigaEdgeEnhanceAudioProcessAddNg, AddNG)
GEE_AUDIO_PROCESS_ADD(GigaEdgeEnhanceAudioProcessAddLim, AddLim)
GEE_AUDIO_PROCESS_ADD(GigaEdgeEnhanceAudioProcessAddDrc, AddDRC)

#undef GEE_AUDIO_PROCESS_ADD

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessAddAgcWithParam(GigaEdgeEnhanceAudioProcess model,
                                                                  uint32_t sample_rate, float target_power_level,
                                                                  float max_power_gain) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::enhance::AudioProcessor*>(model)->AddAGC(sample_rate, target_power_level, max_power_gain,
                                                                   1e-2F, 1e-2F);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessAddRcWithParam(GigaEdgeEnhanceAudioProcess model,
                                                                 uint32_t sample_rate, float target_level_db,
                                                                 float max_gain_db, float attack_time,
                                                                 float release_time, float hold_time) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::enhance::AudioProcessor*>(model)->AddRC(sample_rate, target_level_db, max_gain_db,
                                                                  attack_time, release_time, hold_time);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessAddLcWithParam(GigaEdgeEnhanceAudioProcess model,
                                                                 uint32_t sample_rate, float target_level_db,
                                                                 float bypass_time, float max_gain_db, uint32_t use_vad,
                                                                 uint32_t post_processing) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::enhance::AudioProcessor*>(model)->AddLC(sample_rate, target_level_db, bypass_time,
                                                                  max_gain_db, use_vad != 0, post_processing != 0);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessAddNgWithParam(GigaEdgeEnhanceAudioProcess model,
                                                                 uint32_t sample_rate, float threshold_db,
                                                                 float attack_time, float release_time,
                                                                 float hold_time) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::enhance::AudioProcessor*>(model)->AddNG(sample_rate, threshold_db, attack_time, release_time,
                                                                  hold_time);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessAddLimWithParam(GigaEdgeEnhanceAudioProcess model,
                                                                  uint32_t sample_rate, float threshold_db,
                                                                  float knee_width, float attack_time,
                                                                  float release_time) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::enhance::AudioProcessor*>(model)->AddLim(sample_rate, threshold_db, knee_width, attack_time,
                                                                   release_time);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessAddDrcWithParam(GigaEdgeEnhanceAudioProcess model,
                                                                  uint32_t sample_rate, float threshold_db,
                                                                  uint32_t ratio, float knee_width, float attack_time,
                                                                  float release_time) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::enhance::AudioProcessor*>(model)->AddDRC(sample_rate, threshold_db, ratio, knee_width,
                                                                   attack_time, release_time);
    return GigaEdgeEnhanceRetCode::SUCCESS;
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessChunkFp(GigaEdgeEnhanceAudioProcess model, const uint8_t* input,
                                                          uint64_t input_size, uint8_t** output,
                                                          uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<gigaedge::enhance::AudioProcessor*>(model)->ProcessChunk(samples, result, nullptr);
    });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessChunkInt(GigaEdgeEnhanceAudioProcess model, const uint8_t* input,
                                                           uint64_t input_size, uint8_t** output,
                                                           uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<int16_t>(
        input, input_size, output, output_size, [&](const auto& samples, auto& result) {
          return static_cast<gigaedge::enhance::AudioProcessor*>(model)->ProcessChunk(samples, result, nullptr);
        });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessGetFinalChunkFp(GigaEdgeEnhanceAudioProcess model, uint8_t** output,
                                                                  uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::enhance::AudioProcessor*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

GigaEdgeEnhanceRetCode GigaEdgeEnhanceAudioProcessGetFinalChunkInt(GigaEdgeEnhanceAudioProcess model, uint8_t** output,
                                                                   uint64_t* output_size) {
  return ctools::guard(GigaEdgeEnhanceRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::enhance::AudioProcessor*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

}  // extern "C"
