#include "src/handler/neural_network_audio_handler.h"

#include <spdlog/spdlog.h>
#include <src/config_reader.h>

#include <algorithm>
#include <exception>

#include "src/logger.h"

namespace gigaedge::denoiser {

NeuralNetworkAudioHandler::NeuralNetworkAudioHandler(std::shared_ptr<NetworkConfig> config)
    : BaseAudioHandler(std::move(config)), feature_extractor_runner_ptr_(nullptr), inference_runner_ptr_(nullptr) {
  // Ensure do_start() runs after derived members are initialized.
  GigaDenoiserRetCode ret_code = restart();
  if (ret_code != GigaDenoiserRetCode::SUCCESS) {
    spdlog::error("A neural network handler could not be created! {}", std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("A neural network handler module creation failed. ret_code=" +
                             std::to_string(static_cast<int32_t>(ret_code)));
  }
  skip_zero_chunk_ = dynamic_cast<gigaedge::denoiser::NetworkConfig*>(config_.get())->skip_zeroed_chunks();
}

NeuralNetworkAudioHandler::NeuralNetworkAudioHandler(NeuralNetworkAudioHandler&& orig) noexcept
    : BaseAudioHandler(std::move(orig)),
      feature_extractor_runner_ptr_(std::move(orig.feature_extractor_runner_ptr_)),
      inference_runner_ptr_(std::move(orig.inference_runner_ptr_)) {}

NeuralNetworkAudioHandler::~NeuralNetworkAudioHandler() = default;

uint32_t NeuralNetworkAudioHandler::get_engine_threads_num() {
  return std::dynamic_pointer_cast<gigaedge::denoiser::NetworkConfig>(config_)->num_threads();
}

uint64_t NeuralNetworkAudioHandler::get_model_sample_rate() {
  return feature_extractor_runner_ptr_->get_sampling_freq();
}

uint64_t NeuralNetworkAudioHandler::get_input_frame_size() {
  return feature_extractor_runner_ptr_->get_frame_hop_size_samples();
}

GigaDenoiserRetCode NeuralNetworkAudioHandler::do_start() {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;

  ret_code = launch_feature_extractor_runner();
  if (ret_code != GigaDenoiserRetCode::SUCCESS) {
    return ret_code;
  }
  ret_code = launch_inference_runner();
  return ret_code;
}

void NeuralNetworkAudioHandler::do_stop() {
  feature_extractor_runner_ptr_.reset();
  inference_runner_ptr_.reset();
}

GigaDenoiserRetCode NeuralNetworkAudioHandler::launch_feature_extractor_runner() {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  try {
    if (!feature_extractor_runner_ptr_) {
      feature_extractor_runner_ptr_ =
          std::unique_ptr<gigaedge::denoiser::FeatureExtractorRunner>(new gigaedge::denoiser::FeatureExtractorRunner(
              std::dynamic_pointer_cast<gigaedge::denoiser::NetworkConfig>(config_)));
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    ret_code = GigaDenoiserRetCode::ERR_MODEL_START_PROBLEM;
  }
  return ret_code;
}

GigaDenoiserRetCode NeuralNetworkAudioHandler::launch_inference_runner() {
  GigaDenoiserRetCode ret_code = GigaDenoiserRetCode::SUCCESS;
  const uint32_t inference_engine_threads = get_engine_threads_num();

  try {
    if (!inference_runner_ptr_) {
      inference_runner_ptr_ =
          std::unique_ptr<gigaedge::denoiser::InferenceRunner>(new gigaedge::denoiser::InferenceRunner(
              std::dynamic_pointer_cast<gigaedge::denoiser::NetworkConfig>(config_), inference_engine_threads, false));
    }
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    ret_code = GigaDenoiserRetCode::ERR_MODEL_START_PROBLEM;
  }
  return ret_code;
}

bool NeuralNetworkAudioHandler::is_zeroed_vector(const std::vector<float>& v, float eps) noexcept {
  const size_t n = v.size();
  for (size_t i = 0; i < n; ++i) {
    float ax = std::fabs(v[i]);
    if (ax > eps || !std::isfinite(ax)) {
      return false;
    }
  }
  return true;
}

namespace tools {

void prepare_first_input_frame(const std::vector<float>& current_chunk, std::vector<float>& frame,
                               uint32_t frame_size) {
  if (frame.size() != 0) {
    spdlog::error("Zero length frame should be provided as output argument");
    return;
  }
  frame.reserve(1 + frame_size + (current_chunk.size() - 1));
  frame.emplace_back(0.0f);

  const auto first_begin = current_chunk.begin();
  const auto first_end = first_begin + static_cast<std::ptrdiff_t>(frame_size);

  std::reverse_copy(first_begin, first_end, std::back_inserter(frame));
  // keep the original "skip the very first sample" behavior to avoid boundary duplication
  frame.insert(frame.end(), current_chunk.begin() + 1, current_chunk.end());
}

}  // namespace tools

}  // namespace gigaedge::denoiser
