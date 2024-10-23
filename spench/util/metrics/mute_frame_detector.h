#include <vector>
#include <cassert>
#include <cstdint>
#include <cmath>
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
      : m_n{chunk_len}
      , m_n3{m_n * 3}
      , m_scaleDB{20 * std::log10(static_cast<float>(m_scale))}
      , m_buf(m_n3)
      , m_lrrange{lrrange}
      , m_minrms{minrms}
      , m_threshold{threshold} {
    }

    std::vector<float> process(const std::vector<float>& x) noexcept {
        std::vector<float> y(x.size());
        for (uint32_t i = 0; i < x.size(); ++i) {
            y[i] = process(x[i]);
        }
        // TODO: can additionally set the median filter to 1/4 of
        // the frame length to filter out single triggers
        return y;
    }

    uint32_t process(float x) noexcept;

 private:
    [[nodiscard]] int32_t index(int32_t offset) const noexcept {
        return (m_ibuf + offset) % m_n3;
    }

    [[nodiscard]] float pow2db(float v) const noexcept {
        return 10 * std::log10(v + 1e-10);
    }

    const int32_t m_n;
    const int32_t m_n3;
    const int32_t m_scale = 1UL << 15;
    const float m_scaleDB{0};
    std::vector<int64_t> m_buf;
    int32_t m_ibuf{0};
    int64_t m_accL{0};
    int64_t m_accR{0};
    int64_t m_accC{0};
    const float m_lrrange{6};
    const float m_minrms{-30};
    const float m_threshold{20};
};

}  // namespace metrics
}  // namespace util
