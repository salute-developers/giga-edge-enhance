#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
namespace util {
namespace metrics {

/**
 * @brief Detector object for filtering anomalies
 * @details Detects abnormal "gaps" in sound
 *
 */
class MuteFrameDetector {
 public:
  /**
   * @brief Construct a new Mute Frame Detector object for filtering anomalies
   * on input and output audio
   *
   * @param chunk_len Length of chunks for analisys
   * @param threshold How much (dB) is the power in the anomalous area lower than the surrounding signal
   * @param lrrange Allowable difference (dB) in the power of fragments around the anomaly
   * @param minrms Minimum signal strength (dB) for anomalies detection
   */
  explicit MuteFrameDetector(int32_t chunk_len, float threshold = 20, float lrrange = 6, float minrms = -30)
      : n_{chunk_len},
        n3_{n_ * 3},
        scale_db_{20 * std::log10(static_cast<float>(scale_))},
        buf_(n3_),
        lr_range_{lrrange},
        min_rms_{minrms},
        threshold_{threshold} {}

  std::vector<float> process(const std::vector<float>& x) noexcept {
    std::vector<float> y(x.size());
    for (uint32_t i = 0; i < x.size(); ++i) {
      y[i] = process(x[i]);
    }
    // TODO(vitaliy): can additionally set the median filter to 1/4 of
    // the frame length to filter out single triggers
    return y;
  }

  uint32_t process(float x) noexcept;

 private:
  [[nodiscard]] int32_t index(int32_t offset) const noexcept { return (ibuf_ + offset) % n3_; }

  [[nodiscard]] float pow2db(float v) const noexcept { return 10 * std::log10(v + 1e-10); }

  int32_t n_;
  int32_t n3_;
  int32_t scale_ = 1UL << 15;
  float scale_db_{0};
  std::vector<int64_t> buf_;
  int32_t ibuf_{0};
  int64_t acc_l_{0};
  int64_t acc_r_{0};
  int64_t acc_c_{0};
  float lr_range_{6};
  float min_rms_{-30};
  float threshold_{20};
};

}  // namespace metrics
}  // namespace util
