/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <vector>
#include <string>
#include "spench/include/errors.h"
#include "spench/include/stream_stat.h"

namespace spench {
class DenoiseScheduler;
template<typename T>
struct DenoiseContext;

/**
 * \brief: The class that implement audio denoising routine.
 * \ingroup group_cpp
*/
class
#ifdef _MSC_VER
__declspec(dllexport)
#endif
SpenchDenoise {
 public:
  /**
   *  \brief: Constructor which loads model from configuration file.
   *          If no password provided it considers that model is not encrypted.
   *  \param config_file: A pointer to the string with path to .yaml config file.
   *  \param password: A pointer to string with password for a model decryption.
   *                   Could be empty, if a model was not encrypted.
   *  \param checksum: A pointer to string with licence checksum for a model decryption.
   *                   Could be empty, if a model was not encrypted or was not involved
   *                   licencing.
   *  \return: None
   *  \ingroup group_cpp
   */
                                  SpenchDenoise(const char* config_file,
                                               const char* password = "",
                                               const char* checksum = "");

  /**
   *  \brief: Destructor.
   *  \return: None
   *  \ingroup group_cpp
   */
                                  ~SpenchDenoise();
                                  SpenchDenoise(SpenchDenoise&&);
                                  SpenchDenoise(const SpenchDenoise&) = delete;
  SpenchDenoise&                   operator=(const SpenchDenoise&) = delete;

  /**
   * \brief: Reload a feature extractor runner pointer and an inference runner pointer.
   * \ingroup group_cpp
  */
  SpenchRetCode                   ReloadConfig();

  /**
   *  \brief: Retrieve a number of inference engine (onnx, tflite) threads number.
   *  \return: A number of threads.
   *  \ingroup group_cpp
   */
  uint32_t                        GetEngineThreads();

  /**
   *  \brief: Retrieve a model sample rate value, specified in .yaml file.
   *  \return: A model sample rate value.
   *  \ingroup group_cpp
   */
  uint64_t                        GetModelSampleRate();

  /**
   *  \brief: Retrieve a model input size.
   *  \return: A model input size.
   *  \ingroup group_cpp
   */
  uint64_t                        GetModelInputFrameSize();

  /**
   *  \brief: Retrieve a structure with sound stream statistic.
   *  \return: A structure with stat information.
   *  \param: A pointer to a context.
   *  \ingroup group_cpp
   */
  StreamStatStruct                GetStreamStat(const DenoiseContext<float>*);

  // working with context
  /**
   *  \brief: Create a context for audio stream denoising.
   *  \param: An input sample rate of a audio stream. Should be set if
   *          a resampling routine is required.
   *  \param: An output sample rate of a audio stream. Should be set if
   *          a resampling routine is required.
   *  \return: A pointer to a context.
   *  \ingroup group_cpp
   */
  DenoiseContext<float>*          CreateContext(uint32_t in_audio_sr = 0,
                                                uint32_t out_audio_sr = 0);

  /**
   *  \brief: Retrieve an id number of context.
   *  \param: A pointer to a context.
   *  \return: An id number.
   *  \ingroup group_cpp
   */
  uint64_t                        GetContextId(const DenoiseContext<float>*);

  /**
   *  \brief: Set an id number for a context.
   *  \param: A pointer to a context.
   *  \param: A number to be set.
   *  \return: None.
   *  \ingroup group_cpp
   */
  void                            SetContextId(DenoiseContext<float>*,
                                               uint64_t);

  /**
   *  \brief: Deallocate a context memory.
   *  \param: A pointer to a pointer to a context to be destroyed.
   *  \return: None.
   *  \ingroup group_cpp
   */
  void                            DestroyContext(DenoiseContext<float>**);

  // denoising
  /**
   *  \brief: Enhance a chunk of audio with float point data.
   *  \param: A pointer to a context.
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   ProcessChunk(DenoiseContext<float>*,
                                               const std::vector<float>&,
                                               std::vector<float>&);
  /**
   *  \brief: Enhance a chunk of audio with pcm16 data. Implemented through
   *          a call to float point data processing and requires additional
   *          time for convertion.
   *  \param: A pointer to a context.
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   ProcessChunk(DenoiseContext<float>*,
                                               const std::vector<int16_t>&,
                                               std::vector<int16_t>&);

  /**
   *  \brief: Retrieve final chunks with float point data. If model has a future
   *          context, the method enhances and returns remaining data from a
   *          context. In other case method returns empty memory.
   *  \param ctx: A pointer to a context.
   *  \param ret: Memory output data will be allocated.
   *              Should be freed after that. If threre are no data available
   *              NULL will be returned.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   GetFinalChunk(DenoiseContext<float>* ctx,
                                                std::vector<float>& ret);

  /**
   *  \brief: Retrieve final chunks with pcm16 data. If model has a future
   *          context, the method enhances and returns remaining data from a
   *          context. In other case method returns empty memory.
   *  \param ctx: A pointer to a context.
   *  \param ret16: Memory output data will be allocated.
   *                Should be freed after that. If threre are no data available
   *                NULL will be returned.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   GetFinalChunk(DenoiseContext<float>* ctx,
                                                std::vector<int16_t>& ret16);

 private:
  std::unique_ptr<DenoiseScheduler> m_sheduler;
};

namespace tools {
/**
 * \brief: Convert float32 to PCM16.
 * \param: A const vector of input float32 data.
 * \param: A vector of output PCM16 data.
 * \return: None
 * \ingroup group_cpp
*/
#ifdef _MSC_VER
__declspec(dllexport)
#endif
void                              convertFloat32ToPCM16(const std::vector<float>&,
                                                        std::vector<int16_t>&);

/**
 * \brief: Convert PCM16 to float32.
 * \param: A const vector of input PCM16 data.
 * \param: A vector of output float32 data.
 * \return: None
 * \ingroup group_cpp
*/
#ifdef _MSC_VER
__declspec(dllexport)
#endif
void                              convertPCM16ToFloat32(const std::vector<int16_t>&,
                                                        std::vector<float>&);
}  // namespace tools
}  // namespace spench
