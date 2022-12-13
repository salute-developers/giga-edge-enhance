#include <spench/util/sound/sound_stream.h>
#include <spench/util/sound/sound_types.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace util {
namespace sound {

template <typename TIN, typename TOUT>
ISoundStream<TIN, TOUT>::~ISoundStream() = default;

template <typename TIN, typename TOUT>
SoundStream<TIN, TOUT>::~SoundStream() = default;

template <typename TIN, typename TOUT>
SoundStream<TIN, TOUT>::SoundStream(const SoundInfo& src, const SoundInfo& dst)
    : ISoundStream<TIN, TOUT>(src, dst)
    , m_resampler{} {
  m_in_resample = false;
  if (src.channels != dst.channels && src.channels != 1) {
    throw std::runtime_error("SoundStream::SoundStream: channel conversion from " + std::to_string(src.channels) +
                             " to " + std::to_string(dst.channels) + " is not supported!");
  }
  if (src.sample_rate != dst.sample_rate) {
    if (std::is_same<TIN, float>::value || std::is_same<TIN, int16_t>::value || std::is_same<TIN, int32_t>::value) {
      m_in_resample = true;
      m_resampler = BuildResampler(src.sample_rate, dst.sample_rate, src.channels);
    } else if (std::is_same<TOUT, float>::value || std::is_same<TOUT, int16_t>::value ||
               std::is_same<TOUT, int32_t>::value) {
      m_resampler = BuildResampler(src.sample_rate, dst.sample_rate, src.channels);
    } else {
      throw std::runtime_error("SoundStream::SoundStream: when src.sample_rate != dst.sample_rate" +
                               static_cast<std::string>(" either TIN or TOUT has to be float, int16_t or int32_t :("));
    }
  }
}

template <typename TIN, typename TOUT>
std::vector<TOUT> SoundStream<TIN, TOUT>::ReadChunkToVector(const std::vector<TIN>& in_chunk) {
  if constexpr (!std::is_same<TIN, double>::value && !std::is_same<TIN, float>::value &&
                !std::is_same<TIN, int16_t>::value && !std::is_same<TIN, int>::value) {
    throw std::runtime_error("SoundStream::ReadChunk: conversion is not supported :(");
  }
  if constexpr (!std::is_same<TOUT, double>::value && !std::is_same<TOUT, float>::value &&
                !std::is_same<TOUT, int16_t>::value && !std::is_same<TOUT, int>::value) {
    throw std::runtime_error("SoundStream::ReadChunk: conversion is not supported :(");
  }
  std::vector<TOUT> result;

  if (!in_chunk.size()) {
    return {};
  }

  if (ISoundStream<TIN, TOUT>::m_in.channels != ISoundStream<TIN, TOUT>::m_out.channels &&
      ISoundStream<TIN, TOUT>::m_out.channels != 1) {
      throw std::runtime_error(
          "SoundStream::ReadChunk:  in.channels != out.channels and # of channels != 1 (mono)");
  }

  // Resample if TIN
  if (m_in_resample) {
      result = ResampleVec<TIN>(m_resampler,
                                in_chunk,
                                ISoundStream<TIN, TOUT>::m_in.sample_rate,
                                ISoundStream<TIN, TOUT>::m_out.sample_rate);
    }

  return result;
}

template <typename TIN, typename TOUT>
std::vector<TOUT> SoundStream<TIN, TOUT>::ReadChunk(const std::vector<TIN>& in_chunk) {
  if (in_chunk.empty() ||
      ISoundStream<TIN, TOUT>::m_in.sample_rate == ISoundStream<TIN, TOUT>::m_out.sample_rate) {
    return in_chunk;
  }

  const uint32_t threshold = ISoundStream<TIN, TOUT>::m_out.FrameSizeSamples();
  auto resampled_vector = ReadChunkToVector(in_chunk);
  std::move(resampled_vector.begin(),
            resampled_vector.end(),
            std::back_inserter(m_output_buffer));

  std::vector<TOUT> result;
  if (m_output_buffer.size() >= threshold) {
    std::move(std::begin(m_output_buffer),
              std::begin(m_output_buffer) + threshold,
              std::back_inserter(result));
    m_output_buffer.erase(std::begin(m_output_buffer),
                          std::begin(m_output_buffer) + threshold);
  }

  return result;
}

template <typename TIN, typename TOUT>
std::vector<TOUT> SoundStream<TIN, TOUT>::FinalizeStream() {
  std::vector<TOUT> result;
  std::move(std::begin(m_output_buffer),
              std::end(m_output_buffer),
              std::back_inserter(result));

  // resample if TOUT
  if (m_in_resample) {
    if (ISoundStream<TIN, TOUT>::m_in.sample_rate != ISoundStream<TIN, TOUT>::m_out.sample_rate) {
      std::vector<TOUT> flushed_out = FlushVec<TOUT>(m_resampler);
      std::move(std::begin(flushed_out),
                std::end(flushed_out),
                std::back_inserter(result));
    }
  }

  return result;
}

template <typename T>
std::unique_ptr<ISoundStream<T, T>> BuildSoundStream(uint32_t src_rate, uint32_t dst_rate,
                                                     uint32_t frame_size, TemplateType template_type) {
  SoundInfo src;
  SoundInfo dst;

  src.processing_frame_ms = frame_size;
  src.sample_rate = src_rate;
  src.channels = 1;

  dst.processing_frame_ms = frame_size;
  dst.sample_rate = dst_rate;
  dst.channels = 1;

  if (template_type == TemplateType::INPUT) {
    return std::move(BuildSoundStreamTIn<T>(src, dst));
  } else if (template_type == TemplateType::OUTPUT) {
    return std::move(BuildSoundStreamTOut<T>(src, dst));
  } else {
    throw std::runtime_error("SoundStream::BuildSoundStream not able to fetch template type");
  }
}

template <typename TIN>
std::unique_ptr<ISoundStream<TIN, TIN>> BuildSoundStreamTIn(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::move(std::make_unique<SoundStream<TIN, TIN>>(src_info, dst_info));
}

template <typename TOUT>
std::unique_ptr<ISoundStream<TOUT, TOUT>> BuildSoundStreamTOut(const SoundInfo& src_info, const SoundInfo& dst_info) {
  return std::move(std::make_unique<SoundStream<TOUT, TOUT>>(src_info, dst_info));
}

}  // namespace sound
}  // namespace util

// declarations
template class util::sound::SoundStream<float, float>;

template std::unique_ptr<util::sound::ISoundStream<float, float>,
                         std::default_delete<util::sound::ISoundStream<float, float>>>
                              util::sound::BuildSoundStream<float>(uint32_t,
                                                                   uint32_t,
                                                                   uint32_t,
                                                                   util::sound::TemplateType);

template std::unique_ptr<util::sound::ISoundStream<float, float>,
                         std::default_delete<util::sound::ISoundStream<float, float> >>
                              util::sound::BuildSoundStreamTIn<float>(util::sound::SoundInfo const&,
                                                                      util::sound::SoundInfo const&);

template std::unique_ptr<util::sound::ISoundStream<float, float>,
                         std::default_delete<util::sound::ISoundStream<float, float> >>
                              util::sound::BuildSoundStreamTOut<float>(util::sound::SoundInfo const&,
                                                                      util::sound::SoundInfo const&);
