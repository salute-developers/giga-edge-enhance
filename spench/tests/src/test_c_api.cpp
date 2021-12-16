#include <memory>
#include <catch.hpp>
#include "include/spench_c_wrapper.h"
#include "tests/src/utils.h"
#include <chrono> // NOLINT

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


TEST_CASE("Run inference twise with const values and with reset via C API", "[spench]") {
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
                          sizeof(const_input),
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
                          sizeof(const_input),
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


TEST_CASE("Run inference with invalid chunk size via C API", "[spench]") {
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
                          sizeof(const_input1),
                          &ret,
                          &ret_size);
  REQUIRE(ret_code == SPENCH_RET_ERR_INVALID_CHUNK_SIZE);

  SpenchWrapperReset(data.denoiser);
  ret_code = SpenchWrapperProcessChunk(data.denoiser,
                          (const unsigned char*) const_input2,
                          sizeof(const_input2),
                          &ret,
                          &ret_size);
  REQUIRE(ret_code == SPENCH_RET_ERR_INVALID_CHUNK_SIZE);
  SpenchWrapperFree(data.denoiser);
}

TEST_CASE("Run inference on wav and compare inference time with threshold", "[spench]") {
  auto data = std::move(TestCAPI()
    .InitModel().SetWav("test1_48_noised.wav"));

  // average and max inference time in nano seconds for 10ms chunk
  float avg_upperbound = 1.0f;  // ms
  float max_upperbound = 2.0f;  // ms

  uint16_t chunk_size = 10;
  auto chunks = chunkWav<int16_t>(data.wav_data, 48000, chunk_size, chunk_size);
  unsigned char* ret = NULL;
  int32_t ret_size = 0;

  std::vector<uint32_t> stat;

  for (auto& chunk : chunks) {
    auto start = std::chrono::high_resolution_clock::now();
    SpenchWrapperProcessChunk(data.denoiser,
                            (const unsigned char*) chunk.data(),
                            chunk.size() * 2,
                            &ret,
                            &ret_size);
    auto end = std::chrono::high_resolution_clock::now();
    stat.push_back((end - start).count());
  }

  uint32_t sum = std::accumulate(stat.begin(), stat.end(), 0);
  float avg_meausred = static_cast<float>(sum / stat.size()) / 1000000;
  float max_meausred = static_cast<float>(*max_element(stat.begin(), stat.end())) / 1000000;

  std::cout << "avg_measured: " << avg_meausred << " ms" << std::endl;
  std::cout << "max_measured: " << max_meausred << " ms" << std::endl;

  REQUIRE(avg_upperbound > avg_meausred);
  REQUIRE(max_upperbound > max_meausred);

  SpenchWrapperFree(data.denoiser);
}
