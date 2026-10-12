#include "spench/c_api.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#include "include/spench.h"

namespace ctools {

template <typename Function>
GigaEdgeSpenchRetCode guard(GigaEdgeSpenchRetCode exception_code, Function&& function) noexcept {
  try {
    return std::forward<Function>(function)();
  } catch (const std::bad_alloc&) {
    return GigaEdgeSpenchRetCode::ERR_NOT_ENOUGH_MEMORY;
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
GigaEdgeSpenchRetCode bytes_to_vector(const uint8_t* input, uint64_t input_size, std::vector<T>& output) {
  if (input == nullptr) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size % sizeof(T) != 0) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size > std::numeric_limits<size_t>::max()) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  const uint64_t sample_count = input_size / sizeof(T);
  if (sample_count > std::numeric_limits<size_t>::max()) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  output.resize(static_cast<size_t>(sample_count));
  if (input_size != 0) {
    std::memcpy(output.data(), input, static_cast<size_t>(input_size));
  }
  return GigaEdgeSpenchRetCode::SUCCESS;
}

template <typename T>
GigaEdgeSpenchRetCode allocate_and_copy(const std::vector<T>& input, uint8_t** output, uint64_t* output_size) {
  if (!initialize_output(output, output_size)) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input.empty()) {
    return GigaEdgeSpenchRetCode::SUCCESS;
  }
  if (input.size() > std::numeric_limits<uint64_t>::max() / sizeof(T)) {
    return GigaEdgeSpenchRetCode::ERR_ALLOCATION_PROBLEM;
  }

  const uint64_t bytes = static_cast<uint64_t>(input.size()) * sizeof(T);
  if (bytes > std::numeric_limits<size_t>::max()) {
    return GigaEdgeSpenchRetCode::ERR_ALLOCATION_PROBLEM;
  }
  *output = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(bytes)));
  if (*output == nullptr) {
    return GigaEdgeSpenchRetCode::ERR_NOT_ENOUGH_MEMORY;
  }
  std::memcpy(*output, input.data(), static_cast<size_t>(bytes));
  *output_size = bytes;
  return GigaEdgeSpenchRetCode::SUCCESS;
}

template <typename T, typename Function>
GigaEdgeSpenchRetCode process_bytes(const uint8_t* input, uint64_t input_size, uint8_t** output, uint64_t* output_size,
                                    Function&& function) {
  if (!initialize_output(output, output_size)) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> samples;
  const GigaEdgeSpenchRetCode decode_code = bytes_to_vector(input, input_size, samples);
  if (decode_code != GigaEdgeSpenchRetCode::SUCCESS) {
    return decode_code;
  }

  std::vector<T> result;
  const GigaEdgeSpenchRetCode code = std::forward<Function>(function)(samples, result);
  if (code != GigaEdgeSpenchRetCode::SUCCESS && code != GigaEdgeSpenchRetCode::CONTEXT_ACCUMULATION) {
    return code;
  }
  const GigaEdgeSpenchRetCode copy_code = allocate_and_copy(result, output, output_size);
  return copy_code == GigaEdgeSpenchRetCode::SUCCESS ? code : copy_code;
}

template <typename T, typename Function>
GigaEdgeSpenchRetCode final_bytes(uint8_t** output, uint64_t* output_size, Function&& function) {
  if (!initialize_output(output, output_size)) {
    return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> result;
  const GigaEdgeSpenchRetCode code = std::forward<Function>(function)(result);
  if (code != GigaEdgeSpenchRetCode::SUCCESS) {
    return code;
  }
  return allocate_and_copy(result, output, output_size);
}

}  // namespace ctools

extern "C" {

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseInit(const char* config_file, GigaEdgeSpenchDenoise* model) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new gigaedge::spench::Denoiser(config_file));
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

void GigaEdgeSpenchDenoiseFree(GigaEdgeSpenchDenoise* model) {
  ctools::guard_void([&] {
    if (model != nullptr) {
      delete static_cast<gigaedge::spench::Denoiser*>(*model);
      *model = nullptr;
    }
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseCreateContext(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext* ctx,
                                                         uint32_t in_audio_sr, uint32_t out_audio_sr) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (ctx == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = nullptr;
    if (model == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = static_cast<gigaedge::spench::Denoiser*>(model)->CreateContext(in_audio_sr, out_audio_sr);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

void GigaEdgeSpenchDenoiseDestroyContext(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext* ctx) {
  ctools::guard_void([&] {
    if (model != nullptr && ctx != nullptr && *ctx != nullptr) {
      static_cast<gigaedge::spench::Denoiser*>(model)->DestroyContext(
          reinterpret_cast<gigaedge::spench::Context<float>**>(ctx));
    }
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetContextId(GigaEdgeSpenchDenoise model, const GigaEdgeSpenchContext ctx,
                                                        uint64_t* id) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (id != nullptr) *id = 0;
    if (model == nullptr || ctx == nullptr || id == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *id = static_cast<gigaedge::spench::Denoiser*>(model)->GetContextId(
        static_cast<const gigaedge::spench::Context<float>*>(ctx));
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseSetContextId(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext ctx,
                                                        uint64_t id) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    static_cast<gigaedge::spench::Denoiser*>(model)->SetContextId(static_cast<gigaedge::spench::Context<float>*>(ctx),
                                                                  id);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetStreamStat(GigaEdgeSpenchDenoise model, const GigaEdgeSpenchContext ctx,
                                                         StreamStatStruct* stream_stat) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (stream_stat != nullptr) *stream_stat = {};
    if (model == nullptr || ctx == nullptr || stream_stat == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *stream_stat = static_cast<gigaedge::spench::Denoiser*>(model)->GetStreamStat(
        static_cast<const gigaedge::spench::Context<float>*>(ctx));
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetEngineThreads(const GigaEdgeSpenchDenoise model,
                                                            uint64_t* engine_threads) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (engine_threads != nullptr) *engine_threads = 0;
    if (model == nullptr || engine_threads == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *engine_threads = static_cast<gigaedge::spench::Denoiser*>(model)->GetEngineThreads();
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetInputFrameSize(const GigaEdgeSpenchDenoise model, uint64_t* input_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (input_size != nullptr) *input_size = 0;
    if (model == nullptr || input_size == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *input_size = static_cast<gigaedge::spench::Denoiser*>(model)->GetModelInputFrameSize();
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetSampleRate(const GigaEdgeSpenchDenoise model, uint64_t* sample_rate) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (sample_rate != nullptr) *sample_rate = 0;
    if (model == nullptr || sample_rate == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *sample_rate = static_cast<gigaedge::spench::Denoiser*>(model)->GetModelSampleRate();
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseProcessChunkFp(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext ctx,
                                                          const uint8_t* input, uint64_t input_size, uint8_t** output,
                                                          uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<gigaedge::spench::Denoiser*>(model)->ProcessChunk(
          samples, result, static_cast<gigaedge::spench::Context<float>*>(ctx));
    });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseProcessChunkInt(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext ctx,
                                                           const uint8_t* input, uint64_t input_size, uint8_t** output,
                                                           uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<int16_t>(input, input_size, output, output_size,
                                          [&](const auto& samples, auto& result) {
                                            return static_cast<gigaedge::spench::Denoiser*>(model)->ProcessChunk(
                                                samples, result, static_cast<gigaedge::spench::Context<float>*>(ctx));
                                          });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetFinalChunkFp(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext ctx,
                                                           uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::spench::Denoiser*>(model)->GetFinalChunk(
          result, static_cast<gigaedge::spench::Context<float>*>(ctx));
    });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchDenoiseGetFinalChunkInt(GigaEdgeSpenchDenoise model, GigaEdgeSpenchContext ctx,
                                                            uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::spench::Denoiser*>(model)->GetFinalChunk(
          result, static_cast<gigaedge::spench::Context<float>*>(ctx));
    });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessInit(const char* config_file, GigaEdgeSpenchAudioProcess* model) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new gigaedge::spench::AudioProcessor(config_file));
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

void GigaEdgeSpenchAudioProcessFree(GigaEdgeSpenchAudioProcess model) {
  ctools::guard_void([&] { delete static_cast<gigaedge::spench::AudioProcessor*>(model); });
}

#define GES_AUDIO_PROCESS_ADD(function_name, method_name)                              \
  GigaEdgeSpenchRetCode function_name(GigaEdgeSpenchAudioProcess model) {              \
    return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] { \
      if (model == nullptr) {                                                          \
        return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;                 \
      }                                                                                \
      static_cast<gigaedge::spench::AudioProcessor*>(model)->method_name();            \
      return GigaEdgeSpenchRetCode::SUCCESS;                                           \
    });                                                                                \
  }

GES_AUDIO_PROCESS_ADD(GigaEdgeSpenchAudioProcessAddAgc, AddAGC)
GES_AUDIO_PROCESS_ADD(GigaEdgeSpenchAudioProcessAddRc, AddRC)
GES_AUDIO_PROCESS_ADD(GigaEdgeSpenchAudioProcessAddLc, AddLC)
GES_AUDIO_PROCESS_ADD(GigaEdgeSpenchAudioProcessAddNg, AddNG)
GES_AUDIO_PROCESS_ADD(GigaEdgeSpenchAudioProcessAddLim, AddLim)
GES_AUDIO_PROCESS_ADD(GigaEdgeSpenchAudioProcessAddDrc, AddDRC)

#undef GES_AUDIO_PROCESS_ADD

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessAddAgcWithParam(GigaEdgeSpenchAudioProcess model, uint32_t sample_rate,
                                                                float target_power_level, float max_power_gain) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::spench::AudioProcessor*>(model)->AddAGC(sample_rate, target_power_level, max_power_gain,
                                                                  1e-2F, 1e-2F);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessAddRcWithParam(GigaEdgeSpenchAudioProcess model, uint32_t sample_rate,
                                                               float target_level_db, float max_gain_db,
                                                               float attack_time, float release_time, float hold_time) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::spench::AudioProcessor*>(model)->AddRC(sample_rate, target_level_db, max_gain_db, attack_time,
                                                                 release_time, hold_time);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessAddLcWithParam(GigaEdgeSpenchAudioProcess model, uint32_t sample_rate,
                                                               float target_level_db, float bypass_time,
                                                               float max_gain_db, uint32_t use_vad,
                                                               uint32_t post_processing) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::spench::AudioProcessor*>(model)->AddLC(sample_rate, target_level_db, bypass_time, max_gain_db,
                                                                 use_vad != 0, post_processing != 0);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessAddNgWithParam(GigaEdgeSpenchAudioProcess model, uint32_t sample_rate,
                                                               float threshold_db, float attack_time,
                                                               float release_time, float hold_time) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::spench::AudioProcessor*>(model)->AddNG(sample_rate, threshold_db, attack_time, release_time,
                                                                 hold_time);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessAddLimWithParam(GigaEdgeSpenchAudioProcess model, uint32_t sample_rate,
                                                                float threshold_db, float knee_width, float attack_time,
                                                                float release_time) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::spench::AudioProcessor*>(model)->AddLim(sample_rate, threshold_db, knee_width, attack_time,
                                                                  release_time);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessAddDrcWithParam(GigaEdgeSpenchAudioProcess model, uint32_t sample_rate,
                                                                float threshold_db, uint32_t ratio, float knee_width,
                                                                float attack_time, float release_time) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::spench::AudioProcessor*>(model)->AddDRC(sample_rate, threshold_db, ratio, knee_width,
                                                                  attack_time, release_time);
    return GigaEdgeSpenchRetCode::SUCCESS;
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessChunkFp(GigaEdgeSpenchAudioProcess model, const uint8_t* input,
                                                        uint64_t input_size, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<gigaedge::spench::AudioProcessor*>(model)->ProcessChunk(samples, result, nullptr);
    });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessChunkInt(GigaEdgeSpenchAudioProcess model, const uint8_t* input,
                                                         uint64_t input_size, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<int16_t>(
        input, input_size, output, output_size, [&](const auto& samples, auto& result) {
          return static_cast<gigaedge::spench::AudioProcessor*>(model)->ProcessChunk(samples, result, nullptr);
        });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessGetFinalChunkFp(GigaEdgeSpenchAudioProcess model, uint8_t** output,
                                                                uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::spench::AudioProcessor*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

GigaEdgeSpenchRetCode GigaEdgeSpenchAudioProcessGetFinalChunkInt(GigaEdgeSpenchAudioProcess model, uint8_t** output,
                                                                 uint64_t* output_size) {
  return ctools::guard(GigaEdgeSpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::spench::AudioProcessor*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

}  // extern "C"
