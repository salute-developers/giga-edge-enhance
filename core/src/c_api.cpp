#include "gigadenoiser/c_api.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#include "include/gigadenoiser.h"

namespace ctools {

template <typename Function>
GigaDenoiserRetCode guard(GigaDenoiserRetCode exception_code, Function&& function) noexcept {
  try {
    return std::forward<Function>(function)();
  } catch (const std::bad_alloc&) {
    return GigaDenoiserRetCode::ERR_NOT_ENOUGH_MEMORY;
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
GigaDenoiserRetCode bytes_to_vector(const uint8_t* input, uint64_t input_size, std::vector<T>& output) {
  if (input == nullptr) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size % sizeof(T) != 0) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size > std::numeric_limits<size_t>::max()) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  const uint64_t sample_count = input_size / sizeof(T);
  if (sample_count > std::numeric_limits<size_t>::max()) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  output.resize(static_cast<size_t>(sample_count));
  if (input_size != 0) {
    std::memcpy(output.data(), input, static_cast<size_t>(input_size));
  }
  return GigaDenoiserRetCode::SUCCESS;
}

template <typename T>
GigaDenoiserRetCode allocate_and_copy(const std::vector<T>& input, uint8_t** output, uint64_t* output_size) {
  if (!initialize_output(output, output_size)) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input.empty()) {
    return GigaDenoiserRetCode::SUCCESS;
  }
  if (input.size() > std::numeric_limits<uint64_t>::max() / sizeof(T)) {
    return GigaDenoiserRetCode::ERR_ALLOCATION_PROBLEM;
  }

  const uint64_t bytes = static_cast<uint64_t>(input.size()) * sizeof(T);
  if (bytes > std::numeric_limits<size_t>::max()) {
    return GigaDenoiserRetCode::ERR_ALLOCATION_PROBLEM;
  }
  *output = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(bytes)));
  if (*output == nullptr) {
    return GigaDenoiserRetCode::ERR_NOT_ENOUGH_MEMORY;
  }
  std::memcpy(*output, input.data(), static_cast<size_t>(bytes));
  *output_size = bytes;
  return GigaDenoiserRetCode::SUCCESS;
}

template <typename T, typename Function>
GigaDenoiserRetCode process_bytes(const uint8_t* input, uint64_t input_size, uint8_t** output, uint64_t* output_size,
                                  Function&& function) {
  if (!initialize_output(output, output_size)) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> samples;
  const GigaDenoiserRetCode decode_code = bytes_to_vector(input, input_size, samples);
  if (decode_code != GigaDenoiserRetCode::SUCCESS) {
    return decode_code;
  }

  std::vector<T> result;
  const GigaDenoiserRetCode code = std::forward<Function>(function)(samples, result);
  if (code != GigaDenoiserRetCode::SUCCESS && code != GigaDenoiserRetCode::CONTEXT_ACCUMULATION) {
    return code;
  }
  const GigaDenoiserRetCode copy_code = allocate_and_copy(result, output, output_size);
  return copy_code == GigaDenoiserRetCode::SUCCESS ? code : copy_code;
}

template <typename T, typename Function>
GigaDenoiserRetCode final_bytes(uint8_t** output, uint64_t* output_size, Function&& function) {
  if (!initialize_output(output, output_size)) {
    return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> result;
  const GigaDenoiserRetCode code = std::forward<Function>(function)(result);
  if (code != GigaDenoiserRetCode::SUCCESS) {
    return code;
  }
  return allocate_and_copy(result, output, output_size);
}

}  // namespace ctools

extern "C" {

GigaDenoiserRetCode GigaDenoiserDenoiseInit(const char* config_file, GigaDenoiserDenoise* model) {
  return ctools::guard(GigaDenoiserRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new gigaedge::denoiser::Denoiser(config_file));
    return GigaDenoiserRetCode::SUCCESS;
  });
}

void GigaDenoiserDenoiseFree(GigaDenoiserDenoise* model) {
  ctools::guard_void([&] {
    if (model != nullptr) {
      delete static_cast<gigaedge::denoiser::Denoiser*>(*model);
      *model = nullptr;
    }
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseCreateContext(GigaDenoiserDenoise model, GigaDenoiserContext* ctx,
                                                     uint32_t in_audio_sr, uint32_t out_audio_sr) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (ctx == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = nullptr;
    if (model == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = static_cast<gigaedge::denoiser::Denoiser*>(model)->CreateContext(in_audio_sr, out_audio_sr);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

void GigaDenoiserDenoiseDestroyContext(GigaDenoiserDenoise model, GigaDenoiserContext* ctx) {
  ctools::guard_void([&] {
    if (model != nullptr && ctx != nullptr && *ctx != nullptr) {
      static_cast<gigaedge::denoiser::Denoiser*>(model)->DestroyContext(
          reinterpret_cast<gigaedge::denoiser::Context<float>**>(ctx));
    }
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetContextId(GigaDenoiserDenoise model, const GigaDenoiserContext ctx,
                                                    uint64_t* id) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (id != nullptr) *id = 0;
    if (model == nullptr || ctx == nullptr || id == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *id = static_cast<gigaedge::denoiser::Denoiser*>(model)->GetContextId(
        static_cast<const gigaedge::denoiser::Context<float>*>(ctx));
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseSetContextId(GigaDenoiserDenoise model, GigaDenoiserContext ctx, uint64_t id) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr || ctx == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    static_cast<gigaedge::denoiser::Denoiser*>(model)->SetContextId(
        static_cast<gigaedge::denoiser::Context<float>*>(ctx), id);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetStreamStat(GigaDenoiserDenoise model, const GigaDenoiserContext ctx,
                                                     StreamStatStruct* stream_stat) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (stream_stat != nullptr) *stream_stat = {};
    if (model == nullptr || ctx == nullptr || stream_stat == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *stream_stat = static_cast<gigaedge::denoiser::Denoiser*>(model)->GetStreamStat(
        static_cast<const gigaedge::denoiser::Context<float>*>(ctx));
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetEngineThreads(const GigaDenoiserDenoise model, uint64_t* engine_threads) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (engine_threads != nullptr) *engine_threads = 0;
    if (model == nullptr || engine_threads == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *engine_threads = static_cast<gigaedge::denoiser::Denoiser*>(model)->GetEngineThreads();
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetInputFrameSize(const GigaDenoiserDenoise model, uint64_t* input_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (input_size != nullptr) *input_size = 0;
    if (model == nullptr || input_size == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *input_size = static_cast<gigaedge::denoiser::Denoiser*>(model)->GetModelInputFrameSize();
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetSampleRate(const GigaDenoiserDenoise model, uint64_t* sample_rate) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (sample_rate != nullptr) *sample_rate = 0;
    if (model == nullptr || sample_rate == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *sample_rate = static_cast<gigaedge::denoiser::Denoiser*>(model)->GetModelSampleRate();
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseProcessChunkFp(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                      const uint8_t* input, uint64_t input_size, uint8_t** output,
                                                      uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<gigaedge::denoiser::Denoiser*>(model)->ProcessChunk(
          samples, result, static_cast<gigaedge::denoiser::Context<float>*>(ctx));
    });
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseProcessChunkInt(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                       const uint8_t* input, uint64_t input_size, uint8_t** output,
                                                       uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<int16_t>(input, input_size, output, output_size,
                                          [&](const auto& samples, auto& result) {
                                            return static_cast<gigaedge::denoiser::Denoiser*>(model)->ProcessChunk(
                                                samples, result, static_cast<gigaedge::denoiser::Context<float>*>(ctx));
                                          });
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetFinalChunkFp(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                       uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::denoiser::Denoiser*>(model)->GetFinalChunk(
          result, static_cast<gigaedge::denoiser::Context<float>*>(ctx));
    });
  });
}

GigaDenoiserRetCode GigaDenoiserDenoiseGetFinalChunkInt(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                        uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::denoiser::Denoiser*>(model)->GetFinalChunk(
          result, static_cast<gigaedge::denoiser::Context<float>*>(ctx));
    });
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessInit(const char* config_file, GigaDenoiserAudioProcess* model) {
  return ctools::guard(GigaDenoiserRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new gigaedge::denoiser::AudioProcessor(config_file));
    return GigaDenoiserRetCode::SUCCESS;
  });
}

void GigaDenoiserAudioProcessFree(GigaDenoiserAudioProcess model) {
  ctools::guard_void([&] { delete static_cast<gigaedge::denoiser::AudioProcessor*>(model); });
}

#define GDN_AUDIO_PROCESS_ADD(function_name, method_name)                            \
  GigaDenoiserRetCode function_name(GigaDenoiserAudioProcess model) {                \
    return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] { \
      if (model == nullptr) {                                                        \
        return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;                 \
      }                                                                              \
      static_cast<gigaedge::denoiser::AudioProcessor*>(model)->method_name();        \
      return GigaDenoiserRetCode::SUCCESS;                                           \
    });                                                                              \
  }

GDN_AUDIO_PROCESS_ADD(GigaDenoiserAudioProcessAddAgc, AddAGC)
GDN_AUDIO_PROCESS_ADD(GigaDenoiserAudioProcessAddRc, AddRC)
GDN_AUDIO_PROCESS_ADD(GigaDenoiserAudioProcessAddLc, AddLC)
GDN_AUDIO_PROCESS_ADD(GigaDenoiserAudioProcessAddNg, AddNG)
GDN_AUDIO_PROCESS_ADD(GigaDenoiserAudioProcessAddLim, AddLim)
GDN_AUDIO_PROCESS_ADD(GigaDenoiserAudioProcessAddDrc, AddDRC)

#undef GDN_AUDIO_PROCESS_ADD

GigaDenoiserRetCode GigaDenoiserAudioProcessAddAgcWithParam(GigaDenoiserAudioProcess model, uint32_t sample_rate,
                                                            float target_power_level, float max_power_gain) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::denoiser::AudioProcessor*>(model)->AddAGC(sample_rate, target_power_level, max_power_gain,
                                                                    1e-2F, 1e-2F);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessAddRcWithParam(GigaDenoiserAudioProcess model, uint32_t sample_rate,
                                                           float target_level_db, float max_gain_db, float attack_time,
                                                           float release_time, float hold_time) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::denoiser::AudioProcessor*>(model)->AddRC(sample_rate, target_level_db, max_gain_db,
                                                                   attack_time, release_time, hold_time);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessAddLcWithParam(GigaDenoiserAudioProcess model, uint32_t sample_rate,
                                                           float target_level_db, float bypass_time, float max_gain_db,
                                                           uint32_t use_vad, uint32_t post_processing) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::denoiser::AudioProcessor*>(model)->AddLC(sample_rate, target_level_db, bypass_time,
                                                                   max_gain_db, use_vad != 0, post_processing != 0);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessAddNgWithParam(GigaDenoiserAudioProcess model, uint32_t sample_rate,
                                                           float threshold_db, float attack_time, float release_time,
                                                           float hold_time) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::denoiser::AudioProcessor*>(model)->AddNG(sample_rate, threshold_db, attack_time, release_time,
                                                                   hold_time);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessAddLimWithParam(GigaDenoiserAudioProcess model, uint32_t sample_rate,
                                                            float threshold_db, float knee_width, float attack_time,
                                                            float release_time) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::denoiser::AudioProcessor*>(model)->AddLim(sample_rate, threshold_db, knee_width, attack_time,
                                                                    release_time);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessAddDrcWithParam(GigaDenoiserAudioProcess model, uint32_t sample_rate,
                                                            float threshold_db, uint32_t ratio, float knee_width,
                                                            float attack_time, float release_time) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<gigaedge::denoiser::AudioProcessor*>(model)->AddDRC(sample_rate, threshold_db, ratio, knee_width,
                                                                    attack_time, release_time);
    return GigaDenoiserRetCode::SUCCESS;
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessChunkFp(GigaDenoiserAudioProcess model, const uint8_t* input,
                                                    uint64_t input_size, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<gigaedge::denoiser::AudioProcessor*>(model)->ProcessChunk(samples, result, nullptr);
    });
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessChunkInt(GigaDenoiserAudioProcess model, const uint8_t* input,
                                                     uint64_t input_size, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<int16_t>(
        input, input_size, output, output_size, [&](const auto& samples, auto& result) {
          return static_cast<gigaedge::denoiser::AudioProcessor*>(model)->ProcessChunk(samples, result, nullptr);
        });
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessGetFinalChunkFp(GigaDenoiserAudioProcess model, uint8_t** output,
                                                            uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::denoiser::AudioProcessor*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

GigaDenoiserRetCode GigaDenoiserAudioProcessGetFinalChunkInt(GigaDenoiserAudioProcess model, uint8_t** output,
                                                             uint64_t* output_size) {
  return ctools::guard(GigaDenoiserRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<gigaedge::denoiser::AudioProcessor*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

}  // extern "C"
