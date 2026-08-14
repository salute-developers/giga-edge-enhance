#include "spench/util/sound/resampler.h"

#ifndef BUILD_WITHOUT_SOXR
#include <soxr.h>
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
  ResamplerImpl(size_t in_rate, size_t out_rate, size_t channels) : in_rate_{in_rate}, out_rate_{out_rate} {
    // Same rate: pass-through (SoundStream always owns a Resampler; soxr is unnecessary).
    if (in_rate == out_rate) {
      pass_through_ = true;
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
    rsmp_ = std::shared_ptr<soxr>(state, soxr_delete);
  }

  bool set_io_ratio(double io_ratio) {
    if (pass_through_) {
      return true;
    }
    if (!rsmp_) {
      return false;
    }
    soxr_error_t err = soxr_set_io_ratio(rsmp_.get(), io_ratio, 0.0);
    return err == 0;
  }

  std::vector<float> process(const std::vector<float>& v) {
    if (pass_through_) {
      return v;
    }
    if (!rsmp_) {
      throw std::runtime_error("ResampleVec: resampler is null!");
    }

    double ratio = 0;
    size_t out_len = 1;
    size_t input_len = 0;
    std::vector<float> resampled;

    // TODO(?): wtf
    if (in_rate_ < out_rate_) {
      ratio = static_cast<double>(out_rate_) / static_cast<double>(in_rate_);
      // a trick for accurate resampling with 44.1 sample rate case.
      resampled = std::vector<float>(static_cast<size_t>(static_cast<double>(v.size() * ratio) + 0.0f));
    } else {
      ratio = static_cast<double>(in_rate_) / static_cast<double>(out_rate_);
      resampled = std::vector<float>(static_cast<size_t>(static_cast<double>(v.size() / ratio) + 0.5f));
    }

    soxr_error_t err =
        soxr_process(rsmp_.get(), v.data(), v.size(), &input_len, resampled.data(), resampled.size(), &out_len);
    if (err) {
      throw std::runtime_error("Error during soxr process");
    }

    if (resampled.size() != static_cast<size_t>(out_len)) {
      resampled.resize(out_len);
    }
    return resampled;
  }

  std::vector<float> flush() {
    if (pass_through_) {
      return {};
    }
    if (!rsmp_) {
      throw std::runtime_error("ResampleVec: resampler is null!");
    }
    size_t out_len = 1;
    double delayed_samples = soxr_delay((rsmp_.get()));
    std::vector<float> resampled(static_cast<size_t>(delayed_samples) + 1);
    soxr_error_t err;

    err = soxr_process(rsmp_.get(), NULL, 0, NULL, resampled.data(), resampled.size(), &out_len);
    if (err) {
      throw std::runtime_error("ResampleVec: soxr internal error occurred!");
    }
    if (resampled.size() != static_cast<size_t>(out_len)) {
      resampled.resize(out_len);
    }
    return resampled;
  }

  float accumulated_delay_ms() const noexcept {
    if (pass_through_) {
      return 0;
    }
    double ms = 0;
    if (in_rate_ != out_rate_) {
      const double d = soxr_delay(rsmp_.get());  // in output samples
      if (d > 0.0 && out_rate_ != 0) {
        ms += 1000.0 * d / static_cast<double>(out_rate_);
      }
    }
    return ms;
  }

 private:
  bool pass_through_{false};
  std::shared_ptr<soxr> rsmp_;
  size_t in_rate_{1};
  size_t out_rate_{1};
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
  impl_ = std::make_shared<ResamplerImpl>(in_rate, out_rate, channels);
}

std::vector<float> Resampler::process(const std::vector<float>& x) { return impl_->process(x); }

std::vector<float> Resampler::flush() { return impl_->flush(); }

bool Resampler::set_io_ratio(double io_ratio) { return impl_->set_io_ratio(io_ratio); }

float Resampler::accumulated_delay_ms() const noexcept { return impl_->accumulated_delay_ms(); }


}  // namespace sound
}  // namespace util
