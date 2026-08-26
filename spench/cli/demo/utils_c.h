#ifndef UTILS_C_H
#define UTILS_C_H

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct WAV_HEADER {
  uint8_t RIFF[4];
  uint32_t chunkSize;  // RIFF Chunk Size
  uint8_t WAVE[4];
  uint8_t fmt[4];
  uint32_t subchunk1Size;  // Size of the fmt chunk
  uint16_t audioFormat;  // must be 1 (PCM)
  uint16_t numOfChan;
  uint32_t sampleRate;
  uint32_t bytesPerSec;  // bytes per second
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  /* "data" sub-chunk */
  uint8_t subchunk2ID[4];  // "data"
  uint32_t subchunk2Size;  // Sampled data length
} WavHeader;

void loadWavFx(const char* path, int16_t** wavData, uint32_t* wavDataSize);
void loadWavFxFloat(const char* path, float** wavData, u_int32_t* wavDataSize, float keepUnnorm);
float** chunkWav(float* wav,
                 u_int32_t wav_size,
                 u_int64_t sample_rate,
                 u_int32_t chunk_size_ms,
                 u_int32_t stride_ms,
                 u_int32_t* num_chunks);
void writeToFile(FILE* file, uint32_t value, uint32_t size);
void writeWav(float* data, size_t dataSize,
              uint32_t sample_rate,
              uint16_t bitsPerSample,
              const char* output_file);

#endif
