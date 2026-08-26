/**
 * \example 1-cpp_api_demo.cpp
 */
#include <spench.h>
#include "demo/utils.hpp"
#include <getopt.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

namespace {

// getopt_long: short flags in optstring ("hrp:") are returned as their ASCII code ('h', 'r', …).
// Long-only options (--chunk-ms has no -x short form) must use a separate integer val here and in
// the switch below; it must not match any character already used in optstring.
constexpr int32_t getopt_long_only_chunk_ms = 1001;

}  // namespace

void help(int32_t count, char *argv[], Args& args) {
  char* argv_copy[count];
  std::copy(argv, argv + count, argv_copy);
  int32_t help = 0;

  const struct option long_options[] = {
      {"help", no_argument, &help, 1},
      {"resample", no_argument, &args.use_resampling, 1},
      {"packed-model", required_argument, nullptr, 'p'},
      {"chunk-ms", required_argument, nullptr, getopt_long_only_chunk_ms},
      {nullptr, 0, nullptr, 0}
  };

  int32_t option_value;
  int32_t option_index = -1;
  std::string err_message =
                  std::string(
                      "Usage: ./demo_cpp [flags] path_to_input_wav output_wav_save_path [path_to_model_config]\n")
                      +
                  std::string(
                      "Options:\n"
                       "  -h, --help                 show this message\n"
                       "  -r, --resample             use resampling for audio processing\n"
                       "  -p, --packed-model PATH    use packed .spench model (config path not required)\n"
                       "      --chunk-ms MS          input chunk/stride length in ms (default: 16)");
  while ((option_value = getopt_long(count, argv_copy, "hrp:", long_options, &option_index)) != -1) {
      if (option_index < 0) {
          switch (option_value) {
          case 'h':
              help = 1;
              throw std::runtime_error(err_message);
          case 'r':
              args.use_resampling = 1;
              break;
          case 'p':
              if (optarg == nullptr) {
                throw std::runtime_error("Missing argument for --packed-model");
              }
              args.packed_model_path = optarg;
              break;
          case getopt_long_only_chunk_ms:
              if (optarg == nullptr) {
                throw std::runtime_error("Missing argument for --chunk-ms");
              }
              args.chunk_ms = static_cast<uint32_t>(std::stoul(optarg));
              if (args.chunk_ms == 0) {
                throw std::runtime_error("--chunk-ms must be positive");
              }
              break;
          default:
              throw std::runtime_error("Invalid option provided. Use -h for help.");
          }
      }
  }

  const bool use_packed_model = !args.packed_model_path.empty();
  const int32_t required_positionals = use_packed_model ? 2 : 3;
  if (count - optind < required_positionals) {
      throw std::runtime_error(err_message);
  }

  args.source_audio_path = argv[optind];
  args.enhanced_audio_path = argv[optind + 1];
  if (!use_packed_model) {
    args.config_path = argv[optind + 2];
  }

  struct stat conf_sb, packed_sb, input_wav_sb, output_wav_sb;
  if (use_packed_model) {
    if (stat(args.packed_model_path.c_str(), &packed_sb) != 0 || !S_ISREG(packed_sb.st_mode)) {
      std::cerr << "Invalid packed model path has been provided or file does not exist!" << std::endl;
      throw std::runtime_error("File path error.");
    }
  } else if (stat(args.config_path.c_str(), &conf_sb) != 0 || !S_ISREG(conf_sb.st_mode)) {
    std::cerr << "Invalid model config path has been provided or file does't exist!" << std::endl;
    throw std::runtime_error("File path error.");
  }

  if (stat(args.source_audio_path.c_str(), &input_wav_sb) != 0 || !S_ISREG(input_wav_sb.st_mode)) {
    std::cerr << "Invalid input audio file path has been provided or file does't exist!" << std::endl;
    throw std::runtime_error("File path error.");
  } else if (stat(args.enhanced_audio_path.c_str(), &output_wav_sb) == 0 && S_ISDIR(output_wav_sb.st_mode)) {
    std::cerr << "Invalid output audio file path has been provided! It should not be a directory!" << std::endl;
    throw std::runtime_error("File path error.");
  }
}

int32_t main(int32_t argc, char *argv[]) {
  audio::WavInfo info;

  std::vector<float> ret;
  std::vector<float> wav_data;
  std::vector<float> prediction;
  SpenchRetCode ret_code = SpenchRetCode::SUCCESS;
  Args args;

  try {
    parse_arguments(argc, argv, args, help);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  // Read the audio file as program argument into the float vector.
  try {
    // keepUnnormalized = false → normalize into [-1,1]
    audio::load_wav(args.source_audio_path, wav_data, /*keep_unnormalized=*/false, info);
  } catch (const std::exception& e) {
    std::cerr << "Error loading WAV: " << e.what() << "\n";
    return 1;
  }

  uint32_t sample_rate = info.sample_rate;

  std::unique_ptr<spench::SpenchDenoise> denoiser;
  if (!args.packed_model_path.empty()) {
    std::ifstream packed_in(args.packed_model_path, std::ios::binary);
    if (!packed_in) {
      std::cerr << "Cannot open packed model file: " << args.packed_model_path << "\n";
      return 1;
    }
    const std::vector<uint8_t> packed_data{
        std::istreambuf_iterator<char>(packed_in),
        std::istreambuf_iterator<char>()};
    auto bundle = std::make_shared<spench::PackedModelReader>(packed_data);
    denoiser = std::make_unique<spench::SpenchDenoise>(bundle);
  } else {
    denoiser = std::make_unique<spench::SpenchDenoise>(args.config_path.c_str());
  }


  auto input_audio_processor =
          std::unique_ptr<spench::SpenchAudioProcess>
                                  (new spench::SpenchAudioProcess(make_audio_process_config_yaml(sample_rate)));

  if (!args.use_resampling && info.sample_rate != denoiser->GetModelSampleRate()) {
    std::cerr << "The sample rate (" << info.sample_rate
              << ") does not match model rate ("
              << denoiser->GetModelSampleRate() << "). Use -r/--resample.\n";
    return 1;
  }

  // Input/output context rates are the WAV rate; resampling to/from model rate is internal.
  spench::SpenchContext<float>* ctx = denoiser->CreateContext(sample_rate, sample_rate);

  input_audio_processor->AddLC().AddNG().AddLim();

  const uint32_t chunk_ms = args.chunk_ms;
  auto chunks = chunk_wav<float>(wav_data, sample_rate, chunk_ms, chunk_ms);

  for (uint32_t i = 0; i < chunks.size(); ++i) {
    auto& chunk = chunks[i];
    std::vector<float> ret;
    std::vector<float> tmp_ret;
    input_audio_processor->ProcessChunk(chunk, tmp_ret);
    ret_code = denoiser->ProcessChunk(tmp_ret, ret, ctx);
    if (ret_code < SpenchRetCode::SUCCESS) {
      std::cerr << "ProcessChunk failed at chunk " << i << " (code " << static_cast<int32_t>(ret_code)
                << ").\n";
      denoiser->DestroyContext(&ctx);
      return 1;
    }
    if (!ret.empty()) {
      prediction.insert(prediction.end(), ret.begin(), ret.end());
    }
  }

  ret_code = denoiser->GetFinalChunk(ret, ctx);
  if (ret_code < SpenchRetCode::SUCCESS) {
    std::cerr << "GetFinalChunk failed (code " << static_cast<int32_t>(ret_code) << ").\n";
    denoiser->DestroyContext(&ctx);
    return 1;
  }
  if (!ret.empty()) {
    prediction.insert(prediction.end(), ret.begin(), ret.end());
  }

  if (prediction.size() != wav_data.size()) {
    std::cerr << "Output length (" << prediction.size() << ") does not match input ("
              << wav_data.size() << ").\n";
  }

  try {
    audio::write_wav(
        args.enhanced_audio_path,  // output path
        prediction,                 // your float samples
        info);                      // original (or desired) WAV format
  } catch (const std::exception& e) {
    std::cerr << "Error writing WAV: " << e.what() << "\n";
    return 1;
  }

  // Deallocate a context memory.
  denoiser->DestroyContext(&ctx);
  return 0;
}
