#include "spench/spench_c.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#include "include/spench.h"

namespace ctools {

template <typename Function>
SpenchRetCode guard(SpenchRetCode exception_code, Function&& function) noexcept {
  try {
    return std::forward<Function>(function)();
  } catch (const std::bad_alloc&) {
    return SpenchRetCode::ERR_NOT_ENOUGH_MEMORY;
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
SpenchRetCode bytes_to_vector(const uint8_t* input, uint64_t input_size, std::vector<T>& output) {
  if (input == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size % sizeof(T) != 0) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input_size > std::numeric_limits<size_t>::max()) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  const uint64_t sample_count = input_size / sizeof(T);
  if (sample_count > std::numeric_limits<size_t>::max()) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  output.resize(static_cast<size_t>(sample_count));
  if (input_size != 0) {
    std::memcpy(output.data(), input, static_cast<size_t>(input_size));
  }
  return SpenchRetCode::SUCCESS;
}

template <typename T>
SpenchRetCode allocate_and_copy(const std::vector<T>& input, uint8_t** output, uint64_t* output_size) {
  if (!initialize_output(output, output_size)) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  if (input.empty()) {
    return SpenchRetCode::SUCCESS;
  }
  if (input.size() > std::numeric_limits<uint64_t>::max() / sizeof(T)) {
    return SpenchRetCode::ERR_ALLOCATION_PROBLEM;
  }

  const uint64_t bytes = static_cast<uint64_t>(input.size()) * sizeof(T);
  if (bytes > std::numeric_limits<size_t>::max()) {
    return SpenchRetCode::ERR_ALLOCATION_PROBLEM;
  }
  *output = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(bytes)));
  if (*output == nullptr) {
    return SpenchRetCode::ERR_NOT_ENOUGH_MEMORY;
  }
  std::memcpy(*output, input.data(), static_cast<size_t>(bytes));
  *output_size = bytes;
  return SpenchRetCode::SUCCESS;
}

template <typename T, typename Function>
SpenchRetCode process_bytes(const uint8_t* input, uint64_t input_size, uint8_t** output, uint64_t* output_size,
                            Function&& function) {
  if (!initialize_output(output, output_size)) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> samples;
  const SpenchRetCode decode_code = bytes_to_vector(input, input_size, samples);
  if (decode_code != SpenchRetCode::SUCCESS) {
    return decode_code;
  }

  std::vector<T> result;
  const SpenchRetCode code = std::forward<Function>(function)(samples, result);
  if (code != SpenchRetCode::SUCCESS && code != SpenchRetCode::CONTEXT_ACCUMULATION) {
    return code;
  }
  const SpenchRetCode copy_code = allocate_and_copy(result, output, output_size);
  return copy_code == SpenchRetCode::SUCCESS ? code : copy_code;
}

template <typename T, typename Function>
SpenchRetCode final_bytes(uint8_t** output, uint64_t* output_size, Function&& function) {
  if (!initialize_output(output, output_size)) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  std::vector<T> result;
  const SpenchRetCode code = std::forward<Function>(function)(result);
  if (code != SpenchRetCode::SUCCESS) {
    return code;
  }
  return allocate_and_copy(result, output, output_size);
}

}  // namespace ctools

extern "C" {

SpenchRetCode SpenchDenoiseInit(const char* config_file, SpenchDenoise* model) {
  return ctools::guard(SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new spench::SpenchDenoise(config_file));
    return SpenchRetCode::SUCCESS;
  });
}

void SpenchDenoiseFree(SpenchDenoise* model) {
  ctools::guard_void([&] {
    if (model != nullptr) {
      delete static_cast<spench::SpenchDenoise*>(*model);
      *model = nullptr;
    }
  });
}

SpenchRetCode SpenchDenoiseCreateContext(SpenchDenoise model, SpenchContext* ctx, uint32_t in_audio_sr,
                                         uint32_t out_audio_sr) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (ctx == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = nullptr;
    if (model == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *ctx = static_cast<spench::SpenchDenoise*>(model)->CreateContext(in_audio_sr, out_audio_sr);
    return SpenchRetCode::SUCCESS;
  });
}

void SpenchDenoiseDestroyContext(SpenchDenoise model, SpenchContext* ctx) {
  ctools::guard_void([&] {
    if (model != nullptr && ctx != nullptr && *ctx != nullptr) {
      static_cast<spench::SpenchDenoise*>(model)->DestroyContext(reinterpret_cast<spench::SpenchContext<float>**>(ctx));
    }
  });
}

SpenchRetCode SpenchDenoiseGetContextId(SpenchDenoise model, const SpenchContext ctx, uint64_t* id) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (id != nullptr) *id = 0;
    if (model == nullptr || ctx == nullptr || id == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *id =
        static_cast<spench::SpenchDenoise*>(model)->GetContextId(static_cast<const spench::SpenchContext<float>*>(ctx));
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchDenoiseSetContextId(SpenchDenoise model, SpenchContext ctx, uint64_t id) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr || ctx == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    static_cast<spench::SpenchDenoise*>(model)->SetContextId(static_cast<spench::SpenchContext<float>*>(ctx), id);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchDenoiseGetStreamStat(SpenchDenoise model, const SpenchContext ctx, StreamStatStruct* stream_stat) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (stream_stat != nullptr) *stream_stat = {};
    if (model == nullptr || ctx == nullptr || stream_stat == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *stream_stat = static_cast<spench::SpenchDenoise*>(model)->GetStreamStat(
        static_cast<const spench::SpenchContext<float>*>(ctx));
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchDenoiseGetEngineThreads(const SpenchDenoise model, uint64_t* engine_threads) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (engine_threads != nullptr) *engine_threads = 0;
    if (model == nullptr || engine_threads == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *engine_threads = static_cast<spench::SpenchDenoise*>(model)->GetEngineThreads();
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchDenoiseGetInputFrameSize(const SpenchDenoise model, uint64_t* input_size) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (input_size != nullptr) *input_size = 0;
    if (model == nullptr || input_size == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *input_size = static_cast<spench::SpenchDenoise*>(model)->GetModelInputFrameSize();
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchDenoiseGetSampleRate(const SpenchDenoise model, uint64_t* sample_rate) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (sample_rate != nullptr) *sample_rate = 0;
    if (model == nullptr || sample_rate == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *sample_rate = static_cast<spench::SpenchDenoise*>(model)->GetModelSampleRate();
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchDenoiseProcessChunkFp(SpenchDenoise model, SpenchContext ctx, const uint8_t* input,
                                          uint64_t input_size, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<spench::SpenchDenoise*>(model)->ProcessChunk(samples, result,
                                                                      static_cast<spench::SpenchContext<float>*>(ctx));
    });
  });
}

SpenchRetCode SpenchDenoiseProcessChunkInt(SpenchDenoise model, SpenchContext ctx, const uint8_t* input,
                                           uint64_t input_size, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::process_bytes<int16_t>(input, input_size, output, output_size,
                                          [&](const auto& samples, auto& result) {
                                            return static_cast<spench::SpenchDenoise*>(model)->ProcessChunk(
                                                samples, result, static_cast<spench::SpenchContext<float>*>(ctx));
                                          });
  });
}

SpenchRetCode SpenchDenoiseGetFinalChunkFp(SpenchDenoise model, SpenchContext ctx, uint8_t** output,
                                           uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<spench::SpenchDenoise*>(model)->GetFinalChunk(result,
                                                                       static_cast<spench::SpenchContext<float>*>(ctx));
    });
  });
}

SpenchRetCode SpenchDenoiseGetFinalChunkInt(SpenchDenoise model, SpenchContext ctx, uint8_t** output,
                                            uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr || ctx == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<spench::SpenchDenoise*>(model)->GetFinalChunk(result,
                                                                       static_cast<spench::SpenchContext<float>*>(ctx));
    });
  });
}

SpenchRetCode SpenchAudioProcessInit(const char* config_file, SpenchAudioProcess* model) {
  return ctools::guard(SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM, [&] {
    if (model == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = nullptr;
    if (config_file == nullptr) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    *model = static_cast<void*>(new spench::SpenchAudioProcess(config_file));
    return SpenchRetCode::SUCCESS;
  });
}

void SpenchAudioProcessFree(SpenchAudioProcess model) {
  ctools::guard_void([&] { delete static_cast<spench::SpenchAudioProcess*>(model); });
}

#define SPENCH_AUDIO_PROCESS_ADD(function_name, method_name)                   \
  SpenchRetCode function_name(SpenchAudioProcess model) {                      \
    return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] { \
      if (model == nullptr) {                                                  \
        return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;                 \
      }                                                                        \
      static_cast<spench::SpenchAudioProcess*>(model)->method_name();          \
      return SpenchRetCode::SUCCESS;                                           \
    });                                                                        \
  }

SPENCH_AUDIO_PROCESS_ADD(SpenchAudioProcessAddAgc, AddAGC)
SPENCH_AUDIO_PROCESS_ADD(SpenchAudioProcessAddRc, AddRC)
SPENCH_AUDIO_PROCESS_ADD(SpenchAudioProcessAddLc, AddLC)
SPENCH_AUDIO_PROCESS_ADD(SpenchAudioProcessAddNg, AddNG)
SPENCH_AUDIO_PROCESS_ADD(SpenchAudioProcessAddLim, AddLim)
SPENCH_AUDIO_PROCESS_ADD(SpenchAudioProcessAddDrc, AddDRC)

#undef SPENCH_AUDIO_PROCESS_ADD

SpenchRetCode SpenchAudioProcessAddAgcWithParam(SpenchAudioProcess model, uint32_t sample_rate,
                                                float target_power_level, float max_power_gain) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<spench::SpenchAudioProcess*>(model)->AddAGC(sample_rate, target_power_level, max_power_gain, 1e-2F,
                                                            1e-2F);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchAudioProcessAddRcWithParam(SpenchAudioProcess model, uint32_t sample_rate, float target_level_db,
                                               float max_gain_db, float attack_time, float release_time,
                                               float hold_time) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<spench::SpenchAudioProcess*>(model)->AddRC(sample_rate, target_level_db, max_gain_db, attack_time,
                                                           release_time, hold_time);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchAudioProcessAddLcWithParam(SpenchAudioProcess model, uint32_t sample_rate, float target_level_db,
                                               float bypass_time, float max_gain_db, uint32_t use_vad,
                                               uint32_t post_processing) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<spench::SpenchAudioProcess*>(model)->AddLC(sample_rate, target_level_db, bypass_time, max_gain_db,
                                                           use_vad != 0, post_processing != 0);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchAudioProcessAddNgWithParam(SpenchAudioProcess model, uint32_t sample_rate, float threshold_db,
                                               float attack_time, float release_time, float hold_time) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<spench::SpenchAudioProcess*>(model)->AddNG(sample_rate, threshold_db, attack_time, release_time,
                                                           hold_time);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchAudioProcessAddLimWithParam(SpenchAudioProcess model, uint32_t sample_rate, float threshold_db,
                                                float knee_width, float attack_time, float release_time) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<spench::SpenchAudioProcess*>(model)->AddLim(sample_rate, threshold_db, knee_width, attack_time,
                                                            release_time);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchAudioProcessAddDrcWithParam(SpenchAudioProcess model, uint32_t sample_rate, float threshold_db,
                                                uint32_t ratio, float knee_width, float attack_time,
                                                float release_time) {
  return ctools::guard(SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT, [&] {
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    static_cast<spench::SpenchAudioProcess*>(model)->AddDRC(sample_rate, threshold_db, ratio, knee_width, attack_time,
                                                            release_time);
    return SpenchRetCode::SUCCESS;
  });
}

SpenchRetCode SpenchAudioProcessChunkFp(SpenchAudioProcess model, const uint8_t* input, uint64_t input_size,
                                        uint8_t** output, uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<float>(input, input_size, output, output_size, [&](const auto& samples, auto& result) {
      return static_cast<spench::SpenchAudioProcess*>(model)->ProcessChunk(samples, result, nullptr);
    });
  });
}

SpenchRetCode SpenchAudioProcessChunkInt(SpenchAudioProcess model, const uint8_t* input, uint64_t input_size,
                                         uint8_t** output, uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::process_bytes<int16_t>(
        input, input_size, output, output_size, [&](const auto& samples, auto& result) {
          return static_cast<spench::SpenchAudioProcess*>(model)->ProcessChunk(samples, result, nullptr);
        });
  });
}

SpenchRetCode SpenchAudioProcessGetFinalChunkFp(SpenchAudioProcess model, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<float>(output, output_size, [&](auto& result) {
      return static_cast<spench::SpenchAudioProcess*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

SpenchRetCode SpenchAudioProcessGetFinalChunkInt(SpenchAudioProcess model, uint8_t** output, uint64_t* output_size) {
  return ctools::guard(SpenchRetCode::ERR_INFERENCE_PROBLEM, [&] {
    if (!ctools::initialize_output(output, output_size)) {
      return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
    if (model == nullptr) return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    return ctools::final_bytes<int16_t>(output, output_size, [&](auto& result) {
      return static_cast<spench::SpenchAudioProcess*>(model)->GetFinalChunk(result, nullptr);
    });
  });
}

}  // extern "C"
