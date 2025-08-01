#pragma once
/**
 * \file spench_c_api.h
 * \defgroup group_c C API
 * \brief Plain C wrapper for the Spench audio processing C++ library.
 */

#include <stdlib.h>

#include "spench/errors.h"
#include "spench/export.h"
#include "spench/stream_stat.h"

#if defined(EMSCRIPTEN) || defined(WIN32)
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Opaque handles                                                             */
/* -------------------------------------------------------------------------- */

typedef void* SpenchDenoise;
typedef void* SpenchContext;
typedef void* SpenchAudioProcess;

#if defined(EMSCRIPTEN) || defined(WIN32)
typedef uint64_t u_int64_t;
typedef uint32_t u_int32_t;
typedef uint16_t u_int16_t;
typedef uint8_t u_int8_t;
#endif

/* -------------------------------------------------------------------------- */
/*  SpenchDenoise                                                              */
/* -------------------------------------------------------------------------- */

/**
 * \brief Allocate and return an audio enhancement model.
 * \param config_file Pointer to a string with the path to a .yaml config file.
 * \param model       Pointer to the model handle to be allocated.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseInit(const char* config_file, SpenchDenoise* model);

/**
 * \brief Allocate and return an encrypted audio enhancement model.
 * \param config_file Pointer to a string with the path to a .yaml config file.
 * \param password    Pointer to a string with the password for model decryption. Can be empty if the
 *                    model is not encrypted.
 * \param checksum    Pointer to a string with the license checksum for model decryption. Can be empty
 *                    if the model is not encrypted or licensing is not used.
 * \param model       Pointer to the model handle to be allocated.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchEncryptedDenoiseInit(const char* config_file, const char* password, const char* checksum,
                                                    SpenchDenoise* model);

/**
 * \brief Deallocate a model structure.
 * \param model Pointer to the model handle to be destroyed. The handle will be set to NULL.
 * \ingroup group_c
 */
SPENCH_API void SpenchDenoiseFree(SpenchDenoise* model);

/**
 * \brief Create a context for audio stream denoising.
 * \details If resampling is not required, set the corresponding sample rate to 0.
 * \param model        Model handle.
 * \param ctx          Pointer to the context handle to be created.
 * \param in_audio_sr  Input stream sample rate. Set to 0 to skip resampling.
 * \param out_audio_sr Output stream sample rate. Set to 0 to skip resampling.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseCreateContext(SpenchDenoise model, SpenchContext* ctx, u_int32_t in_audio_sr,
                                                    u_int32_t out_audio_sr);

/**
 * \brief Destroy and free a context.
 * \param model Model handle.
 * \param ctx   Pointer to the context handle to be destroyed.
 * \ingroup group_c
 */
SPENCH_API void SpenchDenoiseDestroyContext(SpenchDenoise model, SpenchContext* ctx);

/**
 * \brief Retrieve the numeric identifier of a context.
 * \param model Model handle.
 * \param ctx   Context handle.
 * \param id    Pointer to the returned identifier.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetContextId(SpenchDenoise model, const SpenchContext ctx, u_int64_t* id);

/**
 * \brief Set the numeric identifier for a context.
 * \param model Model handle.
 * \param ctx   Context handle.
 * \param id    Identifier value to set.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseSetContextId(SpenchDenoise model, SpenchContext ctx, u_int64_t id);

/**
 * \brief Retrieve stream statistics (e.g., SNR) for a context.
 * \param model        Model handle.
 * \param ctx          Context handle.
 * \param stream_stat  Pointer to the structure where statistics will be written.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetStreamStat(SpenchDenoise model, const SpenchContext ctx,
                                                    StreamStatStruct* stream_stat);

/**
 * \brief Retrieve the number of threads used by the inference engine (ONNX, TFLite, etc.).
 * \param model          Model handle.
 * \param engine_threads Pointer to the returned number of threads.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetEngineThreads(const SpenchDenoise model, u_int64_t* engine_threads);

/**
 * \brief Retrieve the model input frame size.
 * \param model      Model handle.
 * \param input_size Pointer to the returned input frame size.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetInputFrameSize(const SpenchDenoise model, u_int64_t* input_size);

/**
 * \brief Retrieve the model sample rate value specified in the .yaml file.
 * \param model       Model handle.
 * \param sample_rate Pointer to the returned sample rate value.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetSampleRate(const SpenchDenoise model, u_int64_t* sample_rate);

/**
 * \brief Enhance a chunk of audio with floating-point data.
 * \param model      Model handle.
 * \param ctx        Context handle.
 * \param memory_in  Pointer to input data (float array as raw bytes).
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseProcessChunkFp(SpenchDenoise model, SpenchContext ctx, const u_int8_t* memory_in,
                                                     u_int64_t in_size, u_int8_t** memory_out, u_int64_t* out_size);

/**
 * \brief Enhance a chunk of audio with PCM16 data.
 * \details Internally converts to floating point, processes, then converts back; therefore it is
 *          slightly slower than SpenchDenoiseProcessChunkFp.
 * \param model      Model handle.
 * \param ctx        Context handle.
 * \param memory_in  Pointer to input data.
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseProcessChunkInt(SpenchDenoise model, SpenchContext ctx, const u_int8_t* memory_in,
                                                      u_int64_t in_size, u_int8_t** memory_out, u_int64_t* out_size);

/**
 * \brief Retrieve final buffered data with floating-point samples.
 * \details If the model uses look-ahead/future context, this function flushes and returns the
 *          remaining data. Otherwise, it returns NULL and size 0.
 * \param model      Model handle.
 * \param ctx        Context handle.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller. Can be NULL if no data is available.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes. Zero if no
 *                    data is available.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetFinalChunkFp(SpenchDenoise model, SpenchContext ctx, u_int8_t** memory_out,
                                                      u_int64_t* out_size);

/**
 * \brief Retrieve final buffered data with PCM16 samples.
 * \details If the model uses look-ahead/future context, this function flushes and returns the
 *          remaining data. Otherwise, it returns NULL and size 0.
 * \param model      Model handle.
 * \param ctx        Context handle.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller. Can be NULL if no data is available.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes. Zero if no
 *                    data is available.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchDenoiseGetFinalChunkInt(SpenchDenoise model, SpenchContext ctx, u_int8_t** memory_out,
                                                       u_int64_t* out_size);

/* -------------------------------------------------------------------------- */
/*  SpenchAudioProcess                                                         */
/* -------------------------------------------------------------------------- */

/**
 * \brief Allocate and return a model for audio normalization/processing.
 * \param config_file Pointer to a string with the path to a .yaml config file.
 * \param model       Pointer to the model handle to be allocated.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessInit(const char* config_file, SpenchAudioProcess* model);

/**
 * \brief Deallocate a SpenchAudioProcess model structure.
 * \param model Model handle to be destroyed.
 * \ingroup group_c
 */
SPENCH_API void SpenchAudioProcessFree(SpenchAudioProcess model);

/**
 * \brief Add an Automatic Gain Control (AGC) module with default parameters (optimized for 48 kHz)
 *        or parameters from the config file if provided.
 * \param model Model handle.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddAgc(SpenchAudioProcess model);

/**
 * \brief Add an Automatic Gain Control (AGC) module with explicit parameters.
 *        These override any config file values.
 * \param model              Model handle.
 * \param audio_sample_rate  Audio sample rate (Hz).
 * \param target_power_level Desired target power level.
 * \param max_power_gain     Maximum power gain.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddAgcWithParam(SpenchAudioProcess model, u_int32_t audio_sample_rate,
                                                           float target_power_level, float max_power_gain);

/**
 * \brief Add a Range Control (RC) module with default parameters (optimized for 48 kHz) or
 *        parameters from the config file if provided.
 * \param model Model handle.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddRc(SpenchAudioProcess model);

/**
 * \brief Add a Range Control (RC) module with explicit parameters. These override any config file
 *        values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param target_level_db   Target peak value (dB).
 * \param max_gain_db       Maximum output signal value (dB). 0 dB means no effect.
 * \param attack_time       Attack time for peak estimation (seconds).
 * \param release_time      Release time for peak estimation (seconds).
 * \param hold_time         Hold time for peak estimation (seconds).
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddRcWithParam(SpenchAudioProcess model, u_int32_t audio_sample_rate,
                                                          float target_level_db, float max_gain_db, float attack_time,
                                                          float release_time, float hold_time);

/**
 * \brief Add a Loudness Control (LC) module with default parameters (optimized for 48 kHz) or
 *        parameters from the config file if provided.
 * \param model Model handle.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddLc(SpenchAudioProcess model);

/**
 * \brief Add a Loudness Control (LC) module with explicit parameters. These override any config file
 *        values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param target_level_db   Target level (dB).
 * \param bypass_time       Time in seconds during which LC stays in bypass for configuration.
 * \param max_gain_db       Maximum output signal value (dB). 0 dB means no effect.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddLcWithParam(SpenchAudioProcess model, u_int32_t audio_sample_rate,
                                                          float target_level_db, float bypass_time, float max_gain_db);

/* --------------------------- Newly Added Functions ------------------------ */

/**
 * \brief Add a Noise Gate (NG) module with default parameters (optimized for 48 kHz) or parameters
 *        from the config file if provided.
 * \param model Model handle.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddNg(SpenchAudioProcess model);

/**
 * \brief Add a Noise Gate (NG) module with explicit parameters. These override any config file
 *        values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param threshold_db      Threshold (dB).
 * \param attack_time       Attack time coefficient (seconds).
 * \param release_time      Release time coefficient (seconds).
 * \param hold_time         Hold time (seconds).
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddNgWithParam(SpenchAudioProcess model, u_int32_t audio_sample_rate,
                                                          float threshold_db, float attack_time, float release_time,
                                                          float hold_time);

/**
 * \brief Add a Limiter (Lim) module with default parameters (optimized for 48 kHz) or parameters
 *        from the config file if provided.
 * \param model Model handle.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddLim(SpenchAudioProcess model);

/**
 * \brief Add a Limiter (Lim) module with explicit parameters. These override any config file values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param threshold_db      Threshold (dB).
 * \param knee_width        Knee width.
 * \param attack_time       Attack time coefficient (seconds).
 * \param release_time      Release time coefficient (seconds).
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddLimWithParam(SpenchAudioProcess model, u_int32_t audio_sample_rate,
                                                           float threshold_db, float knee_width, float attack_time,
                                                           float release_time);

/**
 * \brief Add a Dynamic Range Compressor (DRC) module with default parameters (optimized for 48 kHz)
 *        or parameters from the config file if provided.
 * \param model Model handle.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddDrc(SpenchAudioProcess model);

/**
 * \brief Add a Dynamic Range Compressor (DRC) module with explicit parameters. These override any
 *        config file values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param threshold_db      Threshold (dB).
 * \param ratio             Compression ratio.
 * \param knee_width        Knee width.
 * \param attack_time       Attack time coefficient (seconds).
 * \param release_time      Release time coefficient (seconds).
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessAddDrcWithParam(SpenchAudioProcess model, u_int32_t audio_sample_rate,
                                                           float threshold_db, u_int32_t ratio, float knee_width,
                                                           float attack_time, float release_time);

/**
 * \brief Process a chunk of audio with floating-point data through the chain of added modules.
 * \param model      Model handle.
 * \param memory_in  Pointer to input data (float array as raw bytes).
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessChunkFp(SpenchAudioProcess model, const u_int8_t* memory_in,
                                                   u_int64_t in_size, u_int8_t** memory_out, u_int64_t* out_size);

/**
 * \brief Process a chunk of audio with PCM16 data through the chain of added modules.
 * \details Implemented via floating-point processing and therefore requires additional conversion
 *          time.
 * \param model      Model handle.
 * \param memory_in  Pointer to input data.
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessChunkInt(SpenchAudioProcess model, const u_int8_t* memory_in,
                                                    u_int64_t in_size, u_int8_t** memory_out, u_int64_t* out_size);

/**
 * \brief Retrieve final buffered data with floating-point samples from the processing chain.
 * \details If any module buffers data internally (look-ahead, etc.), this flushes the remaining
 *          samples.
 * \param model      Model handle.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller. Can be NULL if no data is available.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes. Zero if no
 *                    data is available.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessGetFinalChunkFp(SpenchAudioProcess model, u_int8_t** memory_out,
                                                           u_int64_t* out_size);

/**
 * \brief Retrieve final buffered data with PCM16 samples from the processing chain.
 * \details If any module buffers data internally (look-ahead, etc.), this flushes the remaining
 *          samples.
 * \param model      Model handle.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller. Can be NULL if no data is available.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes. Zero if no
 *                    data is available.
 * \return SpenchRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
SPENCH_API SpenchRetCode SpenchAudioProcessGetFinalChunkInt(SpenchAudioProcess model, u_int8_t** memory_out,
                                                            u_int64_t* out_size);

#ifdef __cplusplus
}
#endif
