#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/context.h"
#include "src/task.h"
#include "util/onnx/base_onnx_runner.h"
#include "util/onnx/onnx_env.h"

namespace util {
namespace onnx {
class DenoiserDiscriminativeHqOnnxRunner : public BaseOnnxRunner {
  void init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t num_frames = 2);

  template <typename T>
  void parse_results(std::vector<Ort::Value>& network_output, gigaedge::denoiser::TaskData& data) {
    if (model_output_names_.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    gigaedge::denoiser::DenoiseContextDiscriminativeHq<T>* context =
        dynamic_cast<gigaedge::denoiser::DenoiseContextDiscriminativeHq<T>*>(data.context);
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->fullband_hidden.assign(probabilities, probabilities + context->fullband_hidden.size());
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fb_snr_noise_psd.assign(probabilities, probabilities + context->fb_snr_noise_psd.size());
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->fb_snr_noise_pg.assign(probabilities, probabilities + context->fb_snr_noise_pg.size());
    probabilities = network_output[3].GetTensorMutableData<T>();
    context->sb_snr_noise_psd.assign(probabilities, probabilities + context->sb_snr_noise_psd.size());
    probabilities = network_output[4].GetTensorMutableData<T>();
    context->sb_snr_noise_pg.assign(probabilities, probabilities + context->sb_snr_noise_pg.size());
    probabilities = network_output[5].GetTensorMutableData<T>();
    context->fb_conv1_buf.assign(probabilities, probabilities + context->fb_conv1_buf.size());
    probabilities = network_output[6].GetTensorMutableData<T>();
    context->fb_conv3_buf.assign(probabilities, probabilities + context->fb_conv3_buf.size());
    probabilities = network_output[7].GetTensorMutableData<T>();
    auto info = network_output[7].GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();

    if (shape.empty() || shape.back() != 2) {
      throw std::runtime_error("Invalid output shape: expected last dimension = 2 (complex)");
    }

    const size_t num_feats = static_cast<size_t>(static_cast<size_t>(shape.at(1)));
    const size_t total_elems = static_cast<size_t>(info.GetElementCount());

    if (total_elems % 2 != 0) {
      throw std::runtime_error("Invalid output buffer: element count is not divisible by 2");
    }

    const size_t complex_pairs = total_elems / 2;
    if (complex_pairs % num_feats != 0) {
      throw std::runtime_error("Invalid output buffer: complex pairs not divisible by num_feats");
    }

    const size_t num_frames = complex_pairs / num_feats;
    unpack_complex_mask(probabilities, num_feats, num_frames, data.new_spectrogram);
  }

 public:
  DenoiserDiscriminativeHqOnnxRunner() = delete;
  virtual ~DenoiserDiscriminativeHqOnnxRunner() = default;
  explicit DenoiserDiscriminativeHqOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                              uint32_t inference_engine_threads = 1, bool on_gpu = false);
  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* denoise_context) override;

  template <typename T>
  void warm_up() {
    gigaedge::denoiser::Task dummy_task;
    std::unique_ptr<gigaedge::denoiser::DenoiseContextDiscriminativeHq<float>> ctx(
        new gigaedge::denoiser::DenoiseContextDiscriminativeHq<float>());
    dummy_task.data.context = ctx.get();
    std::vector<std::vector<T>*> inputs;
    init_output_tensors(dummy_task.data);

    uint64_t multi = std::abs(std::accumulate(model_input_shapes_[0].begin(), model_input_shapes_[0].end(), int32_t{1},
                                              std::multiplies<int32_t>()));
    std::vector<T> mag(multi, 1e-4);
    prepare_input_in_place(mag, inputs, dummy_task.data.context);

    BaseOnnxRunner::warm_up<T>(inputs);
  }

  void infer(gigaedge::denoiser::Task* denoise_task) override;
};

class DenoiserDiscriminativeNbOnnxRunner : public BaseOnnxRunner {
  void init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t out_shape = 2);

  template <typename T>
  void parse_results(std::vector<Ort::Value>& network_output, gigaedge::denoiser::TaskData& data) {
    if (model_output_names_.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    gigaedge::denoiser::DenoiseContextDiscriminativeNb<T>* context =
        dynamic_cast<gigaedge::denoiser::DenoiseContextDiscriminativeNb<T>*>(data.context);
    if (!context) {
      throw std::runtime_error("Invalid context type for DenoiserDiscriminativeNbOnnxRunner");
    }

    // fb_h
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->fullband_hidden_state.assign(probabilities, probabilities + context->fullband_hidden_state.size());

    // fb_c
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fullband_cell_state.assign(probabilities, probabilities + context->fullband_cell_state.size());

    // fb_conv1_buf
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->fb_conv1_buf.assign(probabilities, probabilities + context->fb_conv1_buf.size());

    // fb_conv3_buf
    probabilities = network_output[3].GetTensorMutableData<T>();
    context->fb_conv3_buf.assign(probabilities, probabilities + context->fb_conv3_buf.size());

    // complex mask output with dynamic time dimension
    probabilities = network_output[4].GetTensorMutableData<T>();
    auto info = network_output[4].GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();

    if (shape.empty() || shape.back() != 2) {
      throw std::runtime_error("Invalid output shape: expected last dimension = 2 (complex)");
    }

    const size_t num_feats = static_cast<size_t>(shape.at(1));
    const size_t total_elems = static_cast<size_t>(info.GetElementCount());

    if (total_elems % 2 != 0) {
      throw std::runtime_error("Invalid output buffer: element count is not divisible by 2");
    }

    const size_t complex_pairs = total_elems / 2;
    if (complex_pairs % num_feats != 0) {
      throw std::runtime_error("Invalid output buffer: complex pairs not divisible by num_feats");
    }

    const size_t num_frames = complex_pairs / num_feats;
    unpack_complex_mask(probabilities, num_feats, num_frames, data.new_spectrogram);
  }

 public:
  DenoiserDiscriminativeNbOnnxRunner() = delete;
  virtual ~DenoiserDiscriminativeNbOnnxRunner() = default;
  explicit DenoiserDiscriminativeNbOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                              uint32_t inference_engine_threads = 1, bool on_gpu = false);

  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* denoise_context) override;

  void infer(gigaedge::denoiser::Task* denoise_task) override;

  template <typename T>
  void warm_up() {
    gigaedge::denoiser::Task dummy_task;
    std::unique_ptr<gigaedge::denoiser::DenoiseContextDiscriminativeNb<float>> ctx(
        new gigaedge::denoiser::DenoiseContextDiscriminativeNb<float>());
    dummy_task.data.context = ctx.get();

    std::vector<std::vector<T>*> inputs;
    init_output_tensors(dummy_task.data);

    std::vector<std::vector<int64_t>> resolved_input_shapes = model_input_shapes_;
    set_time_dim_if_dynamic(resolved_input_shapes[0], time_axis_in_[0], 1);

    uint64_t multi = static_cast<uint64_t>(std::abs(std::accumulate(
        resolved_input_shapes[0].begin(), resolved_input_shapes[0].end(), int32_t{1}, std::multiplies<int32_t>())));

    std::vector<T> dummy_mag(multi, static_cast<T>(1e-4));
    prepare_input_in_place(dummy_mag, inputs, dummy_task.data.context);

    BaseOnnxRunner::warm_up<T>(inputs);
  }
};

class DenoiserHybridOnnxRunner : public BaseOnnxRunner {
  template <typename T>
  void parse_results(std::vector<Ort::Value>& network_output, gigaedge::denoiser::TaskData& data) {
    if (model_output_names_.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    gigaedge::denoiser::DenoiseContextHybrid<T>* context =
        dynamic_cast<gigaedge::denoiser::DenoiseContextHybrid<T>*>(data.context);
    if (context == nullptr || network_output.size() != 9) {
      throw std::runtime_error("Invalid hybrid context or output count");
    }
    std::vector<std::vector<T>*> states = {
        &context->fullband_hidden, &context->fb_snr_noise_psd, &context->fb_snr_noise_pg, &context->sb_snr_noise_psd,
        &context->sb_snr_noise_pg, &context->fb_conv1_buf,     &context->fb_conv3_buf,    &context->stage2_hidden};
    for (size_t i = 0; i < states.size(); ++i) {
      if (network_output[i].GetTensorTypeAndShapeInfo().GetElementCount() != states[i]->size()) {
        throw std::runtime_error("Hybrid recurrent output size mismatch");
      }
      const T* values = network_output[i].GetTensorData<T>();
      states[i]->assign(values, values + states[i]->size());
    }
    const T* probabilities = network_output[8].GetTensorData<T>();
    auto info = network_output[8].GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != model_input_shapes_[0][1] || shape[2] <= 0 ||
        static_cast<size_t>(shape[2]) != data.input_spectrogram.size() || shape[3] != 2) {
      throw std::runtime_error("Invalid hybrid spectrum shape: expected [1, F, T, 2]");
    }

    const size_t num_feats = static_cast<size_t>(shape.at(1));
    const size_t total_elems = static_cast<size_t>(info.GetElementCount());

    if (total_elems % 2 != 0) {
      throw std::runtime_error("Invalid output buffer: element count is not divisible by 2");
    }

    const size_t complex_pairs = total_elems / 2;
    if (complex_pairs % num_feats != 0) {
      throw std::runtime_error("Invalid output buffer: complex pairs not divisible by num_feats");
    }

    const size_t num_frames = complex_pairs / num_feats;
    unpack_complex_mask(probabilities, num_feats, num_frames, data.new_spectrogram);
  }

 public:
  DenoiserHybridOnnxRunner() = delete;
  virtual ~DenoiserHybridOnnxRunner() = default;
  explicit DenoiserHybridOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                    uint32_t inference_engine_threads = 1, bool on_gpu = false);

  void prepare_input_in_place(std::vector<float>& noisy_ri, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* denoise_context) override;

  template <typename T>
  void warm_up() {
    gigaedge::denoiser::Task dummy_task;
    std::unique_ptr<gigaedge::denoiser::DenoiseContextHybrid<float>> ctx(
        new gigaedge::denoiser::DenoiseContextHybrid<float>());
    dummy_task.data.context = ctx.get();

    std::vector<std::vector<T>*> inputs;
    const size_t frames = model_input_shapes_[0][2] > 0 ? model_input_shapes_[0][2] : 1;
    std::vector<T> noisy_ri(static_cast<size_t>(model_input_shapes_[0][1]) * frames * 2, 1e-4);
    prepare_input_in_place(noisy_ri, inputs, dummy_task.data.context);

    BaseOnnxRunner::warm_up<T>(inputs);
  }

  void infer(gigaedge::denoiser::Task* denoise_task) override;
};

}  // namespace onnx
}  // namespace util
