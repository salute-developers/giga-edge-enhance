#include <memory>
#include <catch.hpp>
#include "include/spench_c_wrapper.h"
#include "tests/src/utils.h"

struct TestCAPI {
  std::vector<float> wav_data;
  SpenchWrapper denoiser;

  std::string base = "data/";

  TestCAPI() {}
  TestCAPI& SetWav(const std::string& wav_path) {
    std::string full_wav_path = base + "wavs/" + wav_path;
    wav_data = loadWavFx(full_wav_path.c_str());
    return *this;
  }

  TestCAPI& InitModel() {
    denoiser = (void *)(SpenchWrapperInit());
    return *this;
  }
};


TEST_CASE("Two Infers a tensors with ones with reset and via C API", "[spench]") {
  auto data = std::move(TestCAPI()
      .InitModel());
  std::vector<float> predictions;
  int16_t ones[480] = {32767};

  int32_t ret_size = 0;
  const std::vector<float>& reference_prediction =
                      LoadVectorFromBinaryFile<float, float>("data/results/ones_enhanced.raw");
  for (int16_t chunk = 0; chunk < 5; ++chunk) {
    unsigned char* ret = NULL;
    SpenchWrapperAddChunk(data.denoiser, (const unsigned char*)(*(&ones)), sizeof(ones) / sizeof(ones[0]), &ret, &ret_size);
    int16_t* pred = reinterpret_cast<int16_t*>(ret);
    predictions.insert(predictions.end(), pred, pred+ret_size/2);
    free(ret);
    ret = NULL;
  }

  // IsVectorsClose(reference_prediction, predictions, 0.001);

  SpenchWrapperReset(data.denoiser);
  predictions.clear();

  for (int16_t chunk = 0; chunk < 5; ++chunk) {
    unsigned char* ret = NULL;
    SpenchWrapperAddChunk(data.denoiser, (const unsigned char*)(*(&ones)), sizeof(ones) / sizeof(ones[0]), &ret, &ret_size);
    int16_t* pred = reinterpret_cast<int16_t*>(ret);
    predictions.insert(predictions.end(), pred, pred+ret_size/2);
    free(ret);
    ret = NULL;
  }
  // IsVectorsClose(reference_prediction, predictions, 0.001);

  SpenchWrapperFree(data.denoiser);
}
