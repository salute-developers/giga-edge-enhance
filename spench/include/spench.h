#pragma once

#include <memory>
#include <vector>
#include <string>
#include "spench/include/errors.h"

namespace spench {
class DenoiseScheduler;
template<typename T>
class DenoiseContext;
class AudioDenoise {
 public:
  explicit                        AudioDenoise(const char* config_file);
                                  ~AudioDenoise();
                                  AudioDenoise(const AudioDenoise&) = delete;
  AudioDenoise&                   operator=(const AudioDenoise&) = delete;
  SpenchRetCode                   ReloadConfig();
  uint32_t                        GetEngineThreads();
  uint64_t                        GetModelSampleRate();
  uint64_t                        GetModelInputFrameSize();

  // working with context
  DenoiseContext<float>*          CreateContext();
  uint64_t                        GetContextId(const DenoiseContext<float>*);
  void                            SetContextId(DenoiseContext<float>*,
                                               uint64_t);
  void                            DestroyContext(DenoiseContext<float>**);

  // denoising
  SpenchRetCode                   ProcessChunk(DenoiseContext<float>*,
                                               const std::vector<float>&,
                                               std::vector<float>&);
  SpenchRetCode                   ProcessChunk(DenoiseContext<float>*,
                                               const std::vector<int16_t>&,
                                               std::vector<int16_t>&);
  SpenchRetCode                   GetFinalChunk(DenoiseContext<float>* ctx,
                                                std::vector<float>& ret);
  SpenchRetCode                   GetFinalChunk(DenoiseContext<float>* ctx,
                                                std::vector<int16_t>& ret16);

 private:
  DenoiseScheduler*               m_sheduler;
};

namespace tools {
void                              convertFloat32ToPCM16(const std::vector<float>&,
                                                        std::vector<int16_t>&);

void                              convertPCM16ToFloat32(const std::vector<int16_t>&,
                                                        std::vector<float>&);
}
}  // namespace spench
