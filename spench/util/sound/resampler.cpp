#include "spench/util/sound/resampler.h"

#ifndef BUILD_WITHOUT_SOXR
#include <spench/submodules/soxr/src/soxr.h>
#include <spench/util/sound/resampler_fwd.h>
#endif

#include <memory>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

namespace util {
namespace sound {

#ifndef BUILD_WITHOUT_SOXR

class ResamplerImpl {
 public:
  ResamplerImpl(size_t in_rate, size_t out_rate, size_t channels) : m_in_rate{in_rate}, m_out_rate{out_rate} {
    // Same rate: pass-through (SoundStream always owns a Resampler; soxr is unnecessary).
    if (in_rate == out_rate) {
      m_pass_through = true;
      spdlog::info("Resampler pass-through: input rate equals output rate ({} Hz); soxr not constructed "
                    "({} channel(s))",
                    in_rate, channels);
      return;
    }

    soxr_error_t err;

    // Enable VR (variable-rate) only if 44100 Hz is involved (either side).
    const bool use_vr = (in_rate == 44100 || out_rate == 44100);
    soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_VHQ, use_vr ? SOXR_VR : 0x00);

    soxr* state = soxr_create(in_rate, out_rate, channels, /* Input rate, output rate, # of channels. */
                              &err,                        /* To report any error during creation. */
                              NULL,                        /* io spec    */
                              // pass NULL to keep library defaults when not VR
                              use_vr ? &q_spec : NULL, /* quality    */
                              NULL);                   /* runtime    */

    if (err || !state) {
      throw std::runtime_error(std::string("BuildResampler: failed to create resampler: ") +
                               std::string(soxr_strerror(err)));
    }
    m_rsmp = std::shared_ptr<soxr>(state, soxr_delete);
  }

  bool set_io_ratio(double io_ratio) {
    if (m_pass_through) {
      return true;
    }
    if (!m_rsmp) {
      return false;
    }
    soxr_error_t err = soxr_set_io_ratio(m_rsmp.get(), io_ratio, 0.0);
    return err == 0;
  }

  std::vector<float> process(const std::vector<float>& v) {
    if (m_pass_through) {
      return v;
    }
    if (!m_rsmp) {
      throw std::runtime_error("ResampleVec: resampler is null!");
    }

    double ratio = 0;
    size_t out_len = 1;
    size_t input_len = 0;
    std::vector<float> resampled;

    // TODO(?): wtf
    if (m_in_rate < m_out_rate) {
      ratio = static_cast<double>(m_out_rate) / static_cast<double>(m_in_rate);
      // a trick for accurate resampling with 44.1 sample rate case.
      resampled = std::vector<float>(static_cast<size_t>(static_cast<double>(v.size() * ratio) + 0.0f));
    } else {
      ratio = static_cast<double>(m_in_rate) / static_cast<double>(m_out_rate);
      resampled = std::vector<float>(static_cast<size_t>(static_cast<double>(v.size() / ratio) + 0.5f));
    }

    soxr_error_t err =
        soxr_process(m_rsmp.get(), v.data(), v.size(), &input_len, resampled.data(), resampled.size(), &out_len);
    if (err) {
      throw std::runtime_error("Error during soxr process");
    }

    if (resampled.size() != static_cast<size_t>(out_len)) {
      resampled.resize(out_len);
    }
    return resampled;
  }

  std::vector<float> flush() {
    if (m_pass_through) {
      return {};
    }
    if (!m_rsmp) {
      throw std::runtime_error("ResampleVec: resampler is null!");
    }
    size_t out_len = 1;
    double delayed_samples = soxr_delay((m_rsmp.get()));
    std::vector<float> resampled(static_cast<size_t>(delayed_samples) + 1);
    soxr_error_t err;

    err = soxr_process(m_rsmp.get(), NULL, 0, NULL, resampled.data(), resampled.size(), &out_len);
    if (err) {
      throw std::runtime_error("ResampleVec: soxr internal error occurred!");
    }
    if (resampled.size() != static_cast<size_t>(out_len)) {
      resampled.resize(out_len);
    }
    return resampled;
  }

  float accumulated_delay_ms() const noexcept {
    if (m_pass_through) {
      return 0;
    }
    double ms = 0;
    if (m_in_rate != m_out_rate) {
      const double d = soxr_delay(m_rsmp.get());  // in output samples
      if (d > 0.0 && m_out_rate != 0) {
        ms += 1000.0 * d / static_cast<double>(m_out_rate);
      }
    }
    return ms;
  }

 private:
  bool m_pass_through{false};
  std::shared_ptr<soxr> m_rsmp;
  size_t m_in_rate{1};
  size_t m_out_rate{1};
};

#else

// dumb bypass implementation
class ResamplerImpl {
 public:
  ResamplerImpl(size_t in_rate, size_t out_rate, size_t channels) {
    if (in_rate != out_rate) {
      throw std::runtime_error("Resampling is not supported");
    }
  }

  bool set_io_ratio(double io_ratio) { return true; }

  std::vector<float> process(const std::vector<float>& v) { return v; }

  std::vector<float> flush() { return {}; }

  float accumulated_delay_ms() const noexcept { return 0; }
};

#endif

Resampler::Resampler(size_t in_rate, size_t out_rate, size_t channels) {
  m_d = std::make_shared<ResamplerImpl>(in_rate, out_rate, channels);
}

std::vector<float> Resampler::process(const std::vector<float>& x) { return m_d->process(x); }

std::vector<float> Resampler::flush() { return m_d->flush(); }

bool Resampler::set_io_ratio(double io_ratio) { return m_d->set_io_ratio(io_ratio); }

float Resampler::accumulated_delay_ms() const noexcept { return m_d->accumulated_delay_ms(); }


}  // namespace sound
}  // namespace util
