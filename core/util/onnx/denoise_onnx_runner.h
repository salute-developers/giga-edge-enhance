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
class DenoiserQOnnxRunner : public BaseOnnxRunner {
  void init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t num_frames = 2);

  template <typename T>
  void parse_results(std::vector<Ort::Value>& network_output, gigaedge::denoiser::TaskData& data) {
    if (model_output_names_.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    gigaedge::denoiser::DenoiseContextQ<T>* context =
        dynamic_cast<gigaedge::denoiser::DenoiseContextQ<T>*>(data.context);
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
  DenoiserQOnnxRunner() = delete;
  virtual ~DenoiserQOnnxRunner() = default;
  explicit DenoiserQOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                               uint32_t inference_engine_threads = 1, bool on_gpu = false);
  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* denoise_context) override;

  template <typename T>
  void warm_up() {
    gigaedge::denoiser::Task dummy_task;
    std::unique_ptr<gigaedge::denoiser::DenoiseContextQ<float>> ctx(new gigaedge::denoiser::DenoiseContextQ<float>());
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

class DenoiserFOnnxRunner : public BaseOnnxRunner {
  void init_output_tensors(gigaedge::denoiser::TaskData& data);

  template <typename T>
  void parse_results(std::vector<Ort::Value>& network_output, gigaedge::denoiser::TaskData& data) {
    if (model_output_names_.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    gigaedge::denoiser::DenoiseContextF<T>* context =
        dynamic_cast<gigaedge::denoiser::DenoiseContextF<T>*>(data.context);
    // fb_h
    T* probabilities = network_output[0].GetTensorMutableData<T>();
    context->fullband_hidden.assign(probabilities, probabilities + context->fullband_hidden.size());

    // fb_c
    probabilities = network_output[1].GetTensorMutableData<T>();
    context->fullband_cell_state.assign(probabilities, probabilities + context->fullband_cell_state.size());

    // new_fb_out_5
    probabilities = network_output[2].GetTensorMutableData<T>();
    context->prev_fb_out.assign(probabilities, probabilities + context->prev_fb_out.size());

    // network_output
    probabilities = network_output[3].GetTensorMutableData<T>();
    for (uint64_t i = 0; i < 2 * data.new_spectrogram.real.size(); i += 2) {
      data.new_spectrogram.real[i / 2] = probabilities[i];
      data.new_spectrogram.imag[i / 2] = probabilities[i + 1];
    }
  }

 public:
  DenoiserFOnnxRunner() = delete;
  virtual ~DenoiserFOnnxRunner() = default;
  explicit DenoiserFOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                               uint32_t inference_engine_threads = 1, bool on_gpu = false);

  // change two last arguments
  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* denoise_context) override;
  void infer(gigaedge::denoiser::Task* denoise_task) override;

  template <typename T>
  void warm_up() {
    gigaedge::denoiser::Task dummy_task;
    std::unique_ptr<gigaedge::denoiser::DenoiseContextF<float>> ctx(new gigaedge::denoiser::DenoiseContextF<float>());
    dummy_task.data.context = ctx.get();

    std::vector<std::vector<T>*> inputs;
    // batch_size * num_features (for example 1 * 385)
    uint64_t multi = model_input_shapes_[0][0] * model_input_shapes_[0][1];

    std::vector<T> dummy_mag(multi, 1e-4);
    prepare_input_in_place(dummy_mag, inputs, dummy_task.data.context);
    init_output_tensors(dummy_task.data);
    BaseOnnxRunner::warm_up<T>(inputs);
  }
};

class DenoiserSFOnnxRunner : public BaseOnnxRunner {
  void init_output_tensors(gigaedge::denoiser::TaskData& data, uint32_t out_shape = 2);

  template <typename T>
  void parse_results(std::vector<Ort::Value>& network_output, gigaedge::denoiser::TaskData& data) {
    if (model_output_names_.size() != network_output.size()) {
      throw std::runtime_error("Model output error");
    }
    if (data.context == nullptr) {
      throw std::runtime_error("Context is not allocated!");
    }

    gigaedge::denoiser::DenoiseContextSF<T>* context =
        dynamic_cast<gigaedge::denoiser::DenoiseContextSF<T>*>(data.context);
    if (!context) {
      throw std::runtime_error("Invalid context type for DenoiserSFOnnxRunner");
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
  DenoiserSFOnnxRunner() = delete;
  virtual ~DenoiserSFOnnxRunner() = default;
  explicit DenoiserSFOnnxRunner(const std::shared_ptr<gigaedge::denoiser::NetworkConfig> config,
                                uint32_t inference_engine_threads = 1, bool on_gpu = false);

  void prepare_input_in_place(std::vector<float>& mag, std::vector<std::vector<float>*>& inputs,
                              gigaedge::denoiser::Context<float>* denoise_context) override;

  void infer(gigaedge::denoiser::Task* denoise_task) override;

  template <typename T>
  void warm_up() {
    gigaedge::denoiser::Task dummy_task;
    std::unique_ptr<gigaedge::denoiser::DenoiseContextSF<float>> ctx(new gigaedge::denoiser::DenoiseContextSF<float>());
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

}  // namespace onnx
}  // namespace util
