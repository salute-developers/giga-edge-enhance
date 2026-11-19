/**
 * \file
 * \defgroup errors Return codes
 */
#pragma once

/**
 *  \ingroup errors
 *  \brief: GigaEdgeEnhance API provides status codes that are usually related to client-side errors.
 *          Here the definition and description of each of them.
 */
typedef enum GigaEdgeEnhanceRetCodeEnum {
  /**
   *  \brief: Everything processed successfully.
   */
  SUCCESS = 0,
  /**
   *  \brief: Everything processed successfully, but the library is not able to provide a portion of data as a context
   *          for the model is being accumulated.
   */
  CONTEXT_ACCUMULATION = 1,
  /**
   *  \brief: Everything processed successfully, but the library is not able to provide a portion of data as a context
   *          for the input resampler is being acumulated. Returned when input resapling is employed.
   */
  RESAMPLING_INPUT_BUFFER_ACCUMULATION = 2,
  /**
   *  \brief: Everything processed successfully, but the library is not able to provide a portion of data as a context
   *          for the output resampler is being accumulated. Returned when output resapling is employed.
   */
  RESAMPLING_OUTPUT_BUFFER_ACCUMULATION = 3,
  /**
   *  \brief: A functionality is not implemented.
   */
  ERR_NOT_IMPLEMENTED = -16,
  /**
   *  \brief: A common problem with Denoiser instance. Read detailes in error message.
   */
  ERR_DENOISER_CREATE_AND_SETUP_PROBLEM,
  /**
   *  \brief: Invalid parameter passed to API. Doublecheck API requirements.
   */
  ERR_INVALID_API_PASSED_ARGUMENT,
  /**
   *  \brief: Invalid chunk size set in a configuration file.
   */
  ERR_INVALID_CHUNK_SIZE,
  /**
   *  \brief: A problem happened during a memory allocation. System sayed that no more memory could be allocated.
   */
  ERR_NOT_ENOUGH_MEMORY,
  /**
   *  \brief: A problem happened during tryinga memory allocation.
   */
  ERR_ALLOCATION_PROBLEM,
  /**
   *  \brief: Incorrect configuration file path.
   */
  ERR_CONF_FILE_INCOR_PATH,
  /**
   *  \brief: Configuration file problem.
   */
  ERR_CONF_FILE_PROBLEM,
  /**
   *  \brief: A model loading problem. Doublecheck the configuration file.
   */
  ERR_MODEL_START_PROBLEM,
  /**
   *  \brief: Invalid frame size set in configuration file.
   */
  ERR_INVALID_FRAME_SIZE,
  /**
   *  \brief: A error happened during output resampling. Probably, an incorrect input sample rate was provided for
   * stream context.
   */
  ERR_INPUT_AUDIO_RESAMPLE_PROBLEM,
  /**
   *  \brief: A error happened during output resampling. Probably, an incorrect output sample rate was provided for
   * stream context.
   */
  ERR_OUTPUT_AUDIO_RESAMPLE_PROBLEM,
  /**
   *  \brief: The library is not able to calculate an input spectrogramm. Probably a error connected with parameters,
   *          specified in configuration file.
   */
  ERR_MEL_SPEC_CALC_PROBLEM,
  /**
   *  \brief: A error happened during audio chunk processing by the model. Probably a problem related to inference
   * engine or drivers configuration.
   */
  ERR_INFERENCE_PROBLEM,
  /**
   *  \brief: The library is not able to calculate output signal. Probably a error connected with parameters,
   *          specified in configuration file.
   */
  ERR_MASK_CALC_PROBLEM
} GigaEdgeEnhanceRetCode;
