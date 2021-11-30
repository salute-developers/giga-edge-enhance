#include "spench/feature_extractor.h"
#include "spench/feature_params.h"
#include <memory>
#include <catch.hpp>
#include <fftw3.h>
#include <chrono> // NOLINT

#include "tests/src/utils.h"

static spench::fe::FeatureParams DefaultSpeechFeatureParams16kHz() {
  spench::fe::FeatureParams params;

  params.sampling_freq = 16000;
  params.frame_size_ms = 32;
  params.frame_stride_ms = 16;
  params.low_freq_filterbank = 0;
  params.high_freq_filterbank = 8000;
  params.zero_mean_frame = false;
  params.window_type = spench::fe::WindowType::HANNING;
  params.normalize_gain = true;
  params.normalize_features_across_axis_0 = true;
  params.wav_scaling = false;
  return params;
}

struct TestFE {
  std::unique_ptr<spench::fe::FeatureExtractor<float>> m_feature_extractor;
  const std::size_t m_kNattempts = 1000;
  std::vector<float> m_wav_data;
  const std::string m_kBase = "data/";

  TestFE() {}

  TestFE& SetWav(const std::string& wav_path) {
    std::string full_wav_path = m_kBase + "wavs/" + wav_path;
    loadWavFx(full_wav_path.c_str(), m_wav_data);
    return *this;
  }

  TestFE& LoadDataFromBin(const std::string& dump,
                        std::vector<std::vector<float>>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    uint32_t step_size_samples = m_feature_extractor->GetFeatureParams().FilterFreqResponseLen();
    loadBin(full_wav_path.c_str(), step_size_samples, output);
    return *this;
  }
  TestFE& LoadDataFromBin(const std::string& dump,
                        std::vector<float>& output) {
    std::string full_wav_path = m_kBase + "binary/" + dump;
    loadBin(full_wav_path.c_str(), output);
    return *this;
  }
  TestFE& InitFE(const spench::fe::FeatureParams& params) {
    m_feature_extractor =
            std::unique_ptr<spench::fe::FeatureExtractor<float> >(new spench::fe::FeatureExtractor<float>(params));
    return *this;
  }
};


TEST_CASE("Run STFT with dummy zero vector (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().
                                     InitFE(params));

  const std::vector<float> kConst_input(512, 0.0);
  std::vector<float> reference_mag(257, 0.0);
  std::vector<float> reference_real(257, 0.0);
  std::vector<float> reference_imag(257, 0.0);
  std::vector<std::tuple<std::vector<float>,
                               std::vector<float>,
                               std::vector<float> > > result;
  feature_extractor.m_feature_extractor->STFTApply(kConst_input, result);
  REQUIRE(result.size() >= 1);
  std::vector<float>& result_mag = std::get<0>(result[0]);
  std::vector<float>& result_real = std::get<1>(result[0]);
  std::vector<float>& result_imag = std::get<2>(result[0]);
  IsVectorsClose(reference_mag, result_mag, 1.0);
  IsVectorsClose(reference_real, result_real, 1.0);
  IsVectorsClose(reference_imag, result_imag, 1.0);
}

TEST_CASE("Run 1000 STFT with dummy zero vector for speed measure (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().
                                     InitFE(params));
  const std::vector<float> kConst_input(512, 0.0);
  std::vector<size_t> timings(feature_extractor.m_kNattempts);

  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < feature_extractor.m_kNattempts; ++i) {
    std::vector<std::tuple<std::vector<float>,
                               std::vector<float>,
                               std::vector<float> > > result;
    feature_extractor.m_feature_extractor->STFTApply(kConst_input, result);
    REQUIRE(result.size() >= 1);
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();

  // t of STFT < 0.001 sec
  REQUIRE(static_cast<double>(tot) / (feature_extractor.m_kNattempts * 1000) < 0.001);

  std::cout << "1000 STFT calculations complete. Takes in total (sec): " << static_cast<double>(tot) / 1000
            << " | and average (sec): " << static_cast<double>(tot) / (feature_extractor.m_kNattempts * 1000)
            << std::endl;
}

TEST_CASE("Run STFT with single real vector (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().InitFE(params));
  std::vector<std::tuple<std::vector<float>,
                               std::vector<float>,
                               std::vector<float> > > result;
  const std::vector<float> kConst_input = {
       -0.00042725, -0.00036621, -0.00064087, -0.00048828, -0.00021362,
       -0.00039673, -0.00027466, -0.00018311, -0.0005188 , -0.00042725,
       -0.0005188 , -0.00057983, -0.00054932, -0.00054932, -0.00042725,
       -0.00042725, -0.0005188 , -0.00057983, -0.00054932, -0.0005188 ,
       -0.0005188 , -0.00045776, -0.00057983, -0.00061035, -0.00057983,
       -0.00085449, -0.00088501, -0.00085449, -0.0010376 , -0.00100708,
       -0.00094604, -0.00134277, -0.00137329, -0.00152588, -0.0017395 ,
       -0.00167847, -0.00158691, -0.00161743, -0.00183105, -0.00186157,
       -0.00192261, -0.00201416, -0.00192261, -0.00192261, -0.00183105,
       -0.0017395 , -0.00192261, -0.00189209, -0.00192261, -0.00204468,
       -0.0020752 , -0.00204468, -0.00186157, -0.00192261, -0.00189209,
       -0.00195312, -0.00201416, -0.0020752 , -0.00204468, -0.00210571,
       -0.00195312, -0.00189209, -0.0017395 , -0.00170898, -0.00201416,
       -0.00189209, -0.00183105, -0.00189209, -0.00167847, -0.00152588,
       -0.00146484, -0.00143433, -0.00134277, -0.00131226, -0.0012207 ,
       -0.00106812, -0.00106812, -0.00091553, -0.00079346, -0.00085449,
       -0.00085449, -0.0007019 , -0.00073242, -0.00057983, -0.00033569,
       -0.00039673, -0.00036621, -0.00027466, -0.00042725, -0.00033569,
       -0.00030518, -0.00018311, -0.00006104, -0.00015259,  0.00009155,
        0.0       , -0.00018311, -0.00003052,  0.00006104,  0.00006104,
       -0.00003052, -0.00003052,  0.00003052,  0.00009155,  0.00006104,
       -0.00003052,  0.00003052,  0.00012207,  0.00018311,  0.00018311,
        0.00018311,  0.00012207,  0.00018311,  0.00030518,  0.00030518,
        0.00039673,  0.00030518,  0.00033569,  0.00012207,  0.00015259,
        0.00033569,  0.00027466,  0.00027466,  0.00039673,  0.00030518,
        0.00036621,  0.00021362,  0.00033569,  0.0005188 ,  0.00045776,
        0.00036621,  0.00036621,  0.00045776,  0.00042725,  0.00033569,
        0.00039673,  0.00024414,  0.00036621,  0.0005188 ,  0.00036621,
        0.00061035,  0.00027466,  0.00009155,  0.00030518,  0.00009155,
        0.00009155,  0.00018311,  0.00009155,  0.00018311,  0.00006104,
       -0.00009155, -0.00024414, -0.00012207, -0.00024414, -0.00054932,
       -0.00048828, -0.00054932, -0.00064087, -0.00061035, -0.00094604,
       -0.00100708, -0.00088501, -0.00094604, -0.00109863, -0.0012207 ,
       -0.00109863, -0.00119019, -0.00134277, -0.00119019, -0.00115967,
       -0.00119019, -0.0010376 , -0.00140381, -0.00137329, -0.00125122,
       -0.00134277, -0.00125122, -0.00115967, -0.00119019, -0.0012207 ,
       -0.00125122, -0.00115967, -0.00115967, -0.00106812, -0.00094604,
       -0.00094604, -0.00097656, -0.00088501, -0.00073242, -0.00082397,
       -0.00054932, -0.00045776, -0.00073242, -0.00036621, -0.00030518,
       -0.00067139, -0.00045776, -0.00021362, -0.00042725, -0.00003052,
        0.00015259, -0.00018311,  0.00006104,  0.00006104, -0.00027466,
        0.0       ,  0.00018311,  0.00033569,  0.00045776,  0.00042725,
        0.00015259,  0.00012207, -0.00003052,  0.00003052,  0.00018311,
        0.00036621,  0.00045776,  0.00027466,  0.00018311,  0.00033569,
        0.00021362,  0.00021362,  0.00039673,  0.00024414,  0.0005188 ,
        0.00054932,  0.00030518,  0.00027466,  0.00012207,  0.00018311,
        0.00018311,  0.00021362,  0.00039673,  0.00027466,  0.00003052,
        0.00003052, -0.00015259, -0.00006104,  0.00012207,  0.00003052,
        0.00027466,  0.00024414,  0.00003052,  0.00003052,  0.00006104,
       -0.00006104,  0.00009155,  0.00024414,  0.00030518,  0.00042725,
        0.00036621,  0.00018311,  0.00009155,  0.00015259,  0.00018311,
        0.00054932,  0.0005188 ,  0.00054932,  0.00045776,  0.00012207,
        0.00021362,  0.00033569,  0.00018311,  0.00045776,  0.00054932,
        0.00042725,  0.00064087,  0.00048828,  0.00030518,  0.00054932,
        0.00064087,  0.00067139,  0.00067139,  0.00076294,  0.00073242,
        0.00076294,  0.00067139,  0.00057983,  0.00064087,  0.00073242,
        0.00079346,  0.00085449,  0.0007019 ,  0.00076294,  0.00076294,
        0.00064087,  0.00076294,  0.00076294,  0.00076294,  0.00094604,
        0.00073242,  0.00064087,  0.00067139,  0.00042725,  0.00054932,
        0.00076294,  0.0005188 ,  0.00067139,  0.00067139,  0.00042725,
        0.00076294,  0.00079346,  0.00064087,  0.00076294,  0.00079346,
        0.00097656,  0.00109863,  0.00112915,  0.00128174,  0.00128174,
        0.00125122,  0.00137329,  0.00140381,  0.00161743,  0.00201416,
        0.00180054,  0.00161743,  0.00170898,  0.0017395 ,  0.00204468,
        0.00231934,  0.00222778,  0.00204468,  0.00216675,  0.00234985,
        0.00216675,  0.00238037,  0.00253296,  0.00250244,  0.00262451,
        0.00256348,  0.00253296,  0.00283813,  0.00292969,  0.00280762,
        0.00268555,  0.00259399,  0.00256348,  0.00253296,  0.00247192,
        0.00244141,  0.00256348,  0.00241089,  0.00210571,  0.00210571,
        0.00210571,  0.0020752 ,  0.00219727,  0.00192261,  0.00170898,
        0.00186157,  0.00170898,  0.00146484,  0.00140381,  0.00134277,
        0.00137329,  0.00149536,  0.00112915,  0.00112915,  0.0012207 ,
        0.00082397,  0.00088501,  0.00097656,  0.00085449,  0.00112915,
        0.00106812,  0.00082397,  0.00094604,  0.00100708,  0.00082397,
        0.00085449,  0.00091553,  0.00109863,  0.00134277,  0.00115967,
        0.00125122,  0.00115967,  0.00125122,  0.00152588,  0.00137329,
        0.00149536,  0.0015564 ,  0.00170898,  0.00177002,  0.00152588,
        0.0015564 ,  0.00152588,  0.00161743,  0.0017395 ,  0.00158691,
        0.00170898,  0.00161743,  0.00143433,  0.00137329,  0.00143433,
        0.00140381,  0.00152588,  0.00140381,  0.00140381,  0.00143433,
        0.00125122,  0.00140381,  0.00100708,  0.00106812,  0.00137329,
        0.0012207 ,  0.00152588,  0.00115967,  0.00085449,  0.00106812,
        0.00076294,  0.00100708,  0.00119019,  0.00125122,  0.00149536,
        0.0012207 ,  0.0010376 ,  0.00088501,  0.00091553,  0.00128174,
        0.00140381,  0.00143433,  0.00152588,  0.00143433,  0.00109863,
        0.0012207 ,  0.00146484,  0.0012207 ,  0.00140381,  0.00146484,
        0.00128174,  0.00149536,  0.00137329,  0.00109863,  0.00100708,
        0.00112915,  0.00125122,  0.00143433,  0.00149536,  0.00112915,
        0.00076294,  0.00064087,  0.00045776,  0.00042725,  0.0007019 ,
        0.00073242,  0.00057983,  0.00042725,  0.00015259,  0.00018311,
        0.00015259,  0.00027466,  0.00036621,  0.00042725,  0.00045776,
        0.00024414,  0.0        ,  0.00003052, -0.00009155,  0.00009155,
        0.00012207,  0.00006104,  0.00012207, -0.00009155, -0.00030518,
       -0.00024414, -0.00045776, -0.00033569, -0.00030518, -0.00061035,
       -0.00045776, -0.0007019 , -0.00082397, -0.00057983, -0.00079346,
       -0.00082397, -0.00067139, -0.00067139, -0.00082397, -0.00064087,
       -0.00057983, -0.00082397, -0.00082397, -0.00073242, -0.00091553,
       -0.0007019 , -0.00064087, -0.00097656, -0.00106812, -0.00106812,
       -0.00128174, -0.00112915, -0.00091553, -0.0012207 , -0.00125122,
       -0.00115967, -0.00137329, -0.00134277, -0.0010376 , -0.00134277,
       -0.00112915, -0.00109863, -0.00161743, -0.00109863, -0.00112915,
       -0.00137329, -0.00091553, -0.0010376 , -0.00085449, -0.00027466,
       -0.0005188 , -0.00076294
  };
  // Retrieved via python torch::stft
  std::vector<float> reference_real = {
         1.2922e-01, -7.3843e-02, -2.1597e-02,  1.7163e-02,  3.9733e-03,
         2.3370e-02, -8.8910e-03, -8.9269e-03,  1.1204e-02, -3.1297e-03,
        -1.3144e-02,  9.2553e-03,  1.9495e-03, -3.7023e-04, -1.2953e-03,
        -1.7583e-03,  1.2062e-03, -2.5298e-03,  4.3211e-03, -3.2442e-03,
         3.2992e-03, -5.4111e-04, -1.8072e-03,  2.3228e-03, -1.9234e-03,
         4.8100e-04, -1.1617e-03,  1.1200e-03,  9.3947e-04, -1.4159e-03,
         6.1624e-04, -1.3967e-03,  1.9189e-03, -1.5706e-03,  1.2441e-03,
        -2.4955e-04, -1.3228e-04,  4.4764e-04, -2.1437e-04, -1.1875e-03,
         8.7740e-04,  2.4808e-07,  2.5216e-04,  7.4806e-05, -2.7243e-04,
        -1.7708e-04,  2.9380e-04, -8.8008e-04,  8.2991e-04,  8.3054e-04,
        -1.4303e-03,  1.7099e-04,  1.1843e-03, -1.8009e-03,  1.2188e-03,
        -5.4517e-04,  5.1064e-04,  7.5981e-05, -7.7788e-04,  7.6925e-04,
         8.7065e-04, -5.0854e-03,  7.3341e-03, -4.2373e-03,  4.5122e-03,
        -8.6516e-03,  5.4552e-03, -5.5501e-04, -1.3592e-04,  9.8046e-04,
        -2.9679e-04, -1.8231e-03,  2.5011e-03, -1.2079e-03, -3.8792e-04,
         5.0806e-04, -5.8295e-04, -4.2621e-05,  1.9689e-03, -2.2889e-03,
         1.3586e-03, -6.9579e-04, -4.9092e-04,  7.6389e-04,  8.7186e-04,
        -7.0346e-04,  1.3171e-04, -1.6584e-03,  1.8161e-03, -7.7704e-04,
         4.7242e-04, -2.1169e-04, -4.1791e-05,  4.4460e-04, -1.3444e-03,
         6.5623e-04,  9.9971e-04, -9.6498e-04, -4.6348e-05,  2.6297e-04,
         3.0072e-04, -1.4915e-04, -2.8289e-04, -3.3372e-04,  1.2470e-03,
        -1.3196e-03,  8.2067e-04, -6.4969e-04,  8.0267e-04, -3.8427e-04,
        -8.4968e-04,  1.8983e-03, -4.0983e-04, -3.3578e-03,  3.7926e-03,
        -3.5819e-04, -1.4878e-03,  1.1951e-03, -5.4330e-04,  9.2239e-05,
        -4.2407e-04,  1.4256e-03, -1.8639e-03,  3.2843e-04,  1.0651e-03,
        -5.4819e-04, -6.8477e-05,  6.7395e-05,  1.8654e-04, -4.9366e-04,
         1.3649e-04,  4.0297e-04,  5.9775e-05, -2.8649e-04, -8.8595e-05,
        -1.7079e-04,  5.4629e-04, -3.3343e-04, -2.7505e-04,  3.1057e-04,
         8.4078e-05,  1.9521e-04, -5.0539e-04,  4.8355e-04, -2.8611e-04,
        -2.2041e-04,  3.9434e-04, -1.8777e-04,  1.6692e-04, -2.3979e-04,
         5.0745e-04, -8.0056e-04,  8.0129e-05,  7.5054e-04, -5.7298e-04,
         1.3834e-04, -3.6266e-05, -3.4371e-05, -1.4402e-04,  9.3486e-04,
        -1.0222e-03,  1.0465e-03, -2.9812e-04, -1.4050e-03,  1.0685e-03,
         4.2822e-04, -1.1328e-03,  8.3568e-04, -2.3064e-04, -2.1929e-04,
        -9.4358e-04,  1.2780e-03,  6.0827e-04, -1.5721e-03,  1.6551e-03,
         6.2931e-05, -2.6860e-03,  3.4611e-03, -2.7384e-03,  1.7495e-03,
         4.4793e-05, -9.8921e-04, -4.1401e-04,  6.4171e-04,  2.1029e-04,
         4.8253e-04, -9.3728e-04,  4.7456e-04, -5.4884e-04,  7.7165e-04,
        -1.5659e-04,  5.0485e-04, -1.4669e-03, -3.4268e-04,  1.8158e-03,
        -6.7849e-04,  2.5691e-04, -8.2088e-04,  1.5647e-03, -1.0194e-03,
        -4.5943e-04,  5.7370e-04, -4.4238e-04, -2.4114e-04,  8.8595e-04,
         1.5641e-05, -9.2084e-04,  3.5256e-04,  5.5005e-04, -4.2757e-04,
         5.9154e-04, -1.0007e-03,  1.8855e-04,  5.8956e-04, -2.1145e-04,
         3.5389e-04, -1.1452e-03,  3.4558e-04,  7.9261e-04, -4.7492e-04,
        -1.4132e-04,  2.9779e-04, -9.1893e-05,  2.2285e-04, -3.6407e-04,
        -6.2280e-06,  1.2078e-04, -5.8279e-05,  2.5616e-04, -1.7312e-04,
        -1.3869e-04,  3.4671e-04, -5.4887e-04,  5.1964e-04, -3.7159e-04,
         3.2887e-04, -2.8490e-04,  4.2559e-04, -5.2903e-04,  2.5011e-04,
         1.7013e-04, -3.0227e-04,  1.1989e-04,  1.5937e-04, -5.0368e-04,
         4.7660e-04, -4.2383e-05, -1.0811e-04, -1.1411e-04,  1.8762e-04,
        -1.9653e-05, -1.2514e-04, -1.2684e-05,  3.1024e-04, -3.7877e-04,
         4.8135e-04, -6.3273e-04
  };
  // Retrieved via python torch::stft
  std::vector<float> reference_imag = {
         0.0000e+00,  1.4091e-01, -9.8495e-02,  2.9643e-02,  3.1992e-02,
        -6.3491e-02,  2.3033e-02,  1.9509e-02, -4.1818e-02,  3.1990e-02,
        -8.8667e-04, -1.1619e-02,  2.8069e-03,  3.0596e-03,  1.2962e-03,
        -4.2530e-03,  3.2269e-03, -1.1805e-03,  7.5946e-04, -1.8979e-03,
         1.6704e-03,  3.5525e-04, -3.5930e-05, -1.2105e-03,  2.2767e-03,
        -2.9492e-03,  2.2356e-03, -7.8776e-04, -1.3492e-03,  1.5818e-03,
         1.0527e-04, -9.8890e-04,  1.0618e-03, -8.1539e-04, -3.2490e-04,
         7.8849e-04,  9.6692e-05, -1.6007e-04, -1.9127e-04,  1.5918e-04,
        -2.5536e-04,  2.9267e-04, -5.0121e-04,  2.7672e-04,  1.0557e-03,
        -1.6170e-03,  5.3116e-04,  2.2158e-04,  1.7704e-04,  1.1898e-04,
        -8.0275e-04,  1.0914e-03, -1.7435e-03,  1.0583e-03,  7.0675e-04,
        -8.4912e-04, -7.6892e-05,  4.7256e-04,  8.3749e-05,  8.5352e-04,
        -2.2393e-03,  2.0624e-03, -5.3241e-03,  5.4409e-03, -3.2846e-04,
         9.0736e-04, -2.8301e-04, -2.7079e-03,  1.0532e-03, -3.3991e-04,
         9.1374e-04,  2.2391e-04, -9.8548e-04,  1.2544e-03, -1.0474e-03,
         2.3982e-04,  7.7571e-04, -2.9056e-03,  2.6332e-03, -2.1502e-04,
        -8.0625e-04,  2.3878e-03, -4.0372e-03,  2.3560e-03,  6.7201e-04,
        -1.2842e-03,  1.0902e-03, -9.8993e-04,  7.4923e-04, -7.2019e-04,
         2.6390e-04, -8.0093e-05,  1.0166e-04,  3.0860e-04,  1.9476e-05,
        -1.0430e-03,  3.7098e-04,  9.0463e-04, -2.9266e-04, -4.1971e-04,
         6.6035e-04, -1.4821e-03,  1.5018e-03, -8.3781e-04,  1.1003e-03,
        -1.1099e-03, -2.8553e-04,  1.3507e-03, -8.3677e-04,  2.2846e-04,
        -3.0278e-04,  1.8922e-05,  5.8810e-04, -1.1610e-04, -1.3373e-03,
         1.5248e-03, -5.0259e-04, -1.7704e-04,  4.7711e-04, -2.8792e-04,
        -1.5954e-04,  2.1985e-04, -9.2930e-04,  1.6000e-03, -6.0648e-04,
        -3.7426e-04,  6.4779e-05,  5.2171e-04, -4.5164e-04,  2.6420e-04,
        -4.4735e-04,  1.0739e-04,  5.6044e-04, -7.6477e-04,  3.6195e-04,
         2.6125e-04, -5.2692e-05, -6.2378e-04,  4.7061e-04,  1.1452e-04,
         4.4473e-04, -6.5760e-04, -1.0595e-04, -2.7173e-04,  4.2772e-04,
         3.9200e-04, -7.3204e-04,  5.6363e-04, -1.0690e-04, -3.0019e-04,
         1.8800e-04,  8.0663e-04, -1.5391e-03,  1.1626e-03, -8.5576e-04,
         4.2325e-04,  5.7503e-04, -9.4186e-04,  1.0409e-03, -7.4763e-04,
        -2.2375e-03,  4.6368e-03, -3.4822e-03,  1.9372e-03,  3.8483e-06,
        -2.1306e-03,  2.0247e-03, -9.8968e-04,  7.8321e-04,  6.2729e-05,
        -1.1923e-03,  1.2140e-03,  1.4355e-04, -2.6802e-03,  2.7311e-03,
        -5.6188e-04, -6.4470e-04,  8.5999e-05,  9.9531e-04, -8.5599e-04,
         4.2429e-04, -1.5403e-03,  4.0311e-03, -5.2302e-03,  3.0873e-03,
        -3.2327e-04, -6.3743e-04,  9.6833e-04, -1.1769e-03,  1.3985e-03,
        -1.5154e-03,  1.4249e-03,  1.1498e-04, -1.9455e-03,  1.4571e-03,
        -7.4187e-04,  1.4038e-03, -1.2153e-03, -4.1392e-04, -1.8924e-04,
         2.7122e-03, -1.9038e-03, -6.7103e-04,  4.5709e-04,  7.6973e-04,
        -2.5138e-04, -2.7948e-04,  4.2102e-05, -1.7353e-04, -9.1273e-05,
         1.3054e-04,  7.5059e-04, -4.4443e-04, -8.1976e-04,  5.1277e-04,
         5.3665e-04, -3.5271e-04, -2.7915e-04,  4.1912e-04, -9.0744e-05,
        -2.5630e-04,  3.7716e-06,  1.5390e-04,  1.6672e-04, -2.7558e-04,
         1.8277e-05,  1.5299e-04, -1.4907e-04,  3.7027e-04, -6.1302e-04,
         5.9743e-04, -7.0202e-04,  6.3894e-04, -3.7823e-04,  1.9884e-04,
        -1.6167e-04,  2.3287e-04,  1.0179e-04, -2.2287e-04, -3.6667e-05,
         8.6743e-05,  6.9726e-05, -8.8085e-05, -3.7007e-04,  3.7584e-04,
         2.3769e-04, -3.3803e-04,  7.5349e-05, -1.2613e-04,  4.4333e-04,
        -6.3304e-04,  2.8680e-04, -3.5757e-05,  7.5996e-07,  3.2257e-04,
        -3.1243e-04,  0.0000e+00
  };
  feature_extractor.m_feature_extractor->STFTApply(kConst_input, result);
  REQUIRE(result.size() >= 1);
  std::vector<float>& result_real = std::get<1>(result[0]);
  std::vector<float>& result_imag = std::get<2>(result[0]);

  IsVectorsClose(reference_real, result_real, 0.000005);
  IsVectorsClose(reference_imag, result_imag, 0.000005);
}

TEST_CASE("Run 10 STFT calculations with data from .wav file (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().
                                     InitFE(params).
                                     SetWav("rec_noise_001_16k.wav"));

  std::vector<std::vector<float>> reference_mag;
  std::vector<std::vector<float>> result_mag;
  std::vector<std::tuple<std::vector<float>,
                               std::vector<float>,
                               std::vector<float> > > result;
  size_t iterations = 11;

  feature_extractor.LoadDataFromBin("10_mag_stft_tensors.dump", reference_mag);

  REQUIRE(reference_mag.size() == iterations - 1);

  std::vector<float> audio(feature_extractor.m_wav_data.begin(),
                           feature_extractor.m_wav_data.begin()+ 256 * iterations);

  feature_extractor.m_feature_extractor->STFTApply(audio, result);
  for (size_t i = 0; i < iterations - 1 ; ++i) {
    std::vector<float>& mag = std::get<0>(result[i]);
    result_mag.push_back(mag);
  }

  REQUIRE(reference_mag.size() == result_mag.size());

  for (uint32_t i = 0; i < reference_mag.size(); ++i) {
    IsVectorsClose(reference_mag[i], result_mag[i], 0.0001);
  }
}

TEST_CASE("Run iSTFT with dummy complex zero vector (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().InitFE(params));
  // To get 256 samples we need to provide more for context
  const std::vector<float> kReference_signal(256, 0.0);
  const std::vector<float> kReal_input(257 * 2, 0.0);
  const std::vector<float> kImag_input(257 * 2, 0.0);

  std::vector<float> result;
  auto kInput_complex = std::make_tuple(kReal_input, kImag_input);
  feature_extractor.m_feature_extractor->iSTFTApply(kInput_complex, result);
  IsVectorsClose(kReference_signal, result, 0.0001);
}

TEST_CASE("Run 1000 iSTFT with dummy zero vector for speed measure (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().InitFE(params));
  // To get 256 samples we need to provide more for context
  const std::vector<float> kReference_signal(256, 0.0);
  const std::vector<float> kReal_input(257 * 2, 0.0);
  const std::vector<float> kImag_input(257 * 2, 0.0);

  auto kInput_complex = std::make_tuple(kReal_input, kImag_input);
  std::vector<size_t> timings(feature_extractor.m_kNattempts);

  auto ts_1 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < feature_extractor.m_kNattempts; ++i) {
    std::vector<float> result;
    feature_extractor.m_feature_extractor->iSTFTApply(kInput_complex, result);
    REQUIRE(result.size() == 256);
  }
  auto ts_2 = std::chrono::steady_clock::now();
  auto tot =  std::chrono::duration_cast<std::chrono::milliseconds>(ts_2 - ts_1).count();

  // t of STFT < 0.001 sec
  REQUIRE(static_cast<double>(tot) / (feature_extractor.m_kNattempts * 1000) < 0.001);

  std::cout << "1000 iSTFT calculations complete. Takes in total (sec): " << static_cast<double>(tot) / 1000
            << " | and average (sec): " << static_cast<double>(tot) / (feature_extractor.m_kNattempts * 1000)
            << std::endl;
}

TEST_CASE("Run 10 iSTFT calculations with real data from .bin file (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().
                                     InitFE(params).
                                     SetWav("rec_noise_001_16k.wav"));
  constexpr size_t iterations = 10 + 1;  // last one for the right context
  std::vector<float> result_signal;
  std::vector<float> input_real;
  std::vector<float> input_imag;

  std::vector<float> reference_signal;
  std::vector<std::tuple<std::vector<float>,
                               std::vector<float>,
                               std::vector<float> > > result;

  feature_extractor.LoadDataFromBin("11_real_stft_tensors.dump", input_real);
  feature_extractor.LoadDataFromBin("11_imag_stft_tensors.dump", input_imag);
  REQUIRE(input_real.size() == iterations * 257);
  REQUIRE(input_imag.size() == iterations * 257);
  auto input_complex = std::make_tuple(input_real, input_imag);

  feature_extractor.LoadDataFromBin("restored_istft_signal.dump", reference_signal);
  REQUIRE(reference_signal.size() == 256 * (iterations - 1));

  feature_extractor.m_feature_extractor->iSTFTApply(input_complex, result_signal);
  if (result_signal.size() > 256 * (iterations - 1)) {
    result_signal.resize(256 * (iterations - 1));
  }

  IsVectorsClose(reference_signal, result_signal, 0.0001);
}

TEST_CASE("Run iSTFT with single real complex vector (audio 16kHz) [spench]") {
  spench::fe::FeatureParams params = DefaultSpeechFeatureParams16kHz();
  auto feature_extractor = std::move(TestFE().InitFE(params));
  constexpr size_t iterations = 1 + 1;  // last one for the right context

  std::vector<float> result_signal;
  std::vector<float> input_real;
  std::vector<float> input_imag;

  std::vector<float> reference_signal;
  std::vector<std::tuple<std::vector<float>,
                               std::vector<float>,
                               std::vector<float> > > result;

  feature_extractor.LoadDataFromBin("11_real_stft_tensors.dump", input_real);
  feature_extractor.LoadDataFromBin("11_imag_stft_tensors.dump", input_imag);
  REQUIRE(input_real.size() > iterations * 257);
  REQUIRE(input_imag.size() > iterations * 257);

  input_real.resize(iterations * 257);
  input_imag.resize(iterations * 257);
  auto input_complex = std::make_tuple(input_real, input_imag);

  feature_extractor.LoadDataFromBin("restored_istft_signal.dump", reference_signal);
  REQUIRE(reference_signal.size() > 256 * (iterations - 1));
  reference_signal.resize(256 * (iterations - 1));

  feature_extractor.m_feature_extractor->iSTFTApply(input_complex, result_signal);

  IsVectorsClose(reference_signal, result_signal, 0.0001);
}
