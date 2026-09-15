#pragma once
/**
 * \file gigadenoiser_c_api.h
 * \defgroup group_c C API
 * \brief Plain C wrapper for the GigaDenoiser audio processing C++ library.
 */

#include <stdint.h>
#include <stdlib.h>

#include "gigadenoiser/errors.h"
#include "gigadenoiser/export.h"
#include "gigadenoiser/stream_stat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Opaque handles                                                             */
/* -------------------------------------------------------------------------- */

typedef void* GigaDenoiserDenoise;
typedef void* GigaDenoiserContext;
typedef void* GigaDenoiserAudioProcess;

/* -------------------------------------------------------------------------- */
/*  GigaDenoiserDenoise                                                              */
/* -------------------------------------------------------------------------- */

/**
 * \brief Allocate and return an audio enhancement model.
 * \param config_file Pointer to a string with the path to a .yaml config file.
 * \param model       Pointer to the model handle to be allocated.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseInit(const char* config_file, GigaDenoiserDenoise* model);

/**
 * \brief Deallocate a model structure.
 * \param model Pointer to the model handle to be destroyed. The handle will be set to NULL.
 * \ingroup group_c
 */
GDN_API void GigaDenoiserDenoiseFree(GigaDenoiserDenoise* model);

/**
 * \brief Create a context for audio stream denoising.
 * \details If resampling is not required, set the corresponding sample rate to 0.
 * \param model        Model handle.
 * \param ctx          Pointer to the context handle to be created.
 * \param in_audio_sr  Input stream sample rate. Set to 0 to skip resampling.
 * \param out_audio_sr Output stream sample rate. Set to 0 to skip resampling.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseCreateContext(GigaDenoiserDenoise model, GigaDenoiserContext* ctx,
                                                             uint32_t in_audio_sr, uint32_t out_audio_sr);

/**
 * \brief Destroy and free a context.
 * \param model Model handle.
 * \param ctx   Pointer to the context handle to be destroyed.
 * \ingroup group_c
 */
GDN_API void GigaDenoiserDenoiseDestroyContext(GigaDenoiserDenoise model, GigaDenoiserContext* ctx);

/**
 * \brief Retrieve the numeric identifier of a context.
 * \param model Model handle.
 * \param ctx   Context handle.
 * \param id    Pointer to the returned identifier.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetContextId(GigaDenoiserDenoise model, const GigaDenoiserContext ctx,
                                                            uint64_t* id);

/**
 * \brief Set the numeric identifier for a context.
 * \param model Model handle.
 * \param ctx   Context handle.
 * \param id    Identifier value to set.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseSetContextId(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                            uint64_t id);

/**
 * \brief Retrieve stream statistics (e.g., SNR) for a context.
 * \param model        Model handle.
 * \param ctx          Context handle.
 * \param stream_stat  Pointer to the structure where statistics will be written.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetStreamStat(GigaDenoiserDenoise model, const GigaDenoiserContext ctx,
                                                             StreamStatStruct* stream_stat);

/**
 * \brief Retrieve the number of threads used by the inference engine (ONNX, TFLite, etc.).
 * \param model          Model handle.
 * \param engine_threads Pointer to the returned number of threads.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetEngineThreads(const GigaDenoiserDenoise model,
                                                                uint64_t* engine_threads);

/**
 * \brief Retrieve the model input frame size.
 * \param model      Model handle.
 * \param input_size Pointer to the returned input frame size.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetInputFrameSize(const GigaDenoiserDenoise model, uint64_t* input_size);

/**
 * \brief Retrieve the model sample rate value specified in the .yaml file.
 * \param model       Model handle.
 * \param sample_rate Pointer to the returned sample rate value.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetSampleRate(const GigaDenoiserDenoise model, uint64_t* sample_rate);

/**
 * \brief Enhance a chunk of audio with floating-point data.
 * \param model      Model handle.
 * \param ctx        Context handle.
 * \param memory_in  Pointer to input data (float array as raw bytes).
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseProcessChunkFp(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                              const uint8_t* memory_in, uint64_t in_size,
                                                              uint8_t** memory_out, uint64_t* out_size);

/**
 * \brief Enhance a chunk of audio with PCM16 data.
 * \details Internally converts to floating point, processes, then converts back; therefore it is
 *          slightly slower than GigaDenoiserDenoiseProcessChunkFp.
 * \param model      Model handle.
 * \param ctx        Context handle.
 * \param memory_in  Pointer to input data.
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseProcessChunkInt(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                               const uint8_t* memory_in, uint64_t in_size,
                                                               uint8_t** memory_out, uint64_t* out_size);

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
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetFinalChunkFp(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                               uint8_t** memory_out, uint64_t* out_size);

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
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserDenoiseGetFinalChunkInt(GigaDenoiserDenoise model, GigaDenoiserContext ctx,
                                                                uint8_t** memory_out, uint64_t* out_size);

/* -------------------------------------------------------------------------- */
/*  GigaDenoiserAudioProcess                                                         */
/* -------------------------------------------------------------------------- */

/**
 * \brief Allocate and return a model for audio normalization/processing.
 * \param config_file Pointer to a string with the path to a .yaml config file.
 * \param model       Pointer to the model handle to be allocated.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessInit(const char* config_file, GigaDenoiserAudioProcess* model);

/**
 * \brief Deallocate a GigaDenoiserAudioProcess model structure.
 * \param model Model handle to be destroyed.
 * \ingroup group_c
 */
GDN_API void GigaDenoiserAudioProcessFree(GigaDenoiserAudioProcess model);

/**
 * \brief Add an Automatic Gain Control (AGC) module with default parameters (optimized for 48 kHz)
 *        or parameters from the config file if provided.
 * \param model Model handle.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddAgc(GigaDenoiserAudioProcess model);

/**
 * \brief Add an Automatic Gain Control (AGC) module with explicit parameters.
 *        These override any config file values.
 * \param model              Model handle.
 * \param audio_sample_rate  Audio sample rate (Hz).
 * \param target_power_level Desired target power level.
 * \param max_power_gain     Maximum power gain.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddAgcWithParam(GigaDenoiserAudioProcess model,
                                                                    uint32_t audio_sample_rate,
                                                                    float target_power_level, float max_power_gain);

/**
 * \brief Add a Range Control (RC) module with default parameters (optimized for 48 kHz) or
 *        parameters from the config file if provided.
 * \param model Model handle.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddRc(GigaDenoiserAudioProcess model);

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
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddRcWithParam(GigaDenoiserAudioProcess model,
                                                                   uint32_t audio_sample_rate, float target_level_db,
                                                                   float max_gain_db, float attack_time,
                                                                   float release_time, float hold_time);

/**
 * \brief Add a Loudness Control (LC) module with default parameters (optimized for 48 kHz) or
 *        parameters from the config file if provided.
 * \param model Model handle.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddLc(GigaDenoiserAudioProcess model);

/**
 * \brief Add a Loudness Control (LC) module with explicit parameters. These override any config file
 *        values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param target_level_db   Target level (dB).
 * \param bypass_time       Time in seconds during which LC stays in bypass for configuration.
 * \param max_gain_db       Maximum output signal value (dB). 0 dB means no effect.
 * \param use_vad           If LC should use VAD for voice detection this option shpuld be enabled (1).
 *                          Provide more accurate work, but has a small impact on CPU consumption.
 *                          By default VAD is deactivated (0)
 * \param post_processing   When non-zero, enable compressor/limiter after normalization (1 = on). When zero,
 *                          disable (0 = off). Default product behavior is off; pass 0 only if you need
 *                          gain-only loudness control.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddLcWithParam(GigaDenoiserAudioProcess model,
                                                                   uint32_t audio_sample_rate, float target_level_db,
                                                                   float bypass_time, float max_gain_db,
                                                                   uint32_t use_vad, uint32_t post_processing);

/* --------------------------- Newly Added Functions ------------------------ */

/**
 * \brief Add a Noise Gate (NG) module with default parameters (optimized for 48 kHz) or parameters
 *        from the config file if provided.
 * \param model Model handle.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddNg(GigaDenoiserAudioProcess model);

/**
 * \brief Add a Noise Gate (NG) module with explicit parameters. These override any config file
 *        values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param threshold_db      Threshold (dB).
 * \param attack_time       Attack time coefficient (seconds).
 * \param release_time      Release time coefficient (seconds).
 * \param hold_time         Hold time (seconds).
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddNgWithParam(GigaDenoiserAudioProcess model,
                                                                   uint32_t audio_sample_rate, float threshold_db,
                                                                   float attack_time, float release_time,
                                                                   float hold_time);

/**
 * \brief Add a Limiter (Lim) module with default parameters (optimized for 48 kHz) or parameters
 *        from the config file if provided.
 * \param model Model handle.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddLim(GigaDenoiserAudioProcess model);

/**
 * \brief Add a Limiter (Lim) module with explicit parameters. These override any config file values.
 * \param model             Model handle.
 * \param audio_sample_rate Audio sample rate (Hz).
 * \param threshold_db      Threshold (dB).
 * \param knee_width        Knee width.
 * \param attack_time       Attack time coefficient (seconds).
 * \param release_time      Release time coefficient (seconds).
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddLimWithParam(GigaDenoiserAudioProcess model,
                                                                    uint32_t audio_sample_rate, float threshold_db,
                                                                    float knee_width, float attack_time,
                                                                    float release_time);

/**
 * \brief Add a Dynamic Range Compressor (DRC) module with default parameters (optimized for 48 kHz)
 *        or parameters from the config file if provided.
 * \param model Model handle.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddDrc(GigaDenoiserAudioProcess model);

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
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessAddDrcWithParam(GigaDenoiserAudioProcess model,
                                                                    uint32_t audio_sample_rate, float threshold_db,
                                                                    uint32_t ratio, float knee_width, float attack_time,
                                                                    float release_time);

/**
 * \brief Process a chunk of audio with floating-point data through the chain of added modules.
 * \param model      Model handle.
 * \param memory_in  Pointer to input data (float array as raw bytes).
 * \param in_size    Size of the input data in bytes.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessChunkFp(GigaDenoiserAudioProcess model, const uint8_t* memory_in,
                                                            uint64_t in_size, uint8_t** memory_out, uint64_t* out_size);

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
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessChunkInt(GigaDenoiserAudioProcess model, const uint8_t* memory_in,
                                                             uint64_t in_size, uint8_t** memory_out,
                                                             uint64_t* out_size);

/**
 * \brief Retrieve final buffered data with floating-point samples from the processing chain.
 * \details If any module buffers data internally (look-ahead, etc.), this flushes the remaining
 *          samples.
 * \param model      Model handle.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller. Can be NULL if no data is available.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes. Zero if no
 *                    data is available.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessGetFinalChunkFp(GigaDenoiserAudioProcess model,
                                                                    uint8_t** memory_out, uint64_t* out_size);

/**
 * \brief Retrieve final buffered data with PCM16 samples from the processing chain.
 * \details If any module buffers data internally (look-ahead, etc.), this flushes the remaining
 *          samples.
 * \param model      Model handle.
 * \param memory_out Pointer to the buffer that will be allocated for output data. Must be freed by
 *                    the caller. Can be NULL if no data is available.
 * \param out_size   Pointer to the returned size of the allocated output buffer in bytes. Zero if no
 *                    data is available.
 * \return GigaDenoiserRetCode::SUCCESS on success, otherwise an error code.
 * \ingroup group_c
 */
GDN_API GigaDenoiserRetCode GigaDenoiserAudioProcessGetFinalChunkInt(GigaDenoiserAudioProcess model,
                                                                     uint8_t** memory_out, uint64_t* out_size);

#ifdef __cplusplus
}
#endif
