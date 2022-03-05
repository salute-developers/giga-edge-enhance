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
                                    AudioDenoise(const AudioDenoise& copy) = delete;
    AudioDenoise&                   operator=(const AudioDenoise& copy) = delete;
    SpenchRetCode                   ReloadConfig();
    uint32_t                        GetEngineThreads();
    uint64_t                        GetModelSampleRate();
    uint64_t                        GetModelInputFrameSize();

    // working with context
    DenoiseContext<float>*          CreateContext();
    uint64_t                        GetContextId(const DenoiseContext<float>* ctx);
    void                            SetContextId(DenoiseContext<float>* ctx,
                                                 uint64_t id);
    void                            DestroyContext(DenoiseContext<float>** ctx);

    // denoising
    SpenchRetCode                   ProcessChunk(DenoiseContext<float>* ctx,
                                                 const std::vector<float>&,
                                                 std::vector<float>&);

 private:
    DenoiseScheduler*               m_sheduler;
    void                            Finalize();
};

}  // namespace spench
