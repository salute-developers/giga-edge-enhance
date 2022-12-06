#pragma once

#include <memory>
#include <vector>
#include <string>
#include "spench/include/errors.h"

namespace spench {
class DenoiseScheduler;
template<typename T>
struct DenoiseContext;

/**
 * \brief: The class that implement audio denoising routine.
*/
class AudioDenoise {
 public:
  /**
   *  \brief: Constructor which loads model from configuration file.
   *          If no password provided it considers that model is not encrypted.
   *  \param config_file: A pointer to the string with path to .yaml config file.
   *  \param password: A pointer to the string with password for the model decryption.
   *  \out: None
   */
                                  AudioDenoise(const char* config_file,
                                               const char* password = "");
  /**
   *  \brief: Destructor.
   *  \out: None
   */
                                  ~AudioDenoise();
                                  AudioDenoise(const AudioDenoise&) = delete;
  AudioDenoise&                   operator=(const AudioDenoise&) = delete;

  /**
   * \brief: Reload a feature extractor runner pointer and an inference runner pointer.
  */
  SpenchRetCode                   ReloadConfig();

  /**
   *  \brief: Retrieve a number of inference engine (onnx, tflite) threads number.
   *  \out: A number of threads.
   */
  uint32_t                        GetEngineThreads();

  /**
   *  \brief: Retrieve a model sample rate value, specified in .yaml file.
   *  \out: A model sample rate value.
   */
  uint64_t                        GetModelSampleRate();

  /**
   *  \brief: Retrieve a model input size.
   *  \out: A model input size.
   */
  uint64_t                        GetModelInputFrameSize();

  // working with context
  /**
   *  \brief: Create a context for audio stream denoising.
   *  \out: A pointer to a context.
   */
  DenoiseContext<float>*          CreateContext();

  /**
   *  \brief: Retrieve an id number of context.
   *  \param: A pointer to a context.
   *  \out: An id number.
   */
  uint64_t                        GetContextId(const DenoiseContext<float>*);

  /**
   *  \brief: Set an id number for a context.
   *  \param: A pointer to a context.
   *  \param: A number to be set.
   *  \out: None.
   */
  void                            SetContextId(DenoiseContext<float>*,
                                               uint64_t);
  /**
   *  \brief: Deallocate a context memory.
   *  \param: A pointer to a pointer to a context to be destroyed.
   *  \out: None.
   */
  void                            DestroyContext(DenoiseContext<float>**);

  // denoising
  /**
   *  \brief: Enhance a chunk of audio with float point data.
   *  \param: A pointer to a context.
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \out: Error code. SpenchRetCode::SUCCESS means success.
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
   *  \out: Error code. SpenchRetCode::SUCCESS means success.
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
   *  \out: Error code. SpenchRetCode::SUCCESS means success.
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
   *  \out: Error code. SpenchRetCode::SUCCESS means success.
   */
  SpenchRetCode                   GetFinalChunk(DenoiseContext<float>* ctx,
                                                std::vector<int16_t>& ret16);

 private:
  DenoiseScheduler*               m_sheduler;
};

namespace tools {
/**
 * \brief: Convert float32 to PCM16.
 * \param: A const vector of input float32 data.
 * \param: A vector of output PCM16 data.
 * \out: None
*/
void                              convertFloat32ToPCM16(const std::vector<float>&,
                                                        std::vector<int16_t>&);

/**
 * \brief: Convert PCM16 to float32.
 * \param: A const vector of input PCM16 data.
 * \param: A vector of output float32 data.
 * \out: None
*/
void                              convertPCM16ToFloat32(const std::vector<int16_t>&,
                                                        std::vector<float>&);
}  // namespace tools
}  // namespace spench
