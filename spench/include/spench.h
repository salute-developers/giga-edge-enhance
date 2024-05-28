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
class AudioProcessing;
template<typename T>
struct DenoiseContext;

class
#ifdef _MSC_VER
__declspec(dllexport)
#endif
ISpenchSequencer {
 public:
  ISpenchSequencer() = default;
  ISpenchSequencer(ISpenchSequencer&) = delete;
  ISpenchSequencer(ISpenchSequencer&&) = delete;
  ISpenchSequencer& operator=(const ISpenchSequencer&) = delete;
  virtual ~ISpenchSequencer() = default;

  virtual SpenchRetCode ProcessChunk(const std::vector<float>& chunk,
                                     std::vector<float>& ret,
                                     DenoiseContext<float>* ctx = nullptr) = 0;

  virtual SpenchRetCode ProcessChunk(const std::vector<int16_t>& chunk16,
                                     std::vector<int16_t>& ret16,
                                     DenoiseContext<float>* ctx = nullptr) = 0;

  virtual std::vector<float>& operator()(const std::vector<float>& chunk,
                                         std::vector<float>& ret,
                                         DenoiseContext<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t>& operator()(const std::vector<int16_t>& chunk16,
                                           std::vector<int16_t>& ret16,
                                           DenoiseContext<float>* ctx = nullptr) = 0;

  virtual SpenchRetCode GetFinalChunk(std::vector<float>& ret,
                                      DenoiseContext<float>* ctx = nullptr) = 0;

  virtual SpenchRetCode GetFinalChunk(std::vector<int16_t>& ret16,
                                      DenoiseContext<float>* ctx = nullptr) = 0;

  virtual std::vector<float>& operator()(std::vector<float>& ret,
                                         DenoiseContext<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t>& operator()(std::vector<int16_t>& ret16,
                                           DenoiseContext<float>* ctx = nullptr) = 0;
};

/**
 * \brief: The class that implement audio denoising routine.
 * \ingroup group_cpp
*/
class
#ifdef _MSC_VER
__declspec(dllexport)
#endif
SpenchDenoise : public ISpenchSequencer {
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
  SpenchDenoise&                  operator=(const SpenchDenoise&) = delete;

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
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \param: A pointer to a context.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   ProcessChunk(const std::vector<float>&,
                                               std::vector<float>&,
                                               DenoiseContext<float>*) override;
  /**
   *  \brief: Enhance a chunk of audio with pcm16 data. Implemented as
   *          a call of analouge for fp32 data processing and requires additional
   *          time for convertion.
   *  \param: A const vector of input data.
   *  \param: Memory output data will be allocated.
   *                     Should be freed after that.
   *  \param: A pointer to a context.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   ProcessChunk(const std::vector<int16_t>&,
                                               std::vector<int16_t>&,
                                               DenoiseContext<float>*) override;

  std::vector<float>&             operator()(const std::vector<float>& chunk,
                                         std::vector<float>& ret,
                                         DenoiseContext<float>* ctx) override;

  std::vector<int16_t>&           operator()(const std::vector<int16_t>& chunk16,
                                             std::vector<int16_t>& ret16,
                                             DenoiseContext<float>* ctx) override;
  /**
   *  \brief: Retrieve final chunks with float point data. If model has a future
   *          context, the method enhances and returns remaining data from a
   *          context. In other case method returns empty memory.
   *  \param ret: Memory output data will be allocated.
   *              Should be freed after that. If threre are no data available
   *              NULL will be returned.
   *  \param ctx: A pointer to a context.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   GetFinalChunk(std::vector<float>& ret,
                                                DenoiseContext<float>* ctx) override;

  /**
   *  \brief: Retrieve final chunks with pcm16 data. If model has a future
   *          context, the method enhances and returns remaining data from a
   *          context. In other case method returns empty memory.
   *  \param ret16: Memory output data will be allocated.
   *                Should be freed after that. If threre are no data available
   *                NULL will be returned.
   *  \param ctx: A pointer to a context.
   *  \return: Error code. SpenchRetCode::SUCCESS means success.
   *  \ingroup group_cpp
   */
  SpenchRetCode                   GetFinalChunk(std::vector<int16_t>& ret16,
                                                DenoiseContext<float>* ctx) override;

  std::vector<float>&             operator()(std::vector<float>& ret,
                                             DenoiseContext<float>* ctx) override;

  std::vector<int16_t>&           operator()(std::vector<int16_t>& ret16,
                                             DenoiseContext<float>* ctx) override;

 private:
  std::unique_ptr<DenoiseScheduler> m_sheduler;
};

/**
 * \brief: The class that implement audio processing routine.
 *         This is the sequence of objects that can be added one by one to modify
 *         sound by dsp algorithms.
 * \ingroup group_cpp
*/
class
#ifdef _MSC_VER
__declspec(dllexport)
#endif
SpenchAudioProcess : public ISpenchSequencer {
 public:
  SpenchRetCode ProcessChunk(const std::vector<float>& chunk,
                             std::vector<float>& ret,
                             DenoiseContext<float>* ctx = nullptr) override;

  SpenchRetCode ProcessChunk(const std::vector<int16_t>& chunk16,
                             std::vector<int16_t>& ret16,
                             DenoiseContext<float>* ctx = nullptr) override;

  std::vector<float>& operator() (const std::vector<float>& chunk,
                                  std::vector<float>& ret,
                                  DenoiseContext<float>* ctx = nullptr) override;

  std::vector<int16_t>& operator() (const std::vector<int16_t>& chunk16,
                                    std::vector<int16_t>& ret16,
                                    DenoiseContext<float>* ctx = nullptr) override;

  SpenchRetCode GetFinalChunk(std::vector<float>& ret,
                              DenoiseContext<float>* ctx = nullptr) override;

  SpenchRetCode GetFinalChunk(std::vector<int16_t>& ret16,
                              DenoiseContext<float>* ctx = nullptr) override;

  std::vector<float>& operator()(std::vector<float>& ret,
                                 DenoiseContext<float>* ctx = nullptr) override;

  std::vector<int16_t>& operator() (std::vector<int16_t>& chunk16,
                                    DenoiseContext<float>* ctx = nullptr) override;

 public:
  /**
   *  \brief: Default constructor.
   *  \return: None
   *  \ingroup group_cpp
   */
                                  SpenchAudioProcess();

  /**
   *  \brief: Constructor which could take parameters for dsp objects from config file.
   *          All parameters are reading from config file when AGC, RC, etc are being
   *          created.
   *  \param config_file: A pointer to the string with path to .yaml config file.
   *  \return: None
   *  \ingroup group_cpp
   */
  explicit                        SpenchAudioProcess(const char* config_file);

  /**
   *  \brief: Destructor.
   *  \return: None
   *  \ingroup group_cpp
   */
                                  ~SpenchAudioProcess();
                                  SpenchAudioProcess(SpenchAudioProcess&&) = delete;
                                  SpenchAudioProcess(const SpenchAudioProcess&) = delete;
  SpenchAudioProcess&             operator=(const SpenchAudioProcess&) = delete;

  /**
   *  \brief: Create and Add Automatic Gain Control object to Audio Process with default
   *          parameters optimized for audio sample rate 48000Hz, or specified
   *          in config file if it was provided for SpenchAudioProcess constructor.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddAGC();

  /**
   *  \brief: Create and Add Automatic Gain Control object to Audio Process with specific
   *          parameters.
   *  \param: An audio sample rate.
   *  \param: Desired target power level.
   *  \param: Max power gain.
   *  \param: Step size for gain updates in sec (rise).
   *  \param: Step size for gain updates in sec (fall).
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddAGC(uint32_t audio_sample_rate,
                                         float target_power_level = 1.0,
                                         float max_power_gain = 20.0,
                                         float time_rise = 1e-2,
                                         float time_fall = 1e-2);

  /**
   *  \brief: Create and Add Range Control object to Audio Process with default parameters
   *          optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddRC();

  /**
   *  \brief: Create and Add Range Control object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate.
   *  \param: Max level in db. 0db means that range control do not provide control effect.
   *  \param: Step size in sec.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddRC(uint32_t audio_sample_rate,
                                        float max_level_db = -3.0,
                                        float step_size = 1e-4);
  
 /**
   *  \brief: Create and Add Noise Gate object to Audio Process with default parameters
   *          optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddNG();

  /**
   *  \brief: Create and Add Noise Gate object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate.
   *  \param: Threshold in db.
   *  \param: Attack time coeff in sec.
   *  \param: Release time coeff in sec.
   *  \param: Hold time in sec.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddNG(uint32_t audio_sample_rate,
                                        float threshold_db = -15.0,
                                        float attack_time = 5e-2,
                                        float release_time = 2e-2,
                                        float hold_time = 5e-2);

  /**
   *  \brief: Create and Add Dynamic Range Compressor object to Audio Process with default
   *          parameters optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddDRC();

  /**
   *  \brief: Create and Add Dynamic Range Compressor object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate.
   *  \param: Threshold in db.
   *  \param: Ratio. Defines the amount of gain reduction.
   *  \param: Knee width coeff in sec.
   *  \param: Attack time coeff in sec.
   *  \param: Release time coeff in sec.
   *  \return: None.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddDRC(uint32_t audio_sample_rate,
                                         float threshold_db = -15.0,
                                         uint32_t ratio = 5,
                                         float knee_width = 0.0,
                                         float attack_time = 1e-2,
                                         float release_time = 2e-1);

private:
  /**
   *  \brief: Process a chunk of audio with float point data via sequence of dsp objects.
   *  \param: A const vector of input data.
   *  \param: Vector for output data.
   *  \return: Reference to out.
   *  \ingroup group_cpp
   */
  std::vector<float>&              process(const std::vector<float>& in,
                                                 std::vector<float>& out);
  /**
   *  \brief: Process a chunk of audio withpcm16 data via sequence of dsp objects.
   *          Requires additional time for convertion ot fp32 and vice versa.
   *  \param: A const vector of input data.
   *  \param: Vector for output data.
   *  \return: Reference to out.
   *  \ingroup group_cpp
   */
  std::vector<int16_t>&           process(const std::vector<int16_t>& in,
                                                std::vector<int16_t>& out);


  std::unique_ptr<AudioProcessing> m_dsp;
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
