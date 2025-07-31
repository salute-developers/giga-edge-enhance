/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <vector>
#include <string>

#include "spench/spench_sequencer.h"
#include "spench/export.h"

namespace spench {
  
class AudioProcessing;

template<typename T>
struct SpenchContext;

/**
 * \brief: The class that implement audio processing routine.
 *         This is the sequence of objects that can be added one by one to modify
 *         sound by dsp algorithms.
 * \ingroup group_cpp
*/
class SPENCH_API SpenchAudioProcess : public ISpenchSequencer {
 public:
  SpenchRetCode ProcessChunk(const std::vector<float>& chunk,
                             std::vector<float>& ret,
                             SpenchContext<float>* ctx = nullptr) final;

  SpenchRetCode ProcessChunk(const std::vector<int16_t>& chunk16,
                             std::vector<int16_t>& ret16,
                             SpenchContext<float>* ctx = nullptr) final;

  std::vector<float>  operator() (const std::vector<float>& chunk,
                                  std::vector<float>& ret,
                                  SpenchContext<float>* ctx = nullptr) final;

  std::vector<int16_t>  operator() (const std::vector<int16_t>& chunk16,
                                    std::vector<int16_t>& ret16,
                                    SpenchContext<float>* ctx = nullptr) final;

  SpenchRetCode GetFinalChunk(std::vector<float>& ret,
                              SpenchContext<float>* ctx = nullptr) final;

  SpenchRetCode GetFinalChunk(std::vector<int16_t>& ret16,
                              SpenchContext<float>* ctx = nullptr) final;

  std::vector<float>  operator()(std::vector<float>& ret,
                                 SpenchContext<float>* ctx = nullptr) final;

  std::vector<int16_t>  operator() (std::vector<int16_t>& chunk16,
                                    SpenchContext<float>* ctx = nullptr) final;

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
   *  \return: SpenchAudioProcess object reference.
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
   *  \return: SpenchAudioProcess object reference.
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
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddRC();

  /**
   *  \brief: Create and Add Loudness Control object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate (Hz).
   *  \param: An target peak value (db).
   *  \param: A value of the time in sec bypassed for Loudness controll configuring
   *  \param: A max output signal value (db). 0db means that range control do not provide control effect.
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */

  SpenchAudioProcess&             AddLC(uint32_t audio_sample_rate,
                                        float target_level_db = -23,
                                        float bypass_time = 3,
                                        float max_gain_db = 40);

  /**
   *  \brief: Create and Add Loudness Control object to Audio Process with default parameters
   *          optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddLC();

  /**
   *  \brief: Create and Add Range Control object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate (Hz).
   *  \param: An target peak value (db).
   *  \param: A max output signal value (db). 0db means that range control do not provide control effect.
   *  \param: An attack time for peak value estimation (sec)
   *  \param: A release time for peak value estimation (sec)
   *  \param: A hold time for peak value estimation (sec)
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */

  SpenchAudioProcess&             AddRC(uint32_t audio_sample_rate,
                                        float target_level_db = -3,
                                        float max_gain_db = 10,
                                        float attack_time = 1e-5,
                                        float release_time = 300,
                                        float hold_time = 5);


  /**
   *  \brief: Create and Add Noise Gate object to Audio Process with default parameters
   *          optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddNG();

  /**
   *  \brief: Create and Add Noise Gate object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate (Hz).
   *  \param: Threshold (db).
   *  \param: Attack time coeff (sec).
   *  \param: Release time coeff (sec).
   *  \param: Hold time (sec).
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddNG(uint32_t audio_sample_rate,
                                        float threshold_db = -15.0,
                                        float attack_time = 5e-2,
                                        float release_time = 2e-2,
                                        float hold_time = 5e-2);

  /**
   *  \brief: Create and Add Limiter object to Audio Process with default parameters
   *          optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddLim();

  /**
   *  \brief: Create and Add Limiter object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate (Hz).
   *  \param: Threshold (db).
   *  \param: Knee width coeff (sec).
   *  \param: Attack time coeff (sec).
   *  \param: Release time coeff (sec).
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddLim(uint32_t audio_sample_rate,
                                        float threshold_db = -3.0,
                                        float knee_width = 0.0,
                                        float attack_time = 0.0,
                                        float release_time = 0.2);

  /**
   *  \brief: Create and Add Dynamic Range Compressor object to Audio Process with default
   *          parameters optimized for audio sample rate 48000Hz, or specified in config file
   *          if it was provided for SpenchAudioProcess constructor.
   *  \return: SpenchAudioProcess object reference.
   *  \ingroup group_cpp
   */
  SpenchAudioProcess&             AddDRC();

  /**
   *  \brief: Create and Add Dynamic Range Compressor object to Audio Process with specific
   *          parameters. They will applied for an object even config file was provided.
   *  \param: An audio sample rate (Hz).
   *  \param: Threshold (db).
   *  \param: Ratio. Defines the amount of gain reduction.
   *  \param: Knee width coeff (sec).
   *  \param: Attack time coeff (sec).
   *  \param: Release time coeff (sec).
   *  \return: SpenchAudioProcess object reference.
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
  std::vector<float>               process(const std::vector<float>& in,
                                                 std::vector<float>& out);
  /**
   *  \brief: Process a chunk of audio withpcm16 data via sequence of dsp objects.
   *          Requires additional time for convertion ot fp32 and vice versa.
   *  \param: A const vector of input data.
   *  \param: Vector for output data.
   *  \return: Reference to out.
   *  \ingroup group_cpp
   */
  std::vector<int16_t>            process(const std::vector<int16_t>& in,
                                                std::vector<int16_t>& out);


  std::unique_ptr<AudioProcessing> m_dsp;
};
}  // namespace spench
