#include "include/spench_c.h"
#include "include/spench.h"
#include "spench/src/task.h"
#include <iostream>
#include <cstring>

namespace ctools {
template <typename T>
SpenchRetCode AllocateAndCopy(std::vector<T> input, uint8_t **output, uint64_t *output_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  uint64_t bytes_to_allocate = input.size() * sizeof(T);
  if (input.empty()) {
    std::cerr << __FUNCTION__ << "input container is empty" << std::endl;
    *output_size = 0;
    ret_code = SpenchRetCode::ERR_ALLOCATION_PROBLEM;
  }
  if (ret_code == SpenchRetCode::SUCCESS) {
    *output = reinterpret_cast<uint8_t*>(std::calloc(bytes_to_allocate, sizeof(uint8_t)));
    if (*output == NULL) {
      ret_code = SpenchRetCode::ERR_NOT_ENOUGH_MEMORY;
    }
    *output_size = bytes_to_allocate;
  }
  if (ret_code == SpenchRetCode::SUCCESS) {
    std::memcpy(*output, input.data(), bytes_to_allocate);
  }
  return ret_code;
}
}  // namespace ctools

SpenchRetCode SpenchDenoiseInit(const char* config_file, SpenchDenoise* model) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (config_file == NULL || model == NULL) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *model = reinterpret_cast<void *>(new spench::SpenchDenoise(config_file));
  } catch(...) {
    ret_code = SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }
  return ret_code;
}

SpenchRetCode SpenchEncryptedDenoiseInit(const char* config_file,
                                         const char* password,
                                         const char* checksum,
                                         SpenchDenoise* model) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (config_file == NULL || password == NULL || model == NULL) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *model = reinterpret_cast<void *>(new spench::SpenchDenoise(config_file, password));
  } catch(...) {
    ret_code = SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }
  return ret_code;
}

void SpenchDenoiseFree(SpenchDenoise* model) {
  spench::SpenchDenoise * denoise = (spench::SpenchDenoise *)*model;
  if (denoise != nullptr) {
    delete denoise;
    denoise = nullptr;
  }
}

SpenchRetCode SpenchDenoiseCreateContext(SpenchDenoise model,
                                         SpenchContext* ctx,
                                         uint32_t in_audio_sr,
                                         uint32_t out_audio_sr) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    try {
      *ctx = ((spench::SpenchDenoise*)model)->CreateContext(in_audio_sr, out_audio_sr);
    } catch(...) {
      ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
    }
  }
  return ret_code;
}

void SpenchDenoiseDestroyContext(SpenchDenoise model,
                                 SpenchContext* ctx) {
  ((spench::SpenchDenoise*)model)->DestroyContext((spench::DenoiseContext<float>**)ctx);
}

SpenchRetCode SpenchDenoiseGetContextId(SpenchDenoise model,
                                        const SpenchContext ctx,
                                        uint64_t* id) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr || ctx == nullptr || id == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    *id = ((spench::SpenchDenoise*)model)->GetContextId((const spench::DenoiseContext<float>*)ctx);
  }
  return ret_code;
}

SpenchRetCode SpenchDenoiseSetContextId(SpenchDenoise model,
                                        SpenchContext ctx,
                                        uint64_t id) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr || ctx == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    ((spench::SpenchDenoise*)model)->SetContextId((spench::DenoiseContext<float>*)ctx, id);
  }
  return ret_code;
}

SpenchRetCode SpenchDenoiseGetStreamStat(SpenchDenoise model,
                                         const SpenchContext ctx,
                                         StreamStatStruct* stream_stat) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr || ctx == nullptr || stream_stat == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    *stream_stat = ((spench::SpenchDenoise*)model)->GetStreamStat((const spench::DenoiseContext<float>*)ctx);
  }
  return ret_code;
}

SpenchRetCode SpenchDenoiseGetEngineThreads(SpenchDenoise model, uint64_t* engine_threads) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr || engine_threads == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    *engine_threads = ((spench::SpenchDenoise*)model)->GetEngineThreads();
  }
  return ret_code;
}

SpenchRetCode   SpenchDenoiseGetSampleRate(const SpenchDenoise model,
                                           u_int64_t* sample_rate) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr || sample_rate == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    *sample_rate = ((spench::SpenchDenoise*)model)->GetModelSampleRate();
  }
  return ret_code;
}

SpenchRetCode SpenchDenoiseGetInputFrameSize(const SpenchDenoise model,
                                             u_int64_t* input_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr || input_size == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    *input_size = ((spench::SpenchDenoise*)model)->GetModelInputFrameSize();
  }
  return ret_code;
}

SpenchRetCode SpenchDenoiseProcessChunkFp(SpenchDenoise model,
                                          SpenchContext ctx,
                                          const uint8_t *data,
                                          uint64_t input_size,
                                          uint8_t **output,
                                          uint64_t *output_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  const float* input_ptr = reinterpret_cast<const float*> (data);
  std::vector<float> res;
  ret_code = ((spench::SpenchDenoise*)model)->
                              ProcessChunk(std::vector<float> (input_ptr, input_ptr + input_size / sizeof(float)),
                                           res,
                                           (spench::DenoiseContext<float>*)ctx);
  if (ret_code == SpenchRetCode::SUCCESS ||
      ret_code == SpenchRetCode::CONTEXT_ACCUMULATION) {
    ret_code = ctools::AllocateAndCopy<float>(res, output, output_size);
  }

  return ret_code;
}

SpenchRetCode SpenchDenoiseProcessChunkInt(SpenchDenoise model,
                                           SpenchContext ctx,
                                           const uint8_t *data,
                                           uint64_t input_size,
                                           uint8_t **output,
                                           uint64_t *output_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  const int16_t* input_ptr = reinterpret_cast<const int16_t*> (data);
  std::vector<int16_t> res;
  ret_code = ((spench::SpenchDenoise*)model)->
                              ProcessChunk(std::vector<int16_t> (input_ptr, input_ptr + input_size / sizeof(int16_t)),
                                           res,
                                           (spench::DenoiseContext<float>*)ctx);
  if (ret_code == SpenchRetCode::SUCCESS ||
      ret_code == SpenchRetCode::CONTEXT_ACCUMULATION) {
    ret_code = ctools::AllocateAndCopy<int16_t>(res, output, output_size);
  }

  return ret_code;
}

SpenchRetCode SpenchDenoiseGetFinalChunkFp(SpenchDenoise model,
                                           SpenchContext ctx,
                                           uint8_t** output,
                                           u_int64_t* output_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  std::vector<float> res;
  ret_code = ((spench::SpenchDenoise*)model)->
                              GetFinalChunk(res,
                                            (spench::DenoiseContext<float>*)ctx);
  if (ret_code == SpenchRetCode::SUCCESS) {
    if (res.size() == 0) {
      *output = NULL;
      *output_size = 0;
    } else {
      ret_code = ctools::AllocateAndCopy<float>(res, output, output_size);
    }
  }

  return ret_code;
}

SpenchRetCode SpenchDenoiseGetFinalChunkInt(SpenchDenoise model,
                                          SpenchContext ctx,
                                          uint8_t **output,
                                          uint64_t *output_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  std::vector<int16_t> res;
  ret_code = ((spench::SpenchDenoise*)model)->
                              GetFinalChunk(res,
                                            (spench::DenoiseContext<float>*)ctx);
  if (ret_code == SpenchRetCode::SUCCESS) {
    if (res.size() == 0) {
      *output = NULL;
      *output_size = 0;
    } else {
      ret_code = ctools::AllocateAndCopy<int16_t>(res, output, output_size);
    }
  }

  return ret_code;
}

SpenchRetCode   SpenchAudioProcessInit(const char* config_file,
                                       SpenchAudioProcess* model) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (config_file == NULL || model == NULL) {
    return SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  }
  try {
    *model = reinterpret_cast<void *>(new spench::SpenchAudioProcess(config_file));
  } catch(...) {
    ret_code = SpenchRetCode::ERR_DENOISER_CREATE_AND_SETUP_PROBLEM;
  }
  return ret_code;
}

void  SpenchAudioProcessFree(SpenchAudioProcess* model) {
  spench::SpenchAudioProcess * audio_process = (spench::SpenchAudioProcess *)*model;
  if (audio_process != nullptr) {
    delete audio_process;
    audio_process = nullptr;
  }
}

SpenchRetCode   SpenchAudioProcessAddAgc(SpenchAudioProcess model) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    ((spench::SpenchAudioProcess*)model)->AddAGC();
  }
  return ret_code;
}

SpenchRetCode   SpenchAudioProcessAddAgcWithParam(SpenchAudioProcess model,
                                                  u_int32_t audio_sample_rate,
                                                  float target_power_level,
                                                  float max_power_gain) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    ((spench::SpenchAudioProcess*)model)->AddAGC(audio_sample_rate,
                                                 target_power_level,
                                                 max_power_gain);
  }
  return ret_code;
}

SpenchRetCode   SpenchAudioProcessAddRc(SpenchAudioProcess model) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    ((spench::SpenchAudioProcess*)model)->AddRC();
  }
  return ret_code;
}

SpenchRetCode   SpenchAudioProcessAddRcWithParam(SpenchAudioProcess model,
                                                 u_int32_t audio_sample_rate,
                                                 float target_level_db,
                                                 float max_gain_db,
                                                 float attack_time,
                                                 float release_time,
                                                 float hold_time) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  if (model == nullptr) {
    ret_code = SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT;
  } else {
    ((spench::SpenchAudioProcess*)model)->AddRC(audio_sample_rate,
                                                target_level_db,
                                                max_gain_db,
                                                attack_time,
                                                release_time,
                                                hold_time);
  }
  return ret_code;
}

SpenchRetCode   SpenchAudioProcessChunkFp(SpenchAudioProcess model,
                                          const u_int8_t* memory_in,
                                          u_int64_t in_size,
                                          u_int8_t** memory_out,
                                          u_int64_t* out_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  const float* input_ptr = reinterpret_cast<const float*> (memory_in);
  std::vector<float> res;
  ((spench::SpenchAudioProcess*)model)->
                              ProcessChunk(std::vector<float> (input_ptr, input_ptr + in_size / sizeof(float)),
                                           res);
  if (ret_code == SpenchRetCode::SUCCESS ||
      ret_code == SpenchRetCode::CONTEXT_ACCUMULATION) {
    ret_code = ctools::AllocateAndCopy<float>(res, memory_out, out_size);
  }

  return ret_code;
}

SpenchRetCode   SpenchAudioProcessChunkInt(SpenchAudioProcess model,
                                           const u_int8_t* memory_in,
                                           u_int64_t in_size,
                                           u_int8_t** memory_out,
                                           u_int64_t* out_size) {
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  const int16_t* input_ptr = reinterpret_cast<const int16_t*> (memory_in);
  std::vector<int16_t> res;
  ((spench::SpenchAudioProcess*)model)->
                              ProcessChunk(std::vector<int16_t> (input_ptr, input_ptr + in_size / sizeof(int16_t)),
                                           res);
  if (ret_code == SpenchRetCode::SUCCESS ||
      ret_code == SpenchRetCode::CONTEXT_ACCUMULATION) {
    ret_code = ctools::AllocateAndCopy<int16_t>(res, memory_out, out_size);
  }

  return ret_code;
}
