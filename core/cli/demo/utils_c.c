#include "demo/utils_c.h"
#define BUFFER_SIZE 4096

void loadWavFx(const char* path, int16_t** wavData, uint32_t* wavDataSize) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Can't open file: %s\n", path);
    exit(EXIT_FAILURE);
  }

  WavHeader header;
  uint32_t bytesRead = fread(&header, 1, 44, file);
  if (bytesRead < 0) {
    fprintf(stderr, "File read error.\n");
  }

  int16_t* buffer = malloc(BUFFER_SIZE * sizeof(int16_t));
  if (buffer == NULL) {
    fprintf(stderr, "Allocation error.\n");
  }

  bytesRead = fread(buffer, sizeof buffer[0], BUFFER_SIZE / sizeof buffer[0], file);
  if (bytesRead < 0) {
    fprintf(stderr, "File read error.\n");
  }
  while (bytesRead > 0) {
    *wavData = realloc(*wavData, (*wavDataSize + bytesRead) * sizeof(int16_t));
    if (*wavData == NULL) {
      fprintf(stderr, "Allocation error.\n");
    }
    for (uint32_t i = 0; i < bytesRead; i++) {
      (*wavData)[*wavDataSize + i] = buffer[i];
    }
    *wavDataSize += bytesRead;
    bytesRead = fread(buffer, sizeof buffer[0], BUFFER_SIZE / sizeof buffer[0], file);
    if (bytesRead < 0) {
      fprintf(stderr, "File read error.\n");
    }
  }
  free(buffer);
  fclose(file);
}

void loadWavFxFloat(const char* path, float** wavData, u_int32_t* wavDataSize, float keepUnnorm) {
  int16_t* pcm16 = NULL;
  uint32_t pcm16Size = 0;
  loadWavFx(path, &pcm16, &pcm16Size);

  *wavData = realloc(*wavData, pcm16Size * sizeof(float));
  if (*wavData == NULL) {
    fprintf(stderr, "Allocation error.\n");
  }
  for (uint32_t i = 0; i < pcm16Size; i++) {
    if (keepUnnorm) {
      (*wavData)[i] = (float)pcm16[i];
    } else {
      float value = (pcm16[i] < 0) ? (float)pcm16[i] / -SHRT_MIN : (float)pcm16[i] / SHRT_MAX;
      (*wavData)[i] = value;
    }
  }
  *wavDataSize = pcm16Size;

  free(pcm16);
}

float** chunkWav(float* wav, u_int32_t wav_size, u_int64_t sample_rate, u_int32_t chunk_size_ms, u_int32_t stride_ms,
                 u_int32_t* num_chunks) {
  uint32_t step_size_samples = ceil((sample_rate * chunk_size_ms) / 1000.0f);
  uint32_t hop_window_samples = ceil((sample_rate * stride_ms) / 1000.0f);

  if (step_size_samples > wav_size) {
    *num_chunks = 1;
    float** result = malloc(sizeof(float*));
    if (*result == NULL) {
      fprintf(stderr, "Allocation error.\n");
    }
    result[0] = malloc(sizeof(float) * wav_size);
    if (result[0] == NULL) {
      fprintf(stderr, "Allocation error.\n");
    }
    memcpy(result[0], wav, sizeof(float) * wav_size);
    return result;
  }

  *num_chunks = (wav_size + step_size_samples - 1) / hop_window_samples;
  float** result = malloc(sizeof(float*) * (*num_chunks));
  if (*result == NULL) {
    fprintf(stderr, "Allocation error.\n");
  }

  for (uint32_t step = 0; step < *num_chunks; ++step) {
    result[step] = malloc(sizeof(float) * step_size_samples);
    if (result[step] == NULL) {
      fprintf(stderr, "Allocation error.\n");
    }
    uint32_t start_index = step * hop_window_samples;
    uint32_t end_index = start_index + step_size_samples;
    end_index = end_index > wav_size ? wav_size : end_index;

    memcpy(result[step], wav + start_index, sizeof(float) * (end_index - start_index));

    // padding
    if ((end_index - start_index) < step_size_samples) {
      for (uint32_t i = end_index - start_index; i < step_size_samples; ++i) {
        result[step][i] = 0;
      }
    }
  }

  return result;
}

void writeToFile(FILE* file, uint32_t value, uint32_t size) { fwrite(&value, size, 1, file); }

void writeWav(float* data, size_t dataSize, uint32_t sample_rate, uint16_t bitsPerSample, const char* output_file) {
  // Open the stream for binary
  FILE* audioFile = fopen(output_file, "wb");
  if (!audioFile) {
    perror("File cannot be opened or created.\n");
    exit(EXIT_FAILURE);
  }

  // Header chunk
  fprintf(audioFile, "RIFF");
  fprintf(audioFile, "----");
  fprintf(audioFile, "WAVE");

  // Format chunk
  fprintf(audioFile, "fmt ");
  writeToFile(audioFile, 16, 4);                               // Size
  writeToFile(audioFile, 1, 2);                                // Compression code
  writeToFile(audioFile, 1, 2);                                // Number of channels
  writeToFile(audioFile, sample_rate, 4);                      // Sample rate
  writeToFile(audioFile, sample_rate * bitsPerSample / 8, 4);  // Byte rate
  writeToFile(audioFile, bitsPerSample / 8, 2);                // Block align
  writeToFile(audioFile, bitsPerSample, 2);                    // Bit depth

  // Data chunk
  fprintf(audioFile, "data");
  fprintf(audioFile, "----");

  uint32_t preAudioPosition = ftell(audioFile);

  uint32_t maxAmplitude = pow(2, bitsPerSample - 1) - 1;
  for (uint32_t i = 0; i < dataSize; i++) {
    uint32_t intSample = (uint32_t)(data[i] * maxAmplitude);
    writeToFile(audioFile, intSample, 2);
  }
  uint32_t postAudioPosition = ftell(audioFile);

  fseek(audioFile, preAudioPosition - 4, SEEK_SET);
  writeToFile(audioFile, postAudioPosition - preAudioPosition, 4);

  fseek(audioFile, 4, SEEK_SET);
  writeToFile(audioFile, postAudioPosition - 8, 4);

  fclose(audioFile);
}
