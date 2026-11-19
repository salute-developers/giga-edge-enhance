#include "src/denoiser_model_adviser.h"

#include <spdlog/spdlog.h>

#include "src/context.h"
#include "src/logger.h"

namespace gigaedge::enhance {

// Explicit template instantiations to keep build times short and allow the
// implementation to live primarily in the header for other types.

template <typename T>
T DenoiserModelAdviser<T>::calc_snr_estimation(const ComplexNumber<T>& input_spec,
                                               const ComplexNumber<T>& enhanced_spec,
                                               StreamCurrentMetrics<T>& metrics) {
  double eps = static_cast<double>(this->eps);

  if (input_spec.real.size() != input_spec.imag.size() || enhanced_spec.real.size() != enhanced_spec.imag.size() ||
      input_spec.real.size() != enhanced_spec.real.size()) {
    spdlog::error(
        "Mismatched input sizes: input_spec.real = {}, input_spec.imag = {}, "
        "enhanced_spec.real = {}, enhanced_spec.imag = {}",
        input_spec.real.size(), input_spec.imag.size(), enhanced_spec.real.size(), enhanced_spec.imag.size());
    return static_cast<T>(0.0);
  }

  double e1 = (enhanced_spec.real * enhanced_spec.real + enhanced_spec.imag * enhanced_spec.imag).sum() + eps;

  double e2 = (input_spec.real * input_spec.real + input_spec.imag * input_spec.imag).sum() + eps;

  if (!std::isfinite(e1) || !std::isfinite(e2) || e1 <= 0.0 || e2 <= 0.0) {
    spdlog::error("Invalid energy values: E1 = {}, E2 = {}", e1, e2);
    return static_cast<T>(0.0);
  }

  metrics.e1_max = std::max(metrics.e1_max, static_cast<T>(e1));

  if (e1 > static_cast<double>(metrics.e1_max) * this->snr_vad_threshold) {
    metrics.speech_energy_avg = static_cast<T>((static_cast<double>(metrics.speech_energy_avg) * 0.95) + (e1 * 0.05));
  } else {
    metrics.noise_energy_avg = static_cast<T>((static_cast<double>(metrics.noise_energy_avg) * 0.95) + (e2 * 0.05));
  }

  double speech_energy = static_cast<double>(metrics.speech_energy_avg);
  double noise_energy = static_cast<double>(metrics.noise_energy_avg);

  if (!std::isfinite(speech_energy) || !std::isfinite(noise_energy) || noise_energy <= 0.0) {
    spdlog::error("Invalid avg energies: speech = {}, noise = {}", speech_energy, noise_energy);
    return static_cast<T>(0.0);
  }

  double snr_linear = speech_energy / noise_energy;
  if (snr_linear <= 0.0 || !std::isfinite(snr_linear)) {
    spdlog::error("Invalid SNR linear ratio: {}", snr_linear);
    return static_cast<T>(0.0);
  }

  double snr_est = 10.0 * std::log10(snr_linear);
  if (!std::isfinite(snr_est)) {
    spdlog::error("log10(SNR) returned non-finite: {}", snr_est);
    return static_cast<T>(0.0);
  }

  // When exiting the practical range [-20, 50]
  // we assign a double practical minimum/maximum
  if (snr_est > 50) {
    snr_est = 100.0;
  } else if (snr_est < -20) {
    snr_est = -40.0;
  }

  return static_cast<T>(snr_est);
}

template class DenoiserModelAdviser<float>;

}  // namespace gigaedge::enhance
