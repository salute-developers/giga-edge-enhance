/**
 * \file
 * \defgroup group_c C API
 */
#ifndef __SPENCH_C_WRAPPER_HEADER__
#define __SPENCH_C_WRAPPER_HEADER__

#include <stdlib.h>
#include "spench/errors.h"
#include "spench/stream_stat.h"
#if defined(EMSCRIPTEN) || defined(WIN32)
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SPENCH_EXPORT
# if defined(__GNUC__)
#  define SPENCH_EXPORT __attribute__ ((visibility ("default")))
# elif defined(_MSC_VER)
#  define SPENCH_EXPORT __declspec(dllexport)
# else
#  define SPENCH_EXPORT
# endif
#endif

typedef void* SpenchDenoise;
typedef void* SpenchContext;
typedef void* SpenchAudioProcess;

#if defined(EMSCRIPTEN) || defined(WIN32)
typedef uint64_t u_int64_t;
typedef uint32_t u_int32_t;
typedef uint16_t u_int16_t;
typedef uint8_t  u_int8_t;
#endif

/**
 *    SpenchDenoise
 */

/**
 *  \brief: Allocate and return a model for audio enhancement.
 *  \param config_file: Pointer to string with path to .yaml config file.
 *  \param SpenchDenoise: Pointer to pointer to model to be allocated.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseInit(const char* config_file,
                                                SpenchDenoise* model);

/**
 *  \brief: Allocate and return an encrypted model for audio enhancement.
 *  \param config_file: A pointer to string with path to .yaml config file.
 *  \param password: A pointer to string with password for a model decryption.
 *                   Could be empty, if a model was not encrypted.
 *  \param checksum: A pointer to string with licence checksum for a model decryption.
 *                   Could be empty, if a model was not encrypted or was not involved
 *                   licencing.
 *  \param SpenchDenoise: A pointer to pointer to model to be allocated.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode SpenchEncryptedDenoiseInit(const char* config_file,
                                                       const char* password,
                                                       const char* checksum,
                                                       SpenchDenoise* model);

/**
 *  \brief: Deallocate a model structure.
 *  \param SpenchDenoise: A pointer to pointer to model to be destroyed.
 *  \return: None
 *  \ingroup group_c
 */
SPENCH_EXPORT void            SpenchDenoiseFree(SpenchDenoise* model);

/**
 *  \brief: Create a context for audio stream denoising.
 *  \param SpenchDenoise: A pointer to model.
 *  \param ctx: A pointer to pointer to context.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 *  \param in_audio_sr: An input sample rate of a audio stream.
 *                      If the resampling is not required, please set to 0.
 *  \param out_audio_sr: An output sample rate of a audio stream.
 *                       If the resampling is not required, please set to 0.
 *  \out: Error code. SpenchRetCode::SUCCESS means success.
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseCreateContext(SpenchDenoise model,
                                                         SpenchContext* ctx,
                                                         u_int32_t in_audio_sr,
                                                         u_int32_t out_audio_sr);

/**
 *  \brief: Deallocate a context memory.
 *  \param SpenchDenoise: pointer to a model.
 *  \param ctx: A pointer to pointer to context to be destroyed.
 *  \return: None.
 *  \ingroup group_c
 */
SPENCH_EXPORT void            SpenchDenoiseDestroyContext(SpenchDenoise model,
                                                          SpenchContext* ctx);

/**
 *  \brief: Retrieve the id number of a context.
 *  \param model: A pointer to a model.
 *  \param ctx: A pointer to a context.
 *  \param id: A pointer to returning id number.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetContextId(SpenchDenoise model,
                                                        const SpenchContext ctx,
                                                        u_int64_t* id);
/**
 *  \brief: Set the id number for a context.
 *  \param model: A pointer to a model.
 *  \param ctx: A pointer to a context.
 *  \param id: Number to be set.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseSetContextId(SpenchDenoise model,
                                                        SpenchContext ctx,
                                                        u_int64_t id);

/**
 *  \brief: Retrieve the SNR stat of a context.
 *  \param model: A pointer to a model.
 *  \param ctx: A pointer to a context.
 *  \param snr: A pointer to returning SNR value.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetStreamStat(SpenchDenoise model,
                                                         const SpenchContext ctx,
                                                         StreamStatStruct* stream_stat);

/**
 *  \brief: Retrieve the number of inference engine (onnx, tflite) threads number.
 *  \param model: A pointer to a model.
 *  \param engine_threads: A pointer to returning number of threads.
 *  \return: Code of error. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetEngineThreads(const SpenchDenoise model,
                                                            u_int64_t* engine_threads);

/**
 *  \brief: Retrieve the model input size.
 *  \param model: A pointer to a model.
 *  \param input_size: A pointer to returning number of threads.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetInputFrameSize(const SpenchDenoise model,
                                                             u_int64_t* input_size);

/**
 *  \brief: Retrieve the model sample rate value, specified in .yaml file.
 *  \param model: A pointer to a model.
 *  \param sample_rate: A pointer to returning sample rate value.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetSampleRate(const SpenchDenoise model,
                                                         u_int64_t* sample_rate);

/**
 *  \brief: Enhance a chunk of audio with float point data.
 *  \param model: A pointer to a model.
 *  \param ctx: A pointer to context.
 *  \param memory_in: A pointer to input data.
 *  \param in_size: The size of input data in bytes.
 *  \param memory_out: A pointer to memory output data will be allocated.
 *                     Should be freed after that.
 *  \param out_size: A pointer to variable where the size of allocated
 *                   data in bytes will be provided.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseProcessChunkFp(SpenchDenoise model,
                                                          SpenchContext ctx,
                                                          const u_int8_t* memory_in,
                                                          u_int64_t in_size,
                                                          u_int8_t** memory_out,
                                                          u_int64_t* out_size);

/**
 *  \brief: Enhance a chunk of audio with pcm16 data. Implemented through
 *          a call to float point data processing and requires additional
 *          time for convertion.
 *  \param model: A pointer to model.
 *  \param ctx: A pointer to context.
 *  \param memory_in: A pointer to input data.
 *  \param in_size: A size of input data in bytes.
 *  \param memory_out: A pointer to memory output data will be allocated.
 *                     Should be freed after that.
 *  \param out_size: A pointer to variable where the size of allocated
 *                   data in bytes will be provided.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseProcessChunkInt(SpenchDenoise model,
                                                           SpenchContext ctx,
                                                           const u_int8_t* memory_in,
                                                           u_int64_t in_size,
                                                           u_int8_t** memory_out,
                                                           u_int64_t* out_size);

/**
 *  \brief: Retrieve final chunks with float point data. If model has a future
 *          context, the method enhances and returns remaining data from a
 *          context. In other case method returns empty memory.
 *  \param model: A pointer to model.
 *  \param ctx: A pointer to context.
 *  \param memory_out: A pointer to memory output data will be allocated.
 *              Should be freed after that. If threre are no data available
 *              NULL will be returned.
 *  \param out_size: A pointer to variable where the size of allocated
 *                   data in bytes will be provided. If threre are no data
 *                   available 0 will be returned.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetFinalChunkFp(SpenchDenoise model,
                                                           SpenchContext ctx,
                                                           u_int8_t** memory_out,
                                                           u_int64_t* out_size);

/**
 *  \brief: Retrieve final chunks with pcm16 data. If model has a future
 *          context, the method enhances and returns remaining data from a
 *          context. In other case method returns empty memory.
 *  \param model: A pointer to model.
 *  \param ctx: A pointer to context.
 *  \param memory_out: A the pointer to memory output data will be allocated.
 *              Should be freed after that. If threre are no data available
 *              NULL will be returned.
 *  \param out_size: A pointer to variable where the size of allocated
 *                   data in bytes will be provided. If threre are no data
 *                   available 0 will be returned.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchDenoiseGetFinalChunkInt(SpenchDenoise model,
                                                            SpenchContext ctx,
                                                            u_int8_t** memory_out,
                                                            u_int64_t* out_size);


/**
 *    SpenchAudioProcess
 */

/**
 *  \brief: Allocate and return a model for audio normalizing.
 *  \param config_file: Pointer to string with path to .yaml config file.
 *  \param SpenchAudioProcess: Pointer to pointer to model to be allocated.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessInit(const char* config_file,
                                                     SpenchAudioProcess* model);

/**
 *  \brief: Deallocate a model structure.
 *  \param SpenchAudioProcess: A pointer to pointer to model to be destroyed.
 *  \return: None
 *  \ingroup group_c
 */
SPENCH_EXPORT void  SpenchAudioProcessFree(SpenchAudioProcess model);

/**
 *  \brief: Add Auto Gain Controller object with defauilt parameters
 *          optimized for audio sample rate 48000Hz, or specified in config file
 *          if it was provided for SpenchAudioProcess object.
 *  \param SpenchAudioProcess: Pointer to pointer to model to be allocated.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessAddAgc(SpenchAudioProcess model);

/**
 *  \brief: Add Auto Gain Controller object with specific parameters.
 *          They will applied for an object even config file was provided.
 *  \param SpenchAudioProcess: Pointer to pointer to model to be allocated.
 *  \param audio_sample_rate: An audio sample rate.
 *  \param target_power_level: Desired target power level.
 *  \param max_power_gain: Max power gain.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessAddAgcWithParam(SpenchAudioProcess model,
                                                                u_int32_t audio_sample_rate,
                                                                float target_power_level,
                                                                float max_power_gain);

/**
 *  \brief: Add Range Control object with defauilt parameters
 *          optimized for audio sample rate 48000Hz, or specified in config file
 *          if it was provided for SpenchAudioProcess object.
 *  \param SpenchAudioProcess: Pointer to pointer to model to be allocated.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessAddRc(SpenchAudioProcess model);

/**
 *  \brief: Add Range Controller object with specific parameters.
 *          They will applied for an object even config file was provided.
 *  \param SpenchAudioProcess: A Pointer to pointer to model to be allocated.
 *  \param audio_sample_rate: An audio sample rate (Hz).
 *  \param target_level_db: An target peak value (db).
 *  \param max_gain_db: A max output signal value (db). 0db means that range control do not provide control effect.
 *  \param attack_time: An attack time value for peak value estimation (sec)
 *  \param release_time: A release time value for peak value estimation (sec)
 *  \param hold_time: A hold time value for peak value estimation (sec)
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessAddRcWithParam(SpenchAudioProcess model,
                                                               u_int32_t audio_sample_rate,
                                                               float target_level_db,
                                                               float max_gain_db,
                                                               float attack_time,
                                                               float release_time,
                                                               float hold_time);

/**
 *  \brief: Add Loudness Control object with defauilt parameters
 *          optimized for audio sample rate 48000Hz, or specified in config file
 *          if it was provided for SpenchAudioProcess object.
 *  \param SpenchAudioProcess: Pointer to pointer to model to be allocated.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessAddLc(SpenchAudioProcess model);

/**
 *  \brief: Add Loudness Controller object with specific parameters.
 *          They will applied for an object even config file was provided.
 *  \param SpenchAudioProcess: Pointer to pointer to model to be allocated.
 *  \param audio_sample_rate: An audio sample rate (Hz).
 *  \param target_level_db: An target peak value (db).
 *  \param bypass_time: A value of the time bypassed for Loudness controll configuring (sec)
 *  \param max_gain_db: A max output signal value (db). 0db means that range control do not provide control effect.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessAddLcWithParam(SpenchAudioProcess model,
                                                               u_int32_t audio_sample_rate,
                                                               float target_level_db,
                                                               float bypass_time,
                                                               float max_gain_db);

/**
 *  \brief: Normalize a chunk of audio with float point data.
 *          Process goes through all modules added from first to last one.
 *  \param SpenchAudioProcess: A pointer to a model.
 *  \param memory_in: A pointer to input data.
 *  \param in_size: The size of input data in bytes.
 *  \param memory_out: A pointer to memory output data will be allocated.
 *                     Should be freed after that.
 *  \param out_size: A pointer to variable where the size of allocated
 *                   data in bytes will be provided.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessChunkFp(SpenchAudioProcess model,
                                                        const u_int8_t* memory_in,
                                                        u_int64_t in_size,
                                                        u_int8_t** memory_out,
                                                        u_int64_t* out_size);

/**
 *  \brief: Normalize a chunk of audio with pcm16 data. Implemented through
 *          a call to float point data processing and requires additional
 *          time for convertion.
 *          Process goes through all modules added from first to last one.
 *  \param SpenchAudioProcess: A pointer to model.
 *  \param memory_in: A pointer to input data.
 *  \param in_size: A size of input data in bytes.
 *  \param memory_out: A pointer to memory output data will be allocated.
 *                     Should be freed after that.
 *  \param out_size: A pointer to variable where the size of allocated
 *                   data in bytes will be provided.
 *  \return: Error code. SpenchRetCode::SUCCESS means success.
 *  \ingroup group_c
 */
SPENCH_EXPORT SpenchRetCode   SpenchAudioProcessChunkInt(SpenchAudioProcess model,
                                                         const u_int8_t* memory_in,
                                                         u_int64_t in_size,
                                                         u_int8_t** memory_out,
                                                         u_int64_t* out_size);

#ifdef __cplusplus
}
#endif

#endif  // __SPENCH_C_WRAPPER_HEADER__
