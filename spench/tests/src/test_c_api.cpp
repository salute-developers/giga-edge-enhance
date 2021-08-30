#include <memory>
#include <catch.hpp>
#include "include/spench_c_wrapper.h"
#include "tests/src/utils.h"

struct TestCAPI {
  std::vector<int16_t> wav_data;
  SpenchWrapper denoiser;

  std::string base = "data/";

  TestCAPI() {}
  TestCAPI& SetWav(const std::string& wav_path) {
    std::string full_wav_path = base + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), wav_data);
    return *this;
  }

  TestCAPI& InitModel() {
    denoiser = reinterpret_cast<void *>(SpenchWrapperInit());
    return *this;
  }
};


TEST_CASE("Run inferense twise with const values and with reset via C API", "[spench]") {
  auto data = std::move(TestCAPI()
      .InitModel());
  std::vector<int16_t> predictions;
  int16_t const_input[480];
  std::fill_n(const_input, 480, 300);
  int32_t ret_size = 0;
  int16_t ret_code = 0;
  const std::vector<int16_t>& reference_prediction =
                      LoadVectorFromBinaryFile<int16_t, int16_t>("data/results/dummy_enhanced.raw");
  for (int16_t chunk = 0; chunk < 5; ++chunk) {
    unsigned char* ret = NULL;
    ret_code = SpenchWrapperProcessChunk(data.denoiser,
                          (const unsigned char*) const_input,
                          sizeof(const_input) / sizeof(const_input[0]),
                          &ret,
                          &ret_size);
    REQUIRE(ret_code == SPENCH_RET_SUC);
    int16_t* pred = reinterpret_cast<int16_t*>(ret);
    std::vector<int16_t>  tmp_preds(pred, pred + ret_size / 2);
    predictions.insert(predictions.end(), pred, pred + ret_size / 2);
    free(ret);
    ret = NULL;
  }

  IsVectorsClose(reference_prediction, predictions, 1.0);

  SpenchWrapperReset(data.denoiser);
  predictions.clear();

  for (int16_t chunk = 0; chunk < 5; ++chunk) {
    unsigned char* ret = NULL;
    ret_code = SpenchWrapperProcessChunk(data.denoiser,
                          (const unsigned char*) const_input,
                          sizeof(const_input) / sizeof(const_input[0]),
                          &ret,
                          &ret_size);
    REQUIRE(ret_code == SPENCH_RET_SUC);
    int16_t* pred = reinterpret_cast<int16_t*>(ret);
    predictions.insert(predictions.end(), pred, pred + ret_size / 2);
    free(ret);
    ret = NULL;
  }
  IsVectorsClose(reference_prediction, predictions, 1.0);

  SpenchWrapperFree(data.denoiser);
}


TEST_CASE("Run inferense with invalid chunk size via C API", "[spench]") {
  auto data = std::move(TestCAPI()
      .InitModel());
  int16_t const_input1[481];
  int16_t const_input2[479];
  std::fill_n(const_input1, 481, 0);
  std::fill_n(const_input2, 479, 0);

  unsigned char* ret = NULL;
  int16_t ret_code = 0;
  REQUIRE(ret == NULL);

  int32_t ret_size = 0;
  ret_code = SpenchWrapperProcessChunk(data.denoiser,
                          (const unsigned char*) const_input1,
                          sizeof(const_input1) / sizeof(const_input1[0]),
                          &ret,
                          &ret_size);
  REQUIRE(ret_code == SPENCH_RET_ERR_INVALID_CHUNK_SIZE);

  SpenchWrapperReset(data.denoiser);
  ret_code = SpenchWrapperProcessChunk(data.denoiser,
                          (const unsigned char*) const_input2,
                          sizeof(const_input2) / sizeof(const_input2[0]),
                          &ret,
                          &ret_size);
  REQUIRE(ret_code == SPENCH_RET_ERR_INVALID_CHUNK_SIZE);
  SpenchWrapperFree(data.denoiser);
}
