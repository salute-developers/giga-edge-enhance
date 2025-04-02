#include <dsplib.h>
#include "spench/util/metrics/mute_frame_detector.h"
#include <catch.hpp>
#include <chrono> // NOLINT
#include <algorithm>

#include "tests/src/utils.h"

struct TestDspLibFeat {
  TestDspLibFeat() {}
  TestDspLibFeat& operator=(const TestDspLibFeat&) = delete;
  TestDspLibFeat(const TestDspLibFeat&) = delete;
  TestDspLibFeat(TestDspLibFeat&& orig) {}

  TestDspLibFeat& InitDspLib() {
    return *this;
  }
};

TEST_CASE("SUCCEED Run MedianFilter [dsplib]") {
  auto dsplib = std::move(TestDspLibFeat().
                          InitDspLib());
  (void) dsplib;
  auto t = dsplib::arange(1000) / 16000;
  dsplib::arr_real x = dsplib::sin(2 * dsplib::pi * 440 * t);
  x[20] = 10;
  x[30] = -10;

  dsplib::MedianFilter flt(3);
  auto y = flt(x);


  IsVectorsClose(std::vector<float>({max(x)}),
                 std::vector<float>({10.0}),
                 1e-7);
  IsVectorsClose(std::vector<float>({min(x)}),
                 std::vector<float>({-10.0}),
                 1e-7);

  IsVectorsClose(std::vector<float>({max(y)}),
                 std::vector<float>({1.0}),
                 1e-2);
  IsVectorsClose(std::vector<float>({min(y)}),
                 std::vector<float>({-1.0}),
                 1e-2);
}

TEST_CASE("SUCCEED Run Auto Gain Controller [dsplib]") {
  auto dsplib = std::move(TestDspLibFeat().
                          InitDspLib());
  (void) dsplib;
  // target level = 1 db
  // max gain = 30 db
  auto agc = dsplib::Agc(1, 30, 1000, 0.01);
  auto t = dsplib::arange(10000) / 8000;
  auto x = 10 * expj(2 * dsplib::pi * 440 * t);
  // std::cout << x << std::endl;
  auto [y, g] = agc(x);

  IsVectorsClose(std::vector<float>({max(dsplib::real(x))}),
                 std::vector<float>({10.0}),
                 1e-7);

  IsVectorsClose(std::vector<float>({min(dsplib::real(x))}),
                 std::vector<float>({-10.0}),
                 1e-7);

  REQUIRE(max(dsplib::real(y)) > 10.0);
  REQUIRE(max(dsplib::real(y)) < 30.0);
}

TEST_CASE("SUCCEED Run Noize Gate [dsplib]") {
  auto dsplib = std::move(TestDspLibFeat().
                          InitDspLib());
  (void) dsplib;
  const int fs = 48000;
  auto x1 = dsplib::arange(1000, 1480, 1) / 6000;

  std::vector<float> equal_0;
  auto ng1 = dsplib::NoiseGate(fs, -15);
  auto [y1, g1] = ng1(x1);

  std::copy_if(std::begin(x1), std::end(x1),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });

  REQUIRE(equal_0.size() == 0);
  equal_0.resize(0);
  std::copy_if(std::begin(y1), std::end(y1),
               std::back_inserter(equal_0),
               [](float& i) { return std::fabs(i) == 0.0; });
  REQUIRE(equal_0.size() != 0);
}

TEST_CASE("SUCCEED Run Dynamic Range Compressor [dsplib]") {
  auto dsplib = std::move(TestDspLibFeat().
                          InitDspLib());
  (void) dsplib;
  const int fs = 48000;
  auto x1 = dsplib::arange(1000, 1480, 1) / 1500;

  auto drc1 = dsplib::Compressor(fs, -15);
  auto [y1, g1] = drc1(x1);
  std::vector<float> greater_than_0_75;
  std::copy_if(std::begin(x1), std::end(x1),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });

  REQUIRE(greater_than_0_75.size() != 0);
  greater_than_0_75.resize(0);

  std::copy_if(std::begin(y1), std::end(y1),
               std::back_inserter(greater_than_0_75),
               [](float& i) { return std::fabs(i) > 0.75; });
  REQUIRE(greater_than_0_75.size() == 0);
}

TEST_CASE("SUCCEED Run frame detector and find out 2 gap frames [metric]") {
  const uint32_t sample_rate = 48000;
  constexpr uint32_t chunk_len = uint32_t(0.016 * sample_rate);
  util::metrics::MuteFrameDetector dtc(chunk_len);

  std::vector<float> normal_chunk(chunk_len, 0.65);
  std::vector<float> gap_chunk(chunk_len, 0.01);
  std::vector<float> audio = normal_chunk;
  std::vector<uint32_t> detection_pattern(15, 1);
  // add gaps
  audio.insert(audio.end(), gap_chunk.begin(), gap_chunk.end() );
  // add 2 normal signals again
  audio.insert(audio.end(), normal_chunk.begin(), normal_chunk.end() );
  audio.insert(audio.end(), normal_chunk.begin(), normal_chunk.end() );

  auto detector_out = dtc.process(audio);

  bool detected = false;
  detected = std::search(detector_out.begin(), detector_out.end(),
                         detection_pattern.begin(), detection_pattern.end())
                                                                            != audio.end();

  REQUIRE(detected == true);
}
