#include <fmt/format.h>

#include <algorithm>
#include <argparse/argparse.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "demo/utils.hpp"
#include "fmt/core.h"
#include "include/enhance.h"
#include "util/echo/echo_canceler.h"
#include "util/sound/loudness_control.h"
#include "util/sound/voice_activity.h"

namespace {

std::vector<float> load_channel(const std::string& path, audio::WavInfo& info, uint16_t channel) {
  std::vector<float> interleaved;
  audio::load_wav(path, interleaved, false, info);
  if (channel >= info.num_channels) {
    throw std::invalid_argument("Requested channel does not exist in input WAV");
  }

  std::vector<float> result;
  result.reserve(interleaved.size() / info.num_channels);
  for (size_t i = channel; i < interleaved.size(); i += info.num_channels) {
    result.push_back(interleaved[i]);
  }
  return result;
}

std::vector<float> interleave(const std::vector<float>& first, const std::vector<float>& second) {
  if (first.size() != second.size()) {
    throw std::invalid_argument("Audio channels have different lengths");
  }

  std::vector<float> result;
  result.reserve(first.size() * 2);
  for (size_t i = 0; i < first.size(); ++i) {
    result.push_back(first[i]);
    result.push_back(second[i]);
  }
  return result;
}

template <typename Processor>
std::vector<float> process_fixed_frames(const std::vector<float>& input, size_t frame_size, Processor&& process) {
  if (frame_size == 0) {
    throw std::invalid_argument("Audio frame size must be positive");
  }

  std::vector<float> output;
  output.reserve(input.size());
  for (size_t offset = 0; offset < input.size(); offset += frame_size) {
    const size_t actual_size = std::min(frame_size, input.size() - offset);
    std::vector<float> frame(input.begin() + offset, input.begin() + offset + actual_size);
    frame.resize(frame_size, 0.0f);
    std::vector<float> processed = process(frame);
    if (processed.size() < actual_size) {
      throw std::runtime_error("Audio processor returned an incomplete frame");
    }
    output.insert(output.end(), processed.begin(), processed.begin() + actual_size);
  }
  return output;
}

audio::WavInfo output_info(uint32_t sample_rate, uint16_t channels) {
  return audio::WavInfo{channels, sample_rate, 16};
}

}  // namespace

class IParseCommand {
 public:
  virtual ~IParseCommand() {}
  virtual void exec() = 0;
  virtual std::string name() const noexcept = 0;
  virtual argparse::ArgumentParser& parser() noexcept = 0;
};

class LoudnessControlCommand : public IParseCommand {
 public:
  LoudnessControlCommand() : parser_{name()} {
    parser_.add_description("Apply loudness control");
    parser_.add_argument("-i", "--input").help("input audio file").required();
    parser_.add_argument("-o", "--output").help("result audio file").required();
    parser_.add_argument("--target").help("target loudness in LUFS").default_value(-20.0F).scan<'g', float>();
    parser_.add_argument("--max-gain").help("maximum gain limit dB").default_value(40.0F).scan<'g', float>();
  }

  argparse::ArgumentParser& parser() noexcept final { return parser_; }

  std::string name() const noexcept final { return "norm"; }

  void exec() final {
    const auto input_file = parser_.get<std::string>("--input");
    const auto output_file = parser_.get<std::string>("--output");
    const auto target = parser_.get<float>("--target");
    const auto max_gain = parser_.get<float>("--max-gain");

    audio::WavInfo info;
    const std::vector<float> input = load_channel(input_file, info, 0);
    constexpr uint32_t frame_len = 512;

    auto loudctrl = util::sound::LoudnessControlBuilder()
                        .set_method(util::sound::ILoudnessControl::vadbase)
                        .set_sample_rate(info.sample_rate)
                        .set_target_level(target)
                        .set_max_gain(max_gain)
                        .set_post_processing(true)
                        .create();

    const std::vector<float> normalized = process_fixed_frames(
        input, frame_len, [&loudctrl](const std::vector<float>& frame) { return loudctrl->process(frame); });
    audio::write_wav(output_file, interleave(input, normalized), output_info(info.sample_rate, 2));
  }

 private:
  argparse::ArgumentParser parser_;
};

class VoiceActivityCommand : public IParseCommand {
 public:
  VoiceActivityCommand() : parser_{name()} {
    parser_.add_description("Voice activity detector");
    parser_.add_argument("-i", "--input").help("input audio file").required();
    parser_.add_argument("-o", "--output").help("result audio file").required();
    parser_.add_argument("--threshold").help("speech threshold [0, 1]").scan<'g', float>().default_value(0.5f);
    parser_.add_argument("--use-scores").help("use vad scores in result").default_value(false).implicit_value(true);
  }

  argparse::ArgumentParser& parser() noexcept final { return parser_; }

  std::string name() const noexcept final { return "vad"; }

  void exec() final {
    const auto input_file = parser_.get<std::string>("--input");
    const auto output_file = parser_.get<std::string>("--output");
    const auto threshold = parser_.get<float>("--threshold");
    const auto use_scores = parser_.get<bool>("--use-scores");

    audio::WavInfo info;
    const std::vector<float> input = load_channel(input_file, info, 0);
    constexpr uint32_t frame_len = 512;

    auto vad = util::sound::VadBuilder()
                   .set_frame_len(frame_len)
                   .set_sample_rate(info.sample_rate)
                   .set_threshold(threshold)
                   .set_type(util::sound::VadType::rnnoise)
                   .create();

    const std::vector<float> activity = process_fixed_frames(
        input, frame_len, [vad = std::move(vad), use_scores](const std::vector<float>& frame) mutable {
          const auto state = vad->process(frame);
          const float value = use_scores ? state.score : static_cast<float>(state.speech);
          return std::vector<float>(frame.size(), value);
        });
    audio::write_wav(output_file, interleave(input, activity), output_info(info.sample_rate, 2));
  }

 private:
  argparse::ArgumentParser parser_;
};

class EchoCommand : public IParseCommand {
 public:
  EchoCommand() : parser_{name()} {
    parser_.add_description("Apply echo cancelation");
    parser_.add_argument("-i", "--input").help("input audio file").required();
    parser_.add_argument("-o", "--output").help("result audio file").required();
    parser_.add_argument("-c", "--config").help("aec config file").required();
    parser_.add_argument("-m", "--micmap")
        .help("used mic channels map (>=2)")
        .default_value<std::vector<int32_t>>({0, 1})
        .scan<'i', int32_t>()
        .nargs(2, 8);
    parser_.add_argument("--verbose").help("print verbose information").default_value(false).implicit_value(true);
  }

  argparse::ArgumentParser& parser() noexcept final { return parser_; }

  std::string name() const noexcept final { return "echo"; }

  void exec() final {
    const auto input_file = parser_.get<std::string>("--input");
    const auto output_file = parser_.get<std::string>("--output");
    const auto config_file = parser_.get<std::string>("--config");
    const auto micmap = parser_.get<std::vector<int32_t>>("--micmap");
    const bool verbose = parser_.get<bool>("--verbose");

    if (micmap.size() < 2) {
      throw std::runtime_error("use minimum 2 mic channels");
    }
    if (micmap[0] < 0 || micmap[1] < 0) {
      throw std::runtime_error("mic channel indices must be non-negative");
    }

    audio::WavInfo info;
    const std::vector<float> mic = load_channel(input_file, info, static_cast<uint16_t>(micmap[0]));
    const std::vector<float> spk = load_channel(input_file, info, static_cast<uint16_t>(micmap[1]));

    auto echo = std::make_unique<gigaedge::enhance::EchoCanceller>(config_file.c_str());
    const uint16_t frame_len = echo->GetFrameLength();
    const uint32_t fs = echo->GetSampleRate();
    const uint16_t filter_len = echo->GetFilterLength();

    if (info.sample_rate != fs) {
      throw std::runtime_error("Input sample rate must match the echo cancellation configuration");
    }

    if (verbose) {
      fmt::print("Sample rate: {}\n", fs);
      fmt::print("Frame Length: {}\n", frame_len);
      fmt::print("Filter Length: {}\n", filter_len);
    }

    std::vector<float> output;
    output.reserve(mic.size());
    for (size_t offset = 0; offset < mic.size(); offset += frame_len) {
      const size_t actual_size = std::min<size_t>(frame_len, mic.size() - offset);
      std::vector<float> mic_frame(mic.begin() + offset, mic.begin() + offset + actual_size);
      std::vector<float> spk_frame(spk.begin() + offset, spk.begin() + offset + actual_size);
      mic_frame.resize(frame_len, 0.0f);
      spk_frame.resize(frame_len, 0.0f);

      std::vector<float> processed;
      const auto ret_code = echo->ProcessChunk(mic_frame, spk_frame, processed);
      if (ret_code != GigaEdgeEnhanceRetCode::SUCCESS || processed.size() < actual_size) {
        throw std::runtime_error("Echo cancellation failed to process an audio frame");
      }
      output.insert(output.end(), processed.begin(), processed.begin() + actual_size);
    }
    audio::write_wav(output_file, output, output_info(fs, 1));

    if (verbose) {
      const auto audio_msec = static_cast<double>(output.size()) / fs * 1000;
      fmt::print("Processed: {:.2f} sec\n", audio_msec / 1000);
    }
  }

 private:
  argparse::ArgumentParser parser_;
};

class DenoiseCommand : public IParseCommand {
 public:
  DenoiseCommand() : parser_{name()} {
    parser_.add_description("Apply denoiser");
    parser_.add_argument("-i", "--input").help("input audio file").required();
    parser_.add_argument("-o", "--output").help("result audio file").required();
    parser_.add_argument("-c", "--config").help("denoiser config file").required();
    parser_.add_argument("--verbose").help("print verbose information").default_value(false).implicit_value(true);
  }

  argparse::ArgumentParser& parser() noexcept final { return parser_; }

  std::string name() const noexcept final { return "denoise"; }

  void exec() final {
    const auto input_file = parser_.get<std::string>("--input");
    const auto output_file = parser_.get<std::string>("--output");
    const auto config_file = parser_.get<std::string>("--config");
    const bool verbose = parser_.get<bool>("--verbose");

    audio::WavInfo info;
    const std::vector<float> input = load_channel(input_file, info, 0);
    auto denoiser = std::make_unique<gigaedge::enhance::Denoiser>(config_file.c_str());
    const uint32_t model_fs = denoiser->GetModelSampleRate();
    auto* ctx = denoiser->CreateContext(info.sample_rate, model_fs);
    const uint32_t frame_size = denoiser->GetModelInputFrameSize();

    if (verbose && info.sample_rate != model_fs) {
      fmt::print("Use internal sample-rate conversion: in:{} -> model:{}\n", info.sample_rate, model_fs);
    }

    const size_t input_frame_size =
        std::max<size_t>(1, (static_cast<uint64_t>(frame_size) * info.sample_rate + model_fs - 1) / model_fs);
    const size_t expected_output_size =
        (static_cast<uint64_t>(input.size()) * model_fs + info.sample_rate - 1) / info.sample_rate;
    std::vector<float> output;
    output.reserve(expected_output_size);
    auto time = std::chrono::nanoseconds(0);

    for (size_t offset = 0; offset < input.size(); offset += input_frame_size) {
      const size_t actual_size = std::min(input_frame_size, input.size() - offset);
      std::vector<float> frame(input.begin() + offset, input.begin() + offset + actual_size);
      frame.resize(input_frame_size, 0.0f);

      std::vector<float> processed;
      const auto t0 = std::chrono::steady_clock::now();
      const auto ret_code = denoiser->ProcessChunk(frame, processed, ctx);
      const auto t1 = std::chrono::steady_clock::now();
      time += t1 - t0;
      if (ret_code < GigaEdgeEnhanceRetCode::SUCCESS) {
        denoiser->DestroyContext(&ctx);
        throw std::runtime_error("Denoiser failed to process an audio frame");
      }
      output.insert(output.end(), processed.begin(), processed.end());
    }

    std::vector<float> final_output;
    const auto final_ret_code = denoiser->GetFinalChunk(final_output, ctx);
    denoiser->DestroyContext(&ctx);
    if (final_ret_code < GigaEdgeEnhanceRetCode::SUCCESS) {
      throw std::runtime_error("Denoiser failed to flush the processing context");
    }
    output.insert(output.end(), final_output.begin(), final_output.end());
    if (output.size() > expected_output_size) {
      output.resize(expected_output_size);
    }
    audio::write_wav(output_file, output, output_info(model_fs, 1));

    if (verbose) {
      fmt::print("Sample rate: {}\n", model_fs);
      const auto dt_msec = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
      const auto audio_msec = static_cast<double>(output.size()) / model_fs * 1000;
      fmt::print("Processed: {:.2f} sec\n", audio_msec / 1000);
      fmt::print("RTF: {:.2f} %\n", static_cast<float>(dt_msec / audio_msec) * 100);
    }
  }

 private:
  argparse::ArgumentParser parser_;
};

int32_t main(int32_t argc, char const* argv[]) {
  argparse::ArgumentParser program("gigaedge-enhance-tool", GEE_CLI_VERSION);

  EchoCommand echo;
  program.add_subparser(echo.parser());

  DenoiseCommand dnoise;
  program.add_subparser(dnoise.parser());

  LoudnessControlCommand norm;
  program.add_subparser(norm.parser());

  VoiceActivityCommand vad;
  program.add_subparser(vad.parser());

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  if (program.is_subcommand_used(echo.name())) {
    echo.exec();
  }
  if (program.is_subcommand_used(dnoise.name())) {
    dnoise.exec();
  }
  if (program.is_subcommand_used(norm.name())) {
    norm.exec();
  }
  if (program.is_subcommand_used(vad.name())) {
    vad.exec();
  }

  return 0;
}
