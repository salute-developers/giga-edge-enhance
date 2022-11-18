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
  soxr* resampler_state = soxr_create(
      in_rate, out_rate, channels,        /* Input rate, output rate, # of channels. */
      &err,                               /* To report any error during creation. */
      NULL, NULL, NULL);                  /* Use configuration defaults.*/

  if (err || !resampler_state) {
    throw std::runtime_error(std::string("BuildResampler: failed to create resampler: ") +
                             std::string(soxr_strerror(err)));
  }
  return ResampleWrapper(resampler_state);
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
  // std::cout << "input rate " << in_rate << " output rate " << out_rate << std::endl;
  if (in_rate < out_rate) {
    ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    if (static_cast<uint32_t>(out_rate) % static_cast<uint32_t>(in_rate) == 0) {
      resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) * ratio));
    } else {
      resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) * ratio) + 1);
    }
  } else {
    ratio = static_cast<double>(in_rate) / static_cast<double>(out_rate);
    if (static_cast<uint32_t>(in_rate) % static_cast<uint32_t>(out_rate) == 0) {
      resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) / ratio));
    } else {
      resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) / ratio) + 1);
    }
  }
  soxr_error_t err;
  // if (v.size() != 0)
    // std::cout << "Input for resampler " << v[0] << " " << v[1] << " " << v[2] << " " << v[3] << " " << std::endl;
  err = soxr_process(resampler_state.get(), v.data(), v.size(), &input_len,
                     resampled.data(), resampled.size(), &out_len);
  // if (out_len != 0)
    // std::cout << "Output for resampler " << resampled[0] << " " << resampled[1] << " " << resampled[2] << " " << resampled[3] << " " << std::endl;
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
    throw std::runtime_error("ResampleVec: speex internal error occured!");
  }
  if (resampled.size() != static_cast<size_t>(out_len)) {
    resampled.resize(out_len);
  }
  return resampled;
}

template <typename T>
std::vector<T> ResampleVecRT(const std::vector<T>& v, const size_t in_rate,
                           const size_t out_rate) {
  double ratio = 0;
  size_t out_len = 1;
  size_t input_len = 0;
  std::vector<T> resampled;
  if (in_rate < out_rate) {
    ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    resampled = std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) * ratio) + 1);
  } else {
    ratio = static_cast<double>(in_rate) / static_cast<double>(out_rate);
    resampled = (in_rate % out_rate == 0) ?
                            std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) / ratio)):
                            std::vector<T>(static_cast<size_t>(static_cast<double>(v.size()) / ratio) + 1);
  }
  soxr_error_t err;
  soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_MQ, 0x00);
  err = soxr_oneshot(in_rate, out_rate, 1, v.data(), v.size(), &input_len,
                     resampled.data(), resampled.size(), &out_len,
                     NULL, &q_spec, NULL);
  if (err) {
    throw std::runtime_error("Error during soxr process");
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
template std::vector<float> util::sound::ResampleVecRT(const std::vector<float>& v, const size_t in_rate,
                                                           const size_t out_rate);
template std::vector<float> util::sound::FlushVec(const util::sound::ResampleWrapper& resampler_state);
