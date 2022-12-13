#pragma once

#include <spench/util/sound/resampler.h>
#include <spench/util/sound/sound_types.h>

#include <cstring>
#include <memory>
#include <vector>
#include <deque>

namespace util {
namespace sound {

template <typename TIN, typename TOUT>
class ISoundStream {
 public:
  virtual std::vector<TOUT> ReadChunk(const std::vector<TIN>& in_chunk) = 0;
  virtual ~ISoundStream();
  virtual std::vector<TOUT> FinalizeStream() = 0;
  SoundInfo m_in;
  SoundInfo m_out;

  ISoundStream(const SoundInfo& in, const SoundInfo& out)
      : m_in(in)
      , m_out(out) {}
};

template <typename TIN, typename TOUT>
class SoundStream : public ISoundStream<TIN, TOUT> {
 public:
  SoundStream(const SoundInfo& src, const SoundInfo& dst);
  virtual ~SoundStream();
  std::vector<TOUT> ReadChunk(const std::vector<TIN>& in_chunk) override;
  std::vector<TOUT> ReadChunkToVector(const std::vector<TIN>& in_chunk);
  std::vector<TOUT> FinalizeStream() override;


 private:
  std::deque<TOUT> m_output_buffer;
  ResampleWrapper m_resampler;
  bool m_in_resample;
};

enum TemplateType {
  INPUT = 0,
  OUTPUT = 1
};

template <typename T>
std::unique_ptr<ISoundStream<T, T>> BuildSoundStream(uint32_t src_rate,
                                                     uint32_t dst_rate,
                                                     uint32_t frame_size,
                                                     TemplateType template_type);
template <typename TIN>
std::unique_ptr<ISoundStream<TIN, TIN>> BuildSoundStreamTIn(const SoundInfo& src_info,
                                                            const SoundInfo& dst_info);
template <typename TOUT>
std::unique_ptr<ISoundStream<TOUT, TOUT>> BuildSoundStreamTOut(const SoundInfo& src_info,
                                                               const SoundInfo& dst_info);
}  // namespace sound
}  // namespace util
