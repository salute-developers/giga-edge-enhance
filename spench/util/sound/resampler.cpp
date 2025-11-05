#include "spench/util/sound/resampler.h"

#include <spench/submodules/soxr/src/soxr.h>
#include <stdexcept>
#include <string>

namespace util {
namespace sound {

void ResamplerDeleter::operator()(soxr_t resampler) const {
  if (resampler != nullptr) {
    soxr_delete(resampler);
  }
}

ResampleWrapper BuildResampler(size_t in_rate, size_t out_rate, size_t channels) {
  if (in_rate == out_rate) {
    throw std::runtime_error("BuildResampler: in_rate equals out_rate.");
  }
  soxr_error_t err;

  // Enable VR (variable-rate) only if 44100 Hz is involved (either side).
  const bool use_vr = (in_rate == 44100 || out_rate == 44100);
  soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_VHQ, use_vr ? SOXR_VR : 0x00);

  soxr* resampler_state = soxr_create(
      in_rate, out_rate, channels,        /* Input rate, output rate, # of channels. */
      &err,                               /* To report any error during creation. */
      NULL,                               /* io spec    */
      // pass NULL to keep library defaults when not VR
      use_vr ? &q_spec : NULL,            /* quality    */
      NULL);                              /* runtime    */

  if (err || !resampler_state) {
    throw std::runtime_error(std::string("BuildResampler: failed to create resampler: ") +
                             std::string(soxr_strerror(err)));
  }
  return ResampleWrapper(resampler_state);
}

bool SetResamplerIoRatio(const ResampleWrapper& resampler_state, double io_ratio) {
  if (!resampler_state) return false;
  soxr_error_t err = soxr_set_io_ratio(resampler_state.get(), io_ratio, 0.0);
  return err == 0;
}

template <typename T>
std::vector<T> ResampleVec(const ResampleWrapper& resampler_state,
                           const std::vector<T>& v, const size_t in_rate,
                           const size_t out_rate) {
  if (!resampler_state) {
    throw std::runtime_error("ResampleVec: resampler is null!");
  }
  if (std::is_same<T, double>::value) {
    throw std::runtime_error("ResampleVec: T=double is not supported!");
  }
  double ratio = 0;
  size_t out_len = 1;
  size_t input_len = 0;
  std::vector<T> resampled;

  if (in_rate < out_rate) {
    ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    // a trick for accurate resampling with 44.1 sample rate case.
    resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size() * ratio) + 0.0f));
  } else {
    ratio = static_cast<double>(in_rate) / static_cast<double>(out_rate);
    resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size() / ratio) + 0.5f));
  }
  soxr_error_t err;

  err = soxr_process(resampler_state.get(), v.data(), v.size(), &input_len,
                     resampled.data(), resampled.size(), &out_len);

  if (err) {
    throw std::runtime_error("Error during soxr process");
  }

  if (resampled.size() != static_cast<size_t>(out_len)) {
    resampled.resize(out_len);
  }
  return resampled;
}

template <typename T>
std::vector<T> FlushVec(const ResampleWrapper& resampler_state) {
  if (!resampler_state) {
    throw std::runtime_error("ResampleVec: resampler is null!");
  }
  if (std::is_same<T, double>::value) {
    throw std::runtime_error("ResampleVec: T=double is not supported!");
  }
  size_t out_len = 1;
  double delayed_samples = soxr_delay((resampler_state.get()));
  std::vector<T> resampled(static_cast<size_t>(delayed_samples)+1);
  soxr_error_t err;

  err = soxr_process(resampler_state.get(), NULL, 0, NULL, resampled.data(), resampled.size(), &out_len);
  if (err) {
    throw std::runtime_error("ResampleVec: soxr internal error occurred!");
  }
  if (resampled.size() != static_cast<size_t>(out_len)) {
    resampled.resize(out_len);
  }
  return resampled;
}

}  // namespace sound
}  // namespace util

template std::vector<float> util::sound::ResampleVec(const util::sound::ResampleWrapper& resampler_state,
                                                           const std::vector<float>& v, const size_t in_rate,
                                                           const size_t out_rate);
template std::vector<float> util::sound::FlushVec(const util::sound::ResampleWrapper& resampler_state);
