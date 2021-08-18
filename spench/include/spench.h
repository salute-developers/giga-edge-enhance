#pragma once

#include <memory>
#include <vector>
#include <string>

struct DenoiseState;

namespace spench {

class AudioDenoise {
 public:
    void                            Reset();
    void                            Finalize() {return;};
    std::vector<float>              ProcessChunk(const std::vector<int16_t>);
    std::vector<float>              ProcessChunk(const std::vector<float>);

                                    AudioDenoise();
                                    ~AudioDenoise();
                                    AudioDenoise(const AudioDenoise& copy) = delete;
    AudioDenoise&                   operator=(const AudioDenoise& copy) = delete;
 private:
    DenoiseState*                   _hidden_state;
    const uint16_t                  _frame_size = 480;
    const std::vector<float>        _init_infer_input;
    void                            warmUp();
};


}  // namespace spench
