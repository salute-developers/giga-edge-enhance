#pragma once

#include <mutex> // NOLINT
#include <tuple>
#include <vector>
#include <valarray>
#include <functional>
#include "spench/src/windowing.h"
#include "spench/src/fft_calculator.h"
#include "spench/src/ifft_calculator.h"

namespace spench {
namespace fe {
// Computes Power Spectrum features for a speech signal.
template <typename T>
class FeatureExtractor {
 public:
  FeatureExtractor() = delete;
  FeatureExtractor& operator=(const FeatureExtractor&) = delete;
  FeatureExtractor(const FeatureExtractor&) = delete;
  explicit FeatureExtractor(const FeatureParams& params);
  virtual ~FeatureExtractor() = default;

  // input - input speech signal (T)
  virtual void STFTApply(const std::vector<T>& input,
                         std::vector<ComplexNumber<T>>&);
  // input - real and image vectors (T)
  virtual void iSTFTApply(const ComplexNumber<T>& input_spec,
                          std::vector<T>& output_signal);
  FeatureParams GetFeatureParams() const;

  // apply mask to real and imaginary parts
  // last 2 vectors (real, imag) of input_spec will be changed
  void calcEnhancedPartsInPlace(const ComplexNumber<T>& input_spec,
                                ComplexNumber<T>& predicted_mask);

 protected:
  void STFTImpl(const std::vector<T>& frames,
                std::vector<ComplexNumber<T>>& result);

  void iSTFTImpl(const ComplexNumber<T>& input_spec,
                 std::vector<T>& output_signal);

  void ValidatePowSpecParams() const;

  // process mask for real and imaginary parts
  // predicted_mask will be changed
  void decompressCIRMInPlace(ComplexNumber<T>& predicted_mask);

  std::valarray<T> boolToT(const std::valarray<bool> b);
  void apply_decompressCIRMInPlace(std::valarray<T>& mask_part);

  void lookupAdd(const std::vector<T>& signal,
                 std::vector<T>& output_signal);

  FeatureParams m_feat_params;
  Windowing<T> m_windowing;
  std::mutex m_fft_mutex;
  FftCalculator<T> m_fft_calculator;
  std::mutex m_ifft_mutex;
  iFftCalculator<T> m_ifft_calculator;
  const T m_cirmK = static_cast<T>(10.0);
  const T m_cirmLimit = static_cast<T>(9.9);
};

}  // namespace fe
}  // namespace spench
