/**
 * \example 3-cpp_dsp_demo.cpp
 */
#include <getopt.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "demo/utils.hpp"
#include "include/giga-edge-enhance.h"

static void help(int32_t count, char* argv[], Args& args) {
  // getopt_long expects a null-terminated argv array and may reorder it.
  std::vector<char*> argv_copy(static_cast<std::size_t>(count) + 1, nullptr);
  std::copy(argv, argv + count, argv_copy.begin());

  const struct option long_options[] = {
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0},
  };

  const std::string usage =
      "Usage: ./demo_dsp [flags] path_to_input_wav output_wav_save_path [path_to_model_config]\n"
      "Options:\n"
      "  -h, --help    to show this message\n";

  int32_t option_index = -1;
  int32_t option_value = 0;

  while ((option_value = getopt_long(count, argv_copy.data(), "h", long_options, &option_index)) != -1) {
    if (option_index < 0) {
      switch (option_value) {
        case 'h': {
          throw std::runtime_error(usage);
        }
        default: {
          throw std::runtime_error("Invalid option provided. Use -h for help.");
        }
      }
    }
  }

  if (count - optind < 2) {
    throw std::runtime_error(
        "Incorrect number of arguments. Usage: ./demo_dsp [flags] path_to_input_wav "
        "output_wav_save_path [path_to_model_config]");
  }

  args.source_audio_path = argv[optind];
  args.enhanced_audio_path = argv[optind + 1];
  if (count - optind >= 3) {
    args.config_path = argv[optind + 2];
  } else {
    args.config_path.clear();
  }

  struct stat input_wav_sb{};
  struct stat output_wav_sb{};
  if (stat(args.source_audio_path.c_str(), &input_wav_sb) != 0 || !S_ISREG(input_wav_sb.st_mode)) {
    std::cerr << "Invalid input audio file path has been provided or file does not exist: " << args.source_audio_path
              << std::endl;
    throw std::runtime_error("File path error.");
  }
  if (stat(args.enhanced_audio_path.c_str(), &output_wav_sb) == 0 && S_ISDIR(output_wav_sb.st_mode)) {
    std::cerr << "Invalid output audio file path has been provided! It should not be a directory: "
              << args.enhanced_audio_path << std::endl;
    throw std::runtime_error("File path error.");
  }
  if (!args.config_path.empty()) {
    struct stat conf_sb{};
    if (stat(args.config_path.c_str(), &conf_sb) != 0 || !S_ISREG(conf_sb.st_mode)) {
      std::cerr << "Invalid model config path has been provided or file does not exist: " << args.config_path
                << std::endl;
      throw std::runtime_error("File path error.");
    }
  }
}

int32_t main(int32_t argc, char* argv[]) {
  audio::WavInfo info{};
  std::vector<float> wav_data;
  std::vector<float> prediction;

  const uint32_t chunk_stride_ms = 16;
  const uint32_t chunk_size_ms = 16;

  Args args{};

  try {
    parse_arguments(argc, argv, args, help);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  std::unique_ptr<giga_edge::enhance::AudioProcessor> audio_processor =
      args.config_path.empty()
          ? std::unique_ptr<giga_edge::enhance::AudioProcessor>(new giga_edge::enhance::AudioProcessor())
          : std::unique_ptr<giga_edge::enhance::AudioProcessor>(
                new giga_edge::enhance::AudioProcessor(args.config_path.c_str()));

  audio_processor->AddRC().AddNG().AddDRC();

  try {
    audio::load_wav(args.source_audio_path, wav_data, /*keep_unnormalized=*/false, info);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  const uint32_t sample_rate = info.sample_rate;
  auto chunks = chunk_wav<float>(wav_data, sample_rate, chunk_size_ms, chunk_stride_ms);

  for (std::size_t i = 0; i < chunks.size(); ++i) {
    std::vector<float> chunk_out;
    std::vector<float>& chunk = chunks[i];
    audio_processor->ProcessChunk(chunk, chunk_out);
    prediction.insert(prediction.end(), chunk_out.begin(), chunk_out.end());
  }

  if (prediction.size() != wav_data.size()) {
    std::cerr << "The dimensions of the vectors do not match (in=" << wav_data.size() << ", out=" << prediction.size()
              << ")." << std::endl;
    return 1;
  }

  try {
    audio::write_wav(args.enhanced_audio_path, prediction, info);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
