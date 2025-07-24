#include "spench/spench_c.h"
#include "include/spench.h"
#include "spench/src/spench_task.h"
#include <cstring>
#include <vector>

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
namespace ctools {

template <typename T>
SpenchRetCode AllocateAndCopy(const std::vector<T>& input,
                              uint8_t**             output,
                              uint64_t*             output_size) {
  if (output == nullptr || output_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  if (input.empty()) {
    *output = nullptr;
    *output_size = 0;
    return SpenchRetCode::SUCCESS;
  }

  const uint64_t bytes = static_cast<uint64_t>(input.size() * sizeof(T));
  *output = reinterpret_cast<uint8_t*>(std::calloc(bytes, sizeof(uint8_t)));
  if (*output == nullptr) {
    *output_size = 0;
    return SpenchRetCode::ERR_NOT_ENOUGH_MEMORY;
  }

  std::memcpy(*output, input.data(), bytes);
  *output_size = bytes;
  return SpenchRetCode::SUCCESS;
}

}  // namespace ctools

// -----------------------------------------------------------------------------
// C API implementation
// -----------------------------------------------------------------------------
extern "C" {

// ------------------------------ SpenchDenoise ---------------------------------

SpenchRetCode SpenchDenoiseInit(const char* config_file, SpenchDenoise* model) {
  if (config_file == nullptr || model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *model = reinterpret_cast<void*>(new spench::SpenchDenoise(config_file));
  } catch (...) {
    return SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchEncryptedDenoiseInit(const char*  config_file,
                                         const char*  password,
                                         const char*  checksum,
                                         SpenchDenoise* model) {
  (void)checksum;  // Not used in current constructor, kept for API symmetry.
  if (config_file == nullptr || password == nullptr || model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *model = reinterpret_cast<void*>(new spench::SpenchDenoise(config_file, password));
  } catch (...) {
    return SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }
  return SpenchRetCode::SUCCESS;
}

void SpenchDenoiseFree(SpenchDenoise* model) {
  if (model == nullptr || *model == nullptr) {
    return;
  }
  auto* denoise = static_cast<spench::SpenchDenoise*>(*model);
  delete denoise;
  *model = nullptr;
}

SpenchRetCode SpenchDenoiseCreateContext(SpenchDenoise  model,
                                         SpenchContext* ctx,
                                         uint32_t       in_audio_sr,
                                         uint32_t       out_audio_sr) {
  if (model == nullptr || ctx == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *ctx = static_cast<spench::SpenchDenoise*>(model)
             ->CreateContext(in_audio_sr, out_audio_sr);
  } catch (...) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  return SpenchRetCode::SUCCESS;
}

void SpenchDenoiseDestroyContext(SpenchDenoise model, SpenchContext* ctx) {
  if (model == nullptr || ctx == nullptr || *ctx == nullptr) {
    return;
  }
  static_cast<spench::SpenchDenoise*>(model)
    ->DestroyContext(reinterpret_cast<spench::SpenchContext<float>**>(ctx));
}

SpenchRetCode SpenchDenoiseGetContextId(SpenchDenoise      model,
                                        const SpenchContext ctx,
                                        uint64_t*          id) {
  if (model == nullptr || ctx == nullptr || id == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  *id = static_cast<spench::SpenchDenoise*>(model)
          ->GetContextId(static_cast<const spench::SpenchContext<float>*>(ctx));
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchDenoiseSetContextId(SpenchDenoise model,
                                        SpenchContext ctx,
                                        uint64_t      id) {
  if (model == nullptr || ctx == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchDenoise*>(model)
    ->SetContextId(static_cast<spench::SpenchContext<float>*>(ctx), id);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchDenoiseGetStreamStat(SpenchDenoise       model,
                                         const SpenchContext ctx,
                                         StreamStatStruct*   stream_stat) {
  if (model == nullptr || ctx == nullptr || stream_stat == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  *stream_stat = static_cast<spench::SpenchDenoise*>(model)
                   ->GetStreamStat(static_cast<const spench::SpenchContext<float>*>(ctx));
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchDenoiseGetEngineThreads(SpenchDenoise model,
                                            uint64_t*     engine_threads) {
  if (model == nullptr || engine_threads == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  *engine_threads = static_cast<spench::SpenchDenoise*>(model)->GetEngineThreads();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchDenoiseGetSampleRate(const SpenchDenoise model,
                                         uint64_t*           sample_rate) {
  if (model == nullptr || sample_rate == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  *sample_rate = static_cast<spench::SpenchDenoise*>(model)->GetModelSampleRate();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchDenoiseGetInputFrameSize(const SpenchDenoise model,
                                             uint64_t*           input_size) {
  if (model == nullptr || input_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  *input_size = static_cast<spench::SpenchDenoise*>(model)->GetModelInputFrameSize();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchDenoiseProcessChunkFp(SpenchDenoise  model,
                                          SpenchContext  ctx,
                                          const uint8_t* data,
                                          uint64_t       input_size,
                                          uint8_t**      output,
                                          uint64_t*      output_size) {
  if (model == nullptr || ctx == nullptr || data == nullptr ||
      output == nullptr || output_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  const float* input_ptr = reinterpret_cast<const float*>(data);
  std::vector<float> res;
  SpenchRetCode rc = static_cast<spench::SpenchDenoise*>(model)->ProcessChunk(
    std::vector<float>(input_ptr, input_ptr + input_size / sizeof(float)),
    res,
    static_cast<spench::SpenchContext<float>*>(ctx));

  if (rc == SpenchRetCode::SUCCESS || rc == SpenchRetCode::CONTEXT_ACCUMULATION) {
    return ctools::AllocateAndCopy<float>(res, output, output_size);
  }
  return rc;
}

SpenchRetCode SpenchDenoiseProcessChunkInt(SpenchDenoise  model,
                                           SpenchContext  ctx,
                                           const uint8_t* data,
                                           uint64_t       input_size,
                                           uint8_t**      output,
                                           uint64_t*      output_size) {
  if (model == nullptr || ctx == nullptr || data == nullptr ||
      output == nullptr || output_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  const int16_t* input_ptr = reinterpret_cast<const int16_t*>(data);
  std::vector<int16_t> res;
  SpenchRetCode rc = static_cast<spench::SpenchDenoise*>(model)->ProcessChunk(
    std::vector<int16_t>(input_ptr, input_ptr + input_size / sizeof(int16_t)),
    res,
    static_cast<spench::SpenchContext<float>*>(ctx));

  if (rc == SpenchRetCode::SUCCESS || rc == SpenchRetCode::CONTEXT_ACCUMULATION) {
    return ctools::AllocateAndCopy<int16_t>(res, output, output_size);
  }
  return rc;
}

SpenchRetCode SpenchDenoiseGetFinalChunkFp(SpenchDenoise model,
                                           SpenchContext ctx,
                                           uint8_t**     output,
                                           uint64_t*     output_size) {
  if (model == nullptr || ctx == nullptr || output == nullptr || output_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  std::vector<float> res;
  SpenchRetCode rc = static_cast<spench::SpenchDenoise*>(model)->GetFinalChunk(
    res,
    static_cast<spench::SpenchContext<float>*>(ctx));

  if (rc != SpenchRetCode::SUCCESS) {
    return rc;
  }

  if (res.empty()) {
    *output = nullptr;
    *output_size = 0;
    return SpenchRetCode::SUCCESS;
  }
  return ctools::AllocateAndCopy<float>(res, output, output_size);
}

SpenchRetCode SpenchDenoiseGetFinalChunkInt(SpenchDenoise model,
                                            SpenchContext ctx,
                                            uint8_t**     output,
                                            uint64_t*     output_size) {
  if (model == nullptr || ctx == nullptr || output == nullptr || output_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  std::vector<int16_t> res;
  SpenchRetCode rc = static_cast<spench::SpenchDenoise*>(model)->GetFinalChunk(
    res,
    static_cast<spench::SpenchContext<float>*>(ctx));

  if (rc != SpenchRetCode::SUCCESS) {
    return rc;
  }

  if (res.empty()) {
    *output = nullptr;
    *output_size = 0;
    return SpenchRetCode::SUCCESS;
  }
  return ctools::AllocateAndCopy<int16_t>(res, output, output_size);
}

// --------------------------- SpenchAudioProcess -----------------------------

SpenchRetCode SpenchAudioProcessInit(const char*         config_file,
                                     SpenchAudioProcess* model) {
  if (config_file == nullptr || model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *model = reinterpret_cast<void*>(new spench::SpenchAudioProcess(config_file));
  } catch (...) {
    return SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }
  return SpenchRetCode::SUCCESS;
}

void SpenchAudioProcessFree(SpenchAudioProcess model) {
  delete static_cast<spench::SpenchAudioProcess*>(model);
}

SpenchRetCode SpenchAudioProcessAddAgc(SpenchAudioProcess model) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddAGC();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddAgcWithParam(SpenchAudioProcess model,
                                                uint32_t           audio_sample_rate,
                                                float              target_power_level,
                                                float              max_power_gain) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddAGC(audio_sample_rate,
                                                          target_power_level,
                                                          max_power_gain,
                                                          1e-2f,
                                                          1e-2f);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddRc(SpenchAudioProcess model) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddRC();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddRcWithParam(SpenchAudioProcess model,
                                               uint32_t           audio_sample_rate,
                                               float              target_level_db,
                                               float              max_gain_db,
                                               float              attack_time,
                                               float              release_time,
                                               float              hold_time) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddRC(audio_sample_rate,
                                                         target_level_db,
                                                         max_gain_db,
                                                         attack_time,
                                                         release_time,
                                                         hold_time);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddLc(SpenchAudioProcess model) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddLC();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddLcWithParam(SpenchAudioProcess model,
                                               uint32_t           audio_sample_rate,
                                               float              target_level_db,
                                               float              bypass_time,
                                               float              max_gain_db) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddLC(audio_sample_rate,
                                                         target_level_db,
                                                         bypass_time,
                                                         max_gain_db);
  return SpenchRetCode::SUCCESS;
}

// -------------------------- Newly added methods -----------------------------

SpenchRetCode SpenchAudioProcessAddNg(SpenchAudioProcess model) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddNG();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddNgWithParam(SpenchAudioProcess model,
                                               uint32_t           audio_sample_rate,
                                               float              threshold_db,
                                               float              attack_time,
                                               float              release_time,
                                               float              hold_time) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddNG(audio_sample_rate,
                                                         threshold_db,
                                                         attack_time,
                                                         release_time,
                                                         hold_time);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddLim(SpenchAudioProcess model) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddLim();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddLimWithParam(SpenchAudioProcess model,
                                                uint32_t           audio_sample_rate,
                                                float              threshold_db,
                                                float              knee_width,
                                                float              attack_time,
                                                float              release_time) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddLim(audio_sample_rate,
                                                          threshold_db,
                                                          knee_width,
                                                          attack_time,
                                                          release_time);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddDrc(SpenchAudioProcess model) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddDRC();
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessAddDrcWithParam(SpenchAudioProcess model,
                                                uint32_t           audio_sample_rate,
                                                float              threshold_db,
                                                uint32_t           ratio,
                                                float              knee_width,
                                                float              attack_time,
                                                float              release_time) {
  if (model == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  static_cast<spench::SpenchAudioProcess*>(model)->AddDRC(audio_sample_rate,
                                                          threshold_db,
                                                          ratio,
                                                          knee_width,
                                                          attack_time,
                                                          release_time);
  return SpenchRetCode::SUCCESS;
}

SpenchRetCode SpenchAudioProcessChunkFp(SpenchAudioProcess model,
                                        const uint8_t*     memory_in,
                                        uint64_t           in_size,
                                        uint8_t**          memory_out,
                                        uint64_t*          out_size) {
  if (model == nullptr || memory_in == nullptr || memory_out == nullptr || out_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  const float* input_ptr = reinterpret_cast<const float*>(memory_in);
  std::vector<float> res;
  SpenchRetCode rc = static_cast<spench::SpenchAudioProcess*>(model)->ProcessChunk(
    std::vector<float>(input_ptr, input_ptr + in_size / sizeof(float)),
    res,
    nullptr);

  if (rc == SpenchRetCode::SUCCESS || rc == SpenchRetCode::CONTEXT_ACCUMULATION) {
    return ctools::AllocateAndCopy<float>(res, memory_out, out_size);
  }
  return rc;
}

SpenchRetCode SpenchAudioProcessChunkInt(SpenchAudioProcess model,
                                         const uint8_t*     memory_in,
                                         uint64_t           in_size,
                                         uint8_t**          memory_out,
                                         uint64_t*          out_size) {
  if (model == nullptr || memory_in == nullptr || memory_out == nullptr || out_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  const int16_t* input_ptr = reinterpret_cast<const int16_t*>(memory_in);
  std::vector<int16_t> res;
  SpenchRetCode rc = static_cast<spench::SpenchAudioProcess*>(model)->ProcessChunk(
    std::vector<int16_t>(input_ptr, input_ptr + in_size / sizeof(int16_t)),
    res,
    nullptr);

  if (rc == SpenchRetCode::SUCCESS || rc == SpenchRetCode::CONTEXT_ACCUMULATION) {
    return ctools::AllocateAndCopy<int16_t>(res, memory_out, out_size);
  }
  return rc;
}

SpenchRetCode SpenchAudioProcessGetFinalChunkFp(SpenchAudioProcess model,
                                                uint8_t**          memory_out,
                                                uint64_t*          out_size) {
  if (model == nullptr || memory_out == nullptr || out_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  std::vector<float> res;
  SpenchRetCode rc = static_cast<spench::SpenchAudioProcess*>(model)->GetFinalChunk(
    res,
    nullptr);

  if (rc != SpenchRetCode::SUCCESS) {
    return rc;
  }

  if (res.empty()) {
    *memory_out = nullptr;
    *out_size = 0;
    return SpenchRetCode::SUCCESS;
  }
  return ctools::AllocateAndCopy<float>(res, memory_out, out_size);
}

SpenchRetCode SpenchAudioProcessGetFinalChunkInt(SpenchAudioProcess model,
                                                 uint8_t**          memory_out,
                                                 uint64_t*          out_size) {
  if (model == nullptr || memory_out == nullptr || out_size == nullptr) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }

  std::vector<int16_t> res;
  SpenchRetCode rc = static_cast<spench::SpenchAudioProcess*>(model)->GetFinalChunk(
    res,
    nullptr);

  if (rc != SpenchRetCode::SUCCESS) {
    return rc;
  }

  if (res.empty()) {
    *memory_out = nullptr;
    *out_size = 0;
    return SpenchRetCode::SUCCESS;
  }
  return ctools::AllocateAndCopy<int16_t>(res, memory_out, out_size);
}

}  // extern "C"
