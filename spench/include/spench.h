#pragma once

#include <memory>
#include <vector>
#include <string>

struct DenoiseState;

namespace spench {

class AudioDenoise {
 public:
    int16_t                         Reset();
    void                            ProcessChunk(const std::vector<int16_t>, std::vector<int16_t>&);
    void                            ProcessChunk(const std::vector<float>, std::vector<float>&);

                                    AudioDenoise();
                                    ~AudioDenoise();
                                    AudioDenoise(const AudioDenoise& copy) = delete;
    AudioDenoise&                   operator=(const AudioDenoise& copy) = delete;
 private:
    DenoiseState*                   _hidden_state;
    const uint16_t                  _frame_size = 480;
    const std::vector<float>        _init_infer_input;
    void                            warmUp();
    void                            Finalize();
    void                            convertToPCM16(const std::vector<float>& input, std::vector<int16_t>& output);
    void                            convertToFloat32(const std::vector<int16_t>& input, std::vector<float>& output);
};
}  // namespace spench
