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
    std::vector<int16_t>            ProcessChunk(const std::vector<int16_t>);
    std::vector<float>              ProcessChunk(const std::vector<float>);

    // void                            Load(const std::string& filename);
    // void                            LoadRaw(const std::string& filename, SoundInfo info);
    // template <typename T>
    // void                            LoadRaw(std::vector<T>& raw_vector, SoundInfo info);
    // void                            LoadRaw(std::ifstream& f, SoundInfo info);
    // void                            Save(const std::string& filename);
    // void                            SaveRaw(const std::string& filename);
    // void                            SaveRaw(std::ofstream& f);
    // void                            Resample(const size_t sample_rate);
    // void                            Convert(SoundInfo& dst);
    // const SoundInfo&                GetInfo();
    // template <typename T>
    // std::vector<T>                  GetRawBytes();
                                    AudioDenoise();
                                    ~AudioDenoise();
                                    AudioDenoise(const AudioDenoise& copy) = delete;
    AudioDenoise&                   operator=(const AudioDenoise& copy) = delete;
 private:
    DenoiseState*                   _hidden_state;
    const uint16_t                  _frame_size = 480;
};


}  // namespace spench
