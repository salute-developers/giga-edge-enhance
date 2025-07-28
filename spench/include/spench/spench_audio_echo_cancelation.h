/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "spench/export.h"
#include "spench/spench_sequencer.h"

namespace spench {

class AudioEchoCancelationHandler;

template <typename T>
struct SpenchContext;

/**
 * \brief: The class that implement acoustic echo cancelation routine.
 * \ingroup group_cpp
 */
class SPENCH_API SpenchAudioEchoCancelation : public ISpenchSequencer {
 public:
  // echo cancelation
  /**
   *  \brief: Enhance a chunk of audio with float point data.
   *  \param: A const vector of input data.
   *  \param: Output vector.
   *  \param: A pointer to a context. Should be nullptr.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode ProcessChunk(const std::vector<float>& interleaved_chunk, std::vector<float>& ret,
                             SpenchContext<float>* ctx = nullptr) final;

  /**
   *  \brief: Enhance a chunk of audio with pcm16 data. Internally implemented as
   *          a call of ProcessChunk for fp32 data with internal convertion.
   *  \param: A const vector of input data.
   *  \param: Output vector.
   *  \param: A pointer to a context. Should be nullptr.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode ProcessChunk(const std::vector<int16_t>& interleaved_chunk16, std::vector<int16_t>& ret16,
                             SpenchContext<float>* ctx = nullptr) final;

  /**
   *  \brief: Enhance a chunk of audio with float point data.
   *  \param: A const vector of input data.
   *  \param: Output vector.
   *  \param: A pointer to a context. Should be nullptr.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode ProcessChunk(const std::vector<float>& chunk, const std::vector<float>& lbk, std::vector<float>& ret);

  std::vector<float> operator()(const std::vector<float>& interleaved_chunk, std::vector<float>& ret,
                                SpenchContext<float>* ctx = nullptr) final;

  std::vector<int16_t> operator()(const std::vector<int16_t>& interleaved_chunk16, std::vector<int16_t>& ret16,
                                  SpenchContext<float>* ctx = nullptr) final;

  std::vector<float> operator()(std::vector<float>& interleaved_chunk, SpenchContext<float>* ctx = nullptr) final;

  std::vector<int16_t> operator()(std::vector<int16_t>& interleaved_chunk16, SpenchContext<float>* ctx = nullptr) final;
  /**
   *  \brief: Empty method. Is not Used.
   *  \return: Error code. SpenchRetCode::SUCCESS.
   *  \ingroup group_cpp
   */
  SpenchRetCode GetFinalChunk(std::vector<float>& interleaved_chunk, SpenchContext<float>* ctx = nullptr) final;

  /**
   *  \brief: Empty method. Is not Used.
   *  \return: Error code. SpenchRetCode::SUCCESS.
   *  \ingroup group_cpp
   */
  SpenchRetCode GetFinalChunk(std::vector<int16_t>& interleaved_chunk16, SpenchContext<float>* ctx = nullptr) final;

 public:
  /**
   *  \brief: Constructor which could take parameters for aec object from config file.
   *  \param config_file: A pointer to the string with path to .yaml config file.
   *  \return: None
   *  \ingroup group_cpp
   */
  explicit SpenchAudioEchoCancelation(const char* config_file);
  /**
   *  \brief: Constructor which could take parameters for aec object from config file.
   *  \param yaml_conf: A string with YAML configuration. Only provided parameters will be changed. Other ones
   *                    will be passed by default.
   *  \return: None
   *  \ingroup group_cpp
   */
  explicit SpenchAudioEchoCancelation(const std::string& yaml_conf);
  /**
   *  \brief: Destructor.
   *  \return: None
   *  \ingroup group_cpp
   */

  ~SpenchAudioEchoCancelation();
  /**
   *  \brief: Default move constructor.
   *  \return: None
   *  \ingroup group_cpp
   */
  SpenchAudioEchoCancelation(SpenchAudioEchoCancelation&&) noexcept;

  SpenchAudioEchoCancelation(const SpenchAudioEchoCancelation&) = delete;
  SpenchAudioEchoCancelation& operator=(const SpenchAudioEchoCancelation&) = delete;

  /**
   *  \brief: Retrieve a sample rate setup for AEC.
   *  \return: A sample rate value.
   *  \ingroup group_cpp
   */

  const uint32_t GetSampleRate();

  /**
   *  \brief: Retrieve an AEC filter length.
   *  \return: A sample rate value.
   *  \ingroup group_cpp
   */
  const uint16_t GetFilterLength();

  /**
   *  \brief: Retrieve an audio frame length setup for AEC processing.
   *  \return: A sample rate value.
   *  \ingroup group_cpp
   */
  const uint16_t GetFrameLength();

 private:
  /**
   *  \brief: Process a chunk of stereo audio with float point data.
   *  \param: A const vector fp32 of input data.
   *  \param: A const vector fp32 of loppback data.
   *  \param: Vector for output data.
   *  \return: Reference to out.
   *  \ingroup group_cpp
   */
  std::vector<float> process(const std::vector<float>& mic, const std::vector<float>& lbk, std::vector<float>& out);
  /**
   *  \brief: Process a chunk of stereo audio withpcm16 data.
   *          Requires additional time for convertion ot fp32 and vice versa.
   *  \param: A const vector int16 of input data.
   *  \param: A const vector int16 of loopback data.
   *  \param: Vector for output data.
   *  \return: Reference to out.
   *  \ingroup group_cpp
   */
  std::vector<int16_t> process(const std::vector<int16_t>& mic, const std::vector<int16_t>& lbk,
                               std::vector<int16_t>& out);

  /**
   *  \brief: Convert original signal and loopback into an interleaved format
   *  \param: Vector for input audio.
   *  \param: Vector for loopback data.
   *  \return: Vector with interleaved audio.
   *  \ingroup group_cpp
   */
  std::vector<float> convertToInterleaved(const std::vector<float>& mic, const std::vector<float>& lbk);

  /**
   *  \brief: Convert interleaved audio back into separate original and loopback signals
   *  \param: Vector with input interleaved audio.
   *  \param: Vector to return input data.
   *  \param: Vector to return loopback data.
   *  \return: None
   *  \ingroup group_cpp
   */
  void convertFromInterleaved(const std::vector<float>& interlvd, std::vector<float>& mic, std::vector<float>& lbk);
  std::unique_ptr<AudioEchoCancelationHandler> m_aec;
};
}  // namespace spench
