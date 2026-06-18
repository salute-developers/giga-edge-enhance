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
  virtual void stft_apply(const std::vector<T>& input,
                         std::vector<ComplexNumber<T>>&);
  // input - real and image vectors (T)
  virtual void istft_apply(const ComplexNumber<T>& input_spec,
                          std::vector<T>& output_signal);
  FeatureParams feature_params() const;

  // apply mask to real and imaginary parts
  // last 2 vectors (real, imag) of input_spec will be changed
  void calc_enhanced_parts_in_place(const ComplexNumber<T>& input_spec,
                                ComplexNumber<T>& predicted_mask);

 protected:
  void stft_impl(const std::vector<T>& frames,
                std::vector<ComplexNumber<T>>& result);

  void istft_impl(const ComplexNumber<T>& input_spec,
                 std::vector<T>& output_signal);

  void validate_pow_spec_params() const;

  // process mask for real and imaginary parts
  // predicted_mask will be changed
  void decompress_cirm_in_place(ComplexNumber<T>& predicted_mask);

  std::valarray<T> bool_to_t(const std::valarray<bool> b);
  void apply_decompress_cirm_in_place(std::valarray<T>& mask_part);

  void lookup_add(const std::vector<T>& signal,
                 std::vector<T>& output_signal);

  FeatureParams feat_params_;
  Windowing<T> windowing_;
  std::mutex fft_mutex_;
  FftCalculator<T> fft_calculator_;
  std::mutex ifft_mutex_;
  IFftCalculator<T> ifft_calculator_;
  T cirm_k_{static_cast<T>(10.0)};
  T cirm_limit_{static_cast<T>(9.9)};
};

}  // namespace fe
}  // namespace spench
