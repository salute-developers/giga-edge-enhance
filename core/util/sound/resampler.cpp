#include "util/sound/resampler.h"

#ifdef BUILD_RESAMPLER
#include <soxr.h>
#include <util/sound/resampler_fwd.h>
#endif

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include "src/logger.h"

namespace util {
namespace sound {

#ifdef BUILD_RESAMPLER

class ResamplerImpl {
 public:
  ResamplerImpl(size_t in_rate, size_t out_rate, size_t channels)
      : in_rate_{in_rate}, out_rate_{out_rate}, channels_{channels} {
    if (in_rate == 0 || out_rate == 0 || channels == 0) {
      throw std::invalid_argument("Resampler rates and channel count must be positive");
    }
    // Same rate: pass-through (SoundStream always owns a Resampler; soxr is unnecessary).
    if (in_rate == out_rate) {
      pass_through_ = true;
      giga_edge::enhance::spdlog::info(
          "Resampler pass-through: input rate equals output rate ({} Hz); soxr not constructed "
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
    // soxr treats a null input pointer as end-of-stream. An empty chunk must
    // remain a no-op so a later nonempty chunk can continue the same stream.
    if (v.empty()) {
      return {};
    }
    if (v.size() % channels_ != 0) {
      throw std::invalid_argument("Resampler input must contain complete interleaved frames");
    }
    if (pass_through_) {
      return v;
    }
    if (!rsmp_) {
      throw std::runtime_error("ResampleVec: resampler is null!");
    }

    std::vector<float> resampled;
    const size_t input_frames = v.size() / channels_;
    // Preserve the established per-call output budget used by SoundStream.
    // A nonzero budget also allows one-sample downsampling calls to consume input.
    const double ratio = static_cast<double>(out_rate_) / static_cast<double>(in_rate_);
    const size_t block_frames = std::max<size_t>(
        1, static_cast<size_t>(static_cast<double>(input_frames) * ratio + (in_rate_ < out_rate_ ? 0.0 : 0.5)));
    std::vector<float> block(block_frames * channels_);
    size_t consumed = 0;
    while (consumed < input_frames) {
      size_t input_len = 0;
      size_t out_len = 0;
      const auto err = soxr_process(rsmp_.get(), v.data() + consumed * channels_, input_frames - consumed, &input_len,
                                    block.data(), block_frames, &out_len);
      if (err) {
        throw std::runtime_error(std::string("Error during soxr process: ") + soxr_strerror(err));
      }
      if (input_len == 0 && out_len == 0) {
        throw std::runtime_error("Resampler made no progress");
      }
      consumed += input_len;
      resampled.insert(resampled.end(), block.begin(),
                       block.begin() + static_cast<std::ptrdiff_t>(out_len * channels_));
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
    // Variable-rate soxr reports an estimated delay (100 samples in 0.1.3),
    // not an upper bound. Drain until empty rather than truncating to that estimate.
    constexpr size_t block_frames = 4096;
    std::vector<float> block(block_frames * channels_);
    std::vector<float> resampled;
    for (;;) {
      size_t out_len = 0;
      const auto err = soxr_process(rsmp_.get(), nullptr, 0, nullptr, block.data(), block_frames, &out_len);
      if (err) {
        throw std::runtime_error("ResampleVec: soxr internal error occurred!");
      }
      if (out_len == 0) {
        break;
      }
      resampled.insert(resampled.end(), block.begin(),
                       block.begin() + static_cast<std::ptrdiff_t>(out_len * channels_));
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
  size_t channels_{1};
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
