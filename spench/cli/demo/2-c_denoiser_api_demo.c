/**
 * \example 2-c_api_demo.c
 */
#include <spench/spench_c.h>
#include "demo/utils_c.h"

int main(int argc, char *argv[]) {
    WavHeader header;
    memset(&header, 0, sizeof(header));
    struct stat buffer;
    float* wav_data = NULL;
    float* prediction = NULL;
    u_int32_t num_chunks;
    u_int32_t step_size_samples;
    u_int64_t sample_rate = 24000;
    u_int64_t input_size = 0;
    const u_int32_t chunk_stride_ms = 16;
    u_int32_t chunk_size_ms;
    u_int32_t wav_size;
    uint64_t prediction_size = 0;
    SpenchRetCode ret_code = SUCCESS;
    SpenchDenoise denoiser = NULL;
    SpenchContext ctx = NULL;


    if (argc != 4) {
      fprintf(stderr, "Model config path, an input and output .wav files should be provided.\n");
      ret_code = ERR_INFERENCE_PROBLEM;
    } else if (stat(argv[1], &buffer) != 0 || stat(argv[2], &buffer) != 0) {
      fprintf(stderr, "Invalid path of .wav files has been provided!\n");
      ret_code = ERR_INFERENCE_PROBLEM;
    }

    // Read the audio file as program argument into the float vector.
    FILE* input_file = fopen(argv[2], "rb");
    if (input_file == NULL) {
      fprintf(stderr, "File cannot be opened or created.\n");
      ret_code = ERR_INFERENCE_PROBLEM;
    }
    uint32_t bytesRead = fread(&header, 1, 44, input_file);
    if (bytesRead <= 0) {
      fprintf(stderr, "Failed to read file.\n");
      ret_code = ERR_INFERENCE_PROBLEM;
    }
    fclose(input_file);

    if (ret_code == SUCCESS) {
      // Create an instance on denoiser class with passing configuration file as a parameter.
      ret_code = SpenchDenoiseInit(argv[1], &denoiser);
    }

    if (ret_code == SUCCESS) {
      // Retrieve a model sample rate value, specified in .yaml file.
      ret_code = SpenchDenoiseGetSampleRate(denoiser, &sample_rate);
    }

    wav_size = header.subchunk2Size;
    if (ret_code == SUCCESS) {
      if (header.sampleRate != sample_rate) {
        fprintf(stderr, "The sample rate does not match.\n");
        ret_code = ERR_INFERENCE_PROBLEM;
      }
      loadWavFxFloat(argv[2], &wav_data, &wav_size, 0.0);
      if (wav_data == NULL) {
        fprintf(stderr, "Could not read file %s\n", argv[2]);
        ret_code = ERR_INFERENCE_PROBLEM;
      }
    }

    if (ret_code == SUCCESS) {
      // Create the context for audio stream denoising without the resampling routine.
      ret_code = SpenchDenoiseCreateContext(denoiser, &ctx, 0, 0);
    }

    if (ret_code == SUCCESS) {
      ret_code = SpenchDenoiseGetInputFrameSize(denoiser, &input_size);
    }

    // Create a vector of chunks of audio data with size chunk_size_ms ms.
    chunk_size_ms = ((float)input_size / sample_rate) * 1000.0f;
    float** chunks = chunkWav(wav_data, wav_size, sample_rate, chunk_size_ms, chunk_stride_ms, &num_chunks);

    step_size_samples = ceil((sample_rate * chunk_size_ms) / 1000.0f);

    if (ret_code == SUCCESS) {
      // Step by step processing of the frames.
      for (uint32_t i = 0; i < num_chunks; ++i) {
          float* chunk = chunks[i];
          unsigned char* ret_mem = NULL;
          uint64_t ret_size = 0;
          ret_code = SpenchDenoiseProcessChunkFp(denoiser,
                                        ctx,
                                        (const unsigned char*) chunk,
                                        step_size_samples * sizeof(float),
                                        &ret_mem,
                                        &ret_size);
          if (ret_code < SUCCESS) {
            break;
          }
          else {
            // The fast model 24 gives nulls in the first chunk so the first chunk should be skipped.
            if (i != 0) {
              uint64_t ret_vec_size = ret_size / sizeof(float);
              float* ret_vec = (float*)ret_mem;
              prediction = realloc(prediction, (prediction_size + ret_vec_size) * sizeof(float));
              if (prediction == NULL) {
                ret_code = ERR_ALLOCATION_PROBLEM;
              }
              memcpy(prediction + prediction_size, ret_vec, ret_vec_size * sizeof(float));
              prediction_size += ret_vec_size;
              if (ret_mem != NULL) {
                free(ret_mem);
                ret_mem = NULL;
              }
            }
          }
      }
    }

    // Retrieve final chunks with float point data.
    unsigned char* ret = NULL;
    uint64_t ret_size = 0;
    if (ret_code == SUCCESS) {
      ret_code = SpenchDenoiseGetFinalChunkFp(denoiser,
                                            ctx,
                                            &ret,
                                            &ret_size);
      if (ret_code != SUCCESS || ret == NULL) {
        fprintf(stderr, "GetFinalChunk error.\n");
      }
    }
    uint64_t ret_vec_size = ret_size / sizeof(float);
    float* ret_vec = (float*)ret;
    if (ret_code == SUCCESS) {
      prediction = realloc(prediction, (prediction_size + ret_vec_size) * sizeof(float));
    }
    if (prediction == NULL) {
      ret_code = ERR_ALLOCATION_PROBLEM;
    }
    if (ret_code == SUCCESS) {
      memcpy(prediction + prediction_size, ret_vec, ret_vec_size * sizeof(float));
      prediction_size += ret_vec_size;
    }
    if (ret != NULL) {
      free(ret);
      ret = NULL;
    }

    // The length of input and enhanced file must be equal.
    if (num_chunks * step_size_samples != prediction_size) {
      fprintf(stderr, "The dimensions of the vectors does not match.\n");
      ret_code = ERR_INFERENCE_PROBLEM;
    }

    if (ret_code == SUCCESS) {
      // Recording the predicted vector into a wav file.
      writeWav(prediction, wav_size, sample_rate, header.bitsPerSample, argv[3]);
    }

    // Deallocate a context memory.
    SpenchDenoiseDestroyContext(denoiser, &ctx);

    free(wav_data);
    free(prediction);
    for (uint32_t i = 0; i < num_chunks; ++i) {
      free(chunks[i]);
    }
    free(chunks);
    return ret_code;
}
