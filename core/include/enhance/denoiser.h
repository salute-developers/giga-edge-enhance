/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "enhance/export.h"
#include "enhance/packed_model.h"
#include "enhance/sequencer.h"

namespace giga_edge::enhance {

class DenoiserAudioHandler;

template <typename T>
struct Context;

/**
 * \brief: The class that implement audio denoising routine.
 * \ingroup group_cpp
 */
class GEE_API Denoiser : public Sequencer {
 public:
  /**
   *  \brief: Constructor which loads model from configuration file.
   *  \param config_file: A pointer to the string with path to .yaml config file.
   *  \return: None
   *  \ingroup group_cpp
   */
  explicit Denoiser(const char* config_file);

  /**
   *  \brief: Constructor which loads model from YAML string configuration.
   *  \param yaml_str: A string with YAML configuration. Only provided parameters will be changed. Other ones
   *                    will be passed by default.
   *  \return: None
   *  \ingroup group_cpp
   */
  explicit Denoiser(const std::string& yaml_str);

  /**
   *  \brief: Constructor which loads model from packed model reader.
   *  \param bundle: A PackedModelReader instance.
   *  \return: None
   *  \ingroup group_cpp
   */
  explicit Denoiser(std::shared_ptr<giga_edge::enhance::PackedModelReader> bundle);

  /**
   *  \brief: Destructor.
   *  \return: None
   *  \ingroup group_cpp
   */
  ~Denoiser();
  Denoiser(Denoiser&&) noexcept;
  Denoiser(const Denoiser&) = delete;
  Denoiser& operator=(const Denoiser&) = delete;

  /**
   * \brief: Reload a feature extractor runner pointer and an inference runner pointer.
   * \ingroup group_cpp
   */
  GigaEdgeEnhanceRetCode ReloadConfig();

  /**
   *  \brief: Retrieve a number of inference engine (onnx, tflite) threads number.
   *  \return: A number of threads.
   *  \ingroup group_cpp
   */
  uint32_t GetEngineThreads();

  /**
   *  \brief: Retrieve a model sample rate value, specified in .yaml file.
   *  \return: A model sample rate value.
   *  \ingroup group_cpp
   */
  uint64_t GetModelSampleRate();

  /**
   *  \brief: Retrieve a model input size.
   *  \return: A model input size.
   *  \ingroup group_cpp
   */
  uint64_t GetModelInputFrameSize();

  /**
   *  \brief: Retrieve a structure with sound stream statistic.
   *  \return: A structure with stat information.
   *  \param: A pointer to a context.
   *  \ingroup group_cpp
   */
  StreamStatStruct GetStreamStat(const Context<float>*);

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
  Context<float>* CreateContext(uint32_t in_audio_sr = 0, uint32_t out_audio_sr = 0);

  /**
   *  \brief: Retrieve an id number of context.
   *  \param: A pointer to a context.
   *  \return: An id number.
   *  \ingroup group_cpp
   */
  uint64_t GetContextId(const Context<float>*);

  /**
   *  \brief: Set an id number for a context.
   *  \param: A pointer to a context.
   *  \param: A number to be set.
   *  \return: None.
   *  \ingroup group_cpp
   */
  void SetContextId(Context<float>*, uint64_t);

  /**
   *  \brief: Deallocate a context memory.
   *  \param: A pointer to a pointer to a context to be destroyed.
   *  \return: None.
   *  \ingroup group_cpp
   */
  void DestroyContext(Context<float>**);

  // denoising
  /**
   *  \brief: Enhance a chunk of audio with float point data.
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \param: A pointer to a context.
   *  \return: Error code. GigaEdgeEnhanceRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  GigaEdgeEnhanceRetCode ProcessChunk(const std::vector<float>&, std::vector<float>&, Context<float>*) final;
  /**
   *  \brief: Enhance a chunk of audio with pcm16 data. Implemented as
   *          a call of analouge for fp32 data processing and requires additional
   *          time for convertion.
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \param: A pointer to a context.
   *  \return: Error code. GigaEdgeEnhanceRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  GigaEdgeEnhanceRetCode ProcessChunk(const std::vector<int16_t>&, std::vector<int16_t>&, Context<float>*) final;

  std::vector<float> operator()(const std::vector<float>& chunk, std::vector<float>& ret, Context<float>* ctx) final;

  std::vector<int16_t> operator()(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                  Context<float>* ctx) final;
  /**
   *  \brief: Retrieve final chunks with float point data. If model has a future
   *          context, the method enhances and returns remaining data from a
   *          context. In other case method returns empty memory.
   *  \param ret: Memory output data will be allocated.
   *              Should be freed after that. If threre are no data available
   *              NULL will be returned.
   *  \param ctx: A pointer to a context.
   *  \return: Error code. GigaEdgeEnhanceRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  GigaEdgeEnhanceRetCode GetFinalChunk(std::vector<float>& ret, Context<float>* ctx) final;

  /**
   *  \brief: Retrieve final chunks with pcm16 data. If model has a future
   *          context, the method enhances and returns remaining data from a
   *          context. In other case method returns empty memory.
   *  \param ret16: Memory output data will be allocated.
   *                Should be freed after that. If threre are no data available
   *                NULL will be returned.
   *  \param ctx: A pointer to a context.
   *  \return: Error code. GigaEdgeEnhanceRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  GigaEdgeEnhanceRetCode GetFinalChunk(std::vector<int16_t>& ret16, Context<float>* ctx) final;

  std::vector<float> operator()(std::vector<float>& ret, Context<float>* ctx) final;

  std::vector<int16_t> operator()(std::vector<int16_t>& ret16, Context<float>* ctx) final;

 private:
  std::unique_ptr<DenoiserAudioHandler> handler_;
};
}  // namespace giga_edge::enhance
