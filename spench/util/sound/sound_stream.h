#include <spench/util/sound/resampler.h>

#include <cstring>
#include <memory>
#include <vector>

namespace util {
namespace sound {

class ISoundStream {
 public:
  virtual std::vector<uint8_t> ReadChunk(const std::vector<uint8_t>& in_chunk) = 0;
  virtual ~ISoundStream();
  virtual std::vector<uint8_t> FinalizeStream() = 0;
  // virtual void SetTempo(double tempo) = 0;
  // virtual void SetPitch(double pitch) = 0;
  SoundInfo in;
  SoundInfo out;
  ISoundStream(const SoundInfo& in_, const SoundInfo& out_)
      : in(in_)
      , out(out_) {}
};


template <typename TIN, typename TOUT>
class SoundStream : public ISoundStream {
 public:
  SoundStream(const SoundInfo& src, const SoundInfo& dst, bool add_header, FormatScaling scaling,
              ChannelProcess channel_process, SoundtouchSettings st_settings);
  virtual ~SoundStream();
  std::vector<uint8_t> ReadChunk(const std::vector<uint8_t>& in_chunk) override;
  std::vector<TOUT> ReadChunkToVector(const std::vector<uint8_t>& in_chunk);
  std::vector<uint8_t> FinalizeStream() override;
  // void SetPitch(double pitch) override;
  // void SetTempo(double tempo) override;

 private:
  void ShiftPutSamples(const std::vector<TIN>& chunk);
  std::vector<TIN> ShiftReceiveSamples(size_t buffer_size);
  std::vector<TIN> ShiftProcess(const std::vector<TIN>& chunk);
  // std::vector<uint8_t> EncodeOpusChunk(const std::vector<TOUT>& chunk);
  // std::vector<uint8_t> MakeHeader();
  void PrepareSoundtouch();

  bool first_chunk_state_;
  std::unique_ptr<soundtouch::SoundTouch> stouch_;
  SoundtouchSettings st_settings_;
  SpeexResampleWrapper resampler_;
  // double tempo_ratio_;
  // double pitch_ratio_;
  std::unique_ptr<OpusChunkDecoder> opus_decoder_;
  OpusStreamWrapper opus_wfile_;
  bool in_resample_;
  FormatScaling in_out_format_scaling_;
  ChannelProcess channel_process_method_;
};

enum TemplateType {
  INPUT = 0,
  OUTPUT = 1
};

template <typename T>
std::unique_ptr<ISoundStream> BuildSoundStream(size_t src_rate, size_t dst_rate, int src_audio_encoding,
                                               int dst_audio_encoding, bool add_header, TemplateType template_type,
                                               FormatScaling scaling = FormatScaling::ENABLED,
                                               ChannelProcess channel_process = ChannelProcess::LEAVE_FIRST,
                                               SoundtouchSettings st_settings = {});
template <typename TIN>
std::unique_ptr<ISoundStream> BuildSoundStreamTIn(const SoundInfo& src_info, const SoundInfo& dst_info, bool add_header,
                                                  FormatScaling scaling = FormatScaling::ENABLED,
                                                  ChannelProcess channel_process = ChannelProcess::LEAVE_FIRST,
                                                  SoundtouchSettings st_settings = {});
template <typename TOUT>
std::unique_ptr<ISoundStream> BuildSoundStreamTOut(const SoundInfo& src_info, const SoundInfo& dst_info,
                                                   bool add_header, FormatScaling scaling = FormatScaling::ENABLED,
                                                   ChannelProcess channel_process = ChannelProcess::LEAVE_FIRST,
                                                   SoundtouchSettings st_settings = {});
template <typename TIN, typename TOUT>
std::unique_ptr<ISoundStream> BuildSoundStreamWithType(const SoundInfo& src_info, const SoundInfo& dst_info,
                                                       bool add_header, FormatScaling scaling = FormatScaling::ENABLED,
                                                       ChannelProcess channel_process = ChannelProcess::LEAVE_FIRST,
                                                       SoundtouchSettings st_settings = {});

}  // namespace sound
}  // namespace util
