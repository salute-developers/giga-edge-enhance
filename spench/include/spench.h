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
    // reread configuration form disk
    SpenchRetCode                   ReloadConfig();
    // modify configuration value of engine threads in memory
    // void                            SetEngineThreads(const uint32_t num_threads);
    uint32_t                        GetEngineThreads();
    // start with configuration in memory (cached)
    // int16_t                         ReloadCache();

    // working with context
    DenoiseContext<float>*          CreateContext();
    uint64_t                        GetContextId(const DenoiseContext<float>* ctx);
    void                            SetContextId(DenoiseContext<float>* ctx,
                                                 uint64_t id);
    void                            DestroyContext(DenoiseContext<float>** ctx);

    // denoising
    std::vector<float>              DownsamplePCM(const std::vector<float>& in,
                                                  uint64_t sample_rate_in,
                                                  uint64_t sample_rate_out);
    std::vector<float>              UpsamplePCM(const std::vector<float>& in,
                                                uint64_t sample_rate_in,
                                                uint64_t sample_rate_out);
    SpenchRetCode                   ProcessChunk(DenoiseContext<float>* ctx,
                                                 const std::vector<float>&,
                                                 std::vector<float>&);

 private:
    DenoiseScheduler*               m_sheduler;
    void                            Finalize();
};

namespace tools {
    void                            MakeFirstInputFrame(const std::vector<float>& current_chunk,
                                                        std::vector<float>& frame);
    std::vector<float>              DownsamplePCM(const std::vector<float>& in,
                                                  uint64_t sample_rate_in,
                                                  uint64_t sample_rate_out);
    std::vector<float>              UpsamplePCM(const std::vector<float>& in,
                                                uint64_t sample_rate_in,
                                                uint64_t sample_rate_out);
}  // namespace tools
}  // namespace spench
