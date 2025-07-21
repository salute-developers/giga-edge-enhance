#include "spench/src/denoiser_model_adviser.h"
#include <spdlog/spdlog.h>
#include "spench/src/spench_context.h"

namespace spench {

// Explicit template instantiations to keep build times short and allow the
// implementation to live primarily in the header for other types.

template <typename T>
T DenoiserModelAdviser<T>::calcSnrEstimation(const ComplexNumber<T>& input_spec,
                                             const ComplexNumber<T>& enhanced_spec,
                                             StreamCurrentMetrics<T>& metrics) {
  double eps = static_cast<double>(this->m_eps);

  if (input_spec.real.size() != input_spec.imag.size() ||
      enhanced_spec.real.size() != enhanced_spec.imag.size() ||
      input_spec.real.size() != enhanced_spec.real.size()) {
    spdlog::error("Mismatched input sizes: input_spec.real = {}, input_spec.imag = {}, "
                  "enhanced_spec.real = {}, enhanced_spec.imag = {}",
                  input_spec.real.size(), input_spec.imag.size(),
                  enhanced_spec.real.size(), enhanced_spec.imag.size());
    return static_cast<T>(0.0);
  }

  double E1 = (enhanced_spec.real * enhanced_spec.real +
               enhanced_spec.imag * enhanced_spec.imag).sum() + eps;

  double E2 = (input_spec.real * input_spec.real +
               input_spec.imag * input_spec.imag).sum() + eps;

  if (!std::isfinite(E1) || !std::isfinite(E2) || E1 <= 0.0 || E2 <= 0.0) {
    spdlog::error("Invalid energy values: E1 = {}, E2 = {}", E1, E2);
    return static_cast<T>(0.0);
  }

  metrics.m_e1_max = std::max(metrics.m_e1_max, static_cast<T>(E1));

  if (E1 > static_cast<double>(metrics.m_e1_max) * this->vad_threshold) {
    metrics.m_speech_energy_avg = static_cast<T>(
            (static_cast<double>(metrics.m_speech_energy_avg) * 0.95) + (E1 * 0.05));
  } else {
    metrics.m_noise_energy_avg = static_cast<T>(
            (static_cast<double>(metrics.m_noise_energy_avg) * 0.95) + (E2 * 0.05));
  }

  double speech_energy = static_cast<double>(metrics.m_speech_energy_avg);
  double noise_energy  = static_cast<double>(metrics.m_noise_energy_avg);

  if (!std::isfinite(speech_energy) || !std::isfinite(noise_energy) || noise_energy <= 0.0) {
    spdlog::error("Invalid avg energies: speech = {}, noise = {}", speech_energy, noise_energy);
    return static_cast<T>(0.0);
  }

  double snr_linear = speech_energy / noise_energy;
  if (snr_linear <= 0.0 || !std::isfinite(snr_linear)) {
    spdlog::error("Invalid SNR linear ratio: {}", snr_linear);
    return static_cast<T>(0.0);
  }

  double SNR_est = 10.0 * std::log10(snr_linear);
  if (!std::isfinite(SNR_est)) {
    spdlog::error("log10(SNR) returned non-finite: {}", SNR_est);
    return static_cast<T>(0.0);
  }

  return static_cast<T>(SNR_est);
}

template class DenoiserModelAdviser<float>;

}  // namespace spench
