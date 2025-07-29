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
 * @tparam T Floating‑point type used throughout the DSP chain (float, double…).
 *
 * The adviser keeps a sliding window of recent SNR estimates (in dB),
 * and computes their arithmetic mean.
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
  */
  DenoiserModelAdviser(double window_duration_ms,
                       double frame_shift_ms)
        : m_window_frames(static_cast<size_t>(std::max(1.0,
                              std::round(window_duration_ms / frame_shift_ms)))) {}

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
  T processFrame(const ComplexNumber<T>& input_spec,
                            const ComplexNumber<T>& enhanced_spec,
                            StreamCurrentMetrics<T>& metrics) {
    T value = calcSnrEstimation(input_spec, enhanced_spec, metrics);
    add_value(value);
    T average = averageSnrDb();
    return average;
  }

  /** Reset the sliding window (e.g. on stream boundaries). */
  void reset() {
    m_snr_history.clear();
    m_sum = 0;
  }

  void add_value(T value) {
    m_snr_history.push_back(value);
    m_sum += value;

    if (m_snr_history.size() > m_window_frames) {
      m_sum -= m_snr_history.front();
      m_snr_history.pop_front();
    }
  }

  /**
  * @return Current windowed‑average SNR in dB, or NaN if the window is not yet filled.
  */
  [[nodiscard]] T averageSnrDb() const {
    if (m_snr_history.size() == m_window_frames) {
      return m_sum / static_cast<T>(m_snr_history.size());
    }
    return std::numeric_limits<T>::quiet_NaN();
  }

  T calcSnrEstimation(const ComplexNumber<T>& input_spec,
                    const ComplexNumber<T>& enhanced_spec,
                    StreamCurrentMetrics<T>& metrics);

 private:
  std::deque<T> m_snr_history;  ///< Circular buffer of recent SNR estimates (in dB).
  size_t        m_window_frames;  ///< Window length expressed in *frames*.
  T             m_sum = static_cast<T>(0);
  const T       m_eps = static_cast<T>(1e-8);
  const T       m_snr_vad_threshold = static_cast<T>(0.1);
};

}  // namespace spench
