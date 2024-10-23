#include "spench/util/metrics/mute_frame_detector.h"

namespace util {
namespace metrics {

uint32_t MuteFrameDetector::process(float x) noexcept {
    assert(m_buf.size() % 3 == 0);

    // Let's move on to integer arithmetic to make it easier to implement a moving average filter
    x = x * m_scale;
    const auto xx = int64_t(x * x) / m_n;

    m_accL -= m_buf[index(0)];
    m_accC -= m_buf[index(m_n)];
    m_accR -= m_buf[index(2 * m_n)];

    m_buf[m_ibuf] = xx;

    m_accL += m_buf[index(m_n)];
    m_accC += m_buf[index(2 * m_n)];
    m_accR += m_buf[index(3 * m_n)];

    m_ibuf = (m_ibuf + 1) % m_n3;

    const float pl = pow2db(m_accL) - m_scaleDB;
    const float pc = pow2db(m_accC) - m_scaleDB;
    const float pr = pow2db(m_accR) - m_scaleDB;
    const float pm = pow2db((m_accL + m_accR) / 2) - m_scaleDB;

    // don’t make decisions on a weak signal
    if (pm < m_minrms) {
        return 0;
    }

    // the difference between left/right frame should not exceed a threshold
    if (std::abs(pl - pr) > m_lrrange) {
        return 0;
    }

    // local minimum must be greater than threshold
    if (pm - pc < m_threshold) {
        return 0;
    }

    return 1;
}

}  // namespace metrics
}  // namespace util
