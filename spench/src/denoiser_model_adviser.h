#pragma once

#include <deque>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>
#include <type_traits>

namespace spench {

template <typename T> struct ComplexNumber;
template <typename T> struct StreamCurrentMetrics;

/**
 * @brief High‑level instruction returned by the decision controller.
 */
enum class ModelAction {
  UseHeavyModel,  ///< Switch to a more powerful—but slower—denoiser.
  UseLightModel,  ///< Keep (or switch to) a lightweight/real‑time denoiser.
  Bypass          ///< Disable denoiser and forward the original signal unchanged.
};

/**
 * @tparam T Floating‑point type used throughout the DSP chain (float, double…).
 *
 * The adviser keeps a sliding window of recent SNR estimates (in dB),
 * computes their arithmetic mean, and maps that average to a high‑level action:
 *   • avg SNR < lower → heavy model
 *   • lower ≤ avg SNR ≤ upper → light model (default)
 *   • avg SNR > upper → bypass (signal is already clean)
 */
template <typename T>
class DenoiserModelAdviser {
  static_assert(std::is_floating_point_v<T>, "T must be a floating‑point type");

 public:
  // Deleted default constructor – window size and thresholds are mandatory.
  DenoiserModelAdviser() = delete;

  /**
  * @param window_duration_ms  Length of the averaging window (milliseconds).
  * @param frame_shift_ms      Hop size between successive spectra fed into
  *                            calcSnrEstimation (milliseconds).
  * @param lower_db_threshold  SNR (dB) below which the *heavy* model is chosen.
  * @param upper_db_threshold  SNR (dB) above which the denoiser is bypassed.
  */
  DenoiserModelAdviser(double window_duration_ms,
                       double frame_shift_ms,
                       T lower_db_threshold,
                       T upper_db_threshold)
        : m_window_frames(static_cast<size_t>(std::max(1.0,
                              std::round(window_duration_ms / frame_shift_ms)))),
          m_lower_db(lower_db_threshold),
          m_upper_db(upper_db_threshold) {}

  // Copy/move semantics – defaulted explicitly for clarity.
  DenoiserModelAdviser(const DenoiserModelAdviser&)            = default;
  DenoiserModelAdviser& operator=(const DenoiserModelAdviser&) = default;
  DenoiserModelAdviser(DenoiserModelAdviser&&) noexcept        = default;
  DenoiserModelAdviser& operator=(DenoiserModelAdviser&&) noexcept = default;
  ~DenoiserModelAdviser() = default;

  /**
  * @brief Feed one time‑frame worth of spectra and get the recommended action.
  *
  * @tparam Extractor Type that exposes
  *         `T calcSnrEstimation(const ComplexNumber<T>&, const ComplexNumber<T>&, StreamCurrentMetrics<T>&)`.
  *
  * @return A high‑level action for model selection.
  */
//   template <typename Extractor>
//   ModelAction processFrame(const ComplexNumber<T>& input_spec,
//                            const ComplexNumber<T>& enhanced_spec,
//                            StreamCurrentMetrics<T>& metrics) {
//   return ModelAction::UseLightModel;
//   }

  /** Reset the sliding window (e.g. on stream boundaries). */
  void reset() { m_snr_history.clear(); }

  /**
  * @return Current windowed‑average SNR in dB, or NaN if the window is not yet filled.
  */
  [[nodiscard]] T averageSnrDb() const {
    return 0.0f;
  }

  T calcSnrEstimation(const ComplexNumber<T>& input_spec,
                    const ComplexNumber<T>& enhanced_spec,
                    StreamCurrentMetrics<T>& metrics);

 private:
  std::deque<T> m_snr_history;  ///< Circular buffer of recent SNR estimates (in dB).
  size_t        m_window_frames;  ///< Window length expressed in *frames*.
  T             m_lower_db;  ///< Heavy‑model threshold (dB).
  T             m_upper_db;  ///< Bypass threshold (dB).
  const T       m_eps = static_cast<T>(1e-8);
  const T       vad_threshold = static_cast<T>(0.1);
};

}  // namespace spench
