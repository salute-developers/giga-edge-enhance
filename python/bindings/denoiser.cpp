#include "enhance/denoiser.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "enhance/packed_model.h"
#include "src/logger.h"
#include "util/packed_model/packed_model_writer.h"
#include "util/sound/resampler.h"

namespace py = pybind11;

namespace giga_edge::enhance::python {
namespace {

void check_code(GigaEdgeEnhanceRetCode code, const char* operation) {
  if (code < GigaEdgeEnhanceRetCode::SUCCESS) {
    throw std::runtime_error(std::string(operation) + " failed (native status " +
                             std::to_string(static_cast<int32_t>(code)) + ")");
  }
}

py::array_t<float> to_array(const std::vector<float>& values) {
  py::array_t<float> result(values.size());
  if (!values.empty()) {
    std::memcpy(result.mutable_data(), values.data(), values.size() * sizeof(float));
  }
  return result;
}

class DenoiserModel {
 public:
  DenoiserModel(const std::string& source, const std::string& mode, const std::string& base_directory,
                uint32_t threads) {
    if (threads == 0) {
      throw std::invalid_argument("engine_threads must be positive");
    }
    YAML::Node config;
    std::shared_ptr<PackedModelReader> packed;
    std::filesystem::path base = std::filesystem::u8path(base_directory);
    try {
      if (mode == "packed") {
        packed = std::make_shared<PackedModelReader>(source);
        config = YAML::Load(packed->config());
        config["model"]["model_bundled"] = true;
        config["model"].remove("model_path");
      } else if (mode == "file") {
        const auto path = std::filesystem::absolute(std::filesystem::u8path(source));
        config = YAML::LoadFile(path.string());
        base = path.parent_path();
      } else if (mode == "yaml") {
        config = YAML::Load(source);
      } else {
        throw std::invalid_argument("Unknown model source mode");
      }
      model_type_ = config["inference"]["model_type"].as<std::string>("denoiser_discr_web_model");
#ifdef BUILD_TFLITE_ENGINE
      if (model_type_ == "denoiser_discr_hq_model") {
        throw std::invalid_argument("fast8 is supported by the ONNX backend only");
      }
#endif
      future_frames_ = config["model"]["model_input_ctx_len"].as<uint32_t>(0);
      if (!packed) {
        auto path = std::filesystem::u8path(config["model"]["model_path"].as<std::string>());
        if (path.is_relative()) {
          path = base / path;
        }
        path = std::filesystem::absolute(path).lexically_normal();
        if (!std::filesystem::is_regular_file(path)) {
          throw std::invalid_argument("Model weights do not exist: " + path.string());
        }
        config["model"]["model_path"] = path.string();
        config["inference"]["engine_num_threads"] = threads;
        denoiser_ = std::make_shared<Denoiser>(YAML::Dump(config));
      } else {
        config["inference"]["engine_num_threads"] = threads;
        auto configured = std::make_shared<PackedModelReader>(
            giga_edge::enhance::util::write_packed_model(YAML::Dump(config), packed->model()));
        denoiser_ = std::make_shared<Denoiser>(std::move(configured));
      }
    } catch (const YAML::Exception& error) {
      throw std::invalid_argument(std::string("Invalid model configuration: ") + error.what());
    }
  }

  std::shared_ptr<Denoiser> denoiser() const { return denoiser_; }
  uint64_t sample_rate() const { return denoiser_->GetModelSampleRate(); }
  uint64_t frame_size() const { return denoiser_->GetModelInputFrameSize(); }
  uint32_t engine_threads() const { return denoiser_->GetEngineThreads(); }
  uint32_t latency_frames() const { return future_frames_ + 1; }
  const std::string& model_type() const { return model_type_; }

 private:
  std::shared_ptr<Denoiser> denoiser_;
  std::string model_type_;
  uint32_t future_frames_{0};
};

class DenoiserStream {
 public:
  DenoiserStream(const DenoiserModel& model, uint32_t input_rate, uint32_t output_rate)
      : denoiser_(model.denoiser()), input_rate_(input_rate), output_rate_(output_rate) {
    if (input_rate == 0 || output_rate == 0) {
      throw std::invalid_argument("Sample rates must be positive");
    }
    initialize();
  }
  ~DenoiserStream() { destroy_context(); }
  DenoiserStream(const DenoiserStream&) = delete;
  DenoiserStream& operator=(const DenoiserStream&) = delete;

  py::array_t<float> process(py::array_t<float, py::array::c_style> input) {
    if (input.ndim() != 1) {
      throw std::invalid_argument("Native input must be a contiguous mono float32 array");
    }
    std::vector<float> samples(static_cast<size_t>(input.size()));
    if (!samples.empty()) {
      std::memcpy(samples.data(), input.data(), samples.size() * sizeof(float));
    }
    if (!std::all_of(samples.begin(), samples.end(), [](float value) { return std::isfinite(value); })) {
      throw std::invalid_argument("Audio samples must be finite");
    }
    std::vector<float> output;
    {
      py::gil_scoped_release release;
      std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
      require_lock(lock);
      require_open();
      if (finalized_) {
        throw std::runtime_error("Stream has been flushed; reset it before processing");
      }
      const auto model_input = input_resampler_ ? input_resampler_->process(samples) : samples;
      std::vector<float> enhanced;
      consume_model_samples(model_input, false, enhanced);
      output = output_resampler_ ? output_resampler_->process(enhanced) : std::move(enhanced);
    }
    return to_array(output);
  }

  py::array_t<float> flush() {
    std::vector<float> output;
    {
      py::gil_scoped_release release;
      std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
      require_lock(lock);
      require_open();
      if (!finalized_) {
        finalized_ = true;
        std::vector<float> enhanced;
        const auto remaining = input_resampler_ ? input_resampler_->flush() : std::vector<float>{};
        consume_model_samples(remaining, true, enhanced);
        if (processed_) {
          std::vector<float> tail;
          check_code(denoiser_->GetFinalChunk(tail, context_), "flush");
          enhanced.insert(enhanced.end(), tail.begin(), tail.end());
        }
        output = output_resampler_ ? output_resampler_->process(enhanced) : std::move(enhanced);
        if (output_resampler_) {
          // Extend with silence for one output sample before draining. Fractional
          // rate ratios can otherwise round the last requested sample down.
          const uint64_t rate = denoiser_->GetModelSampleRate();
          const auto padding = output_resampler_->process(
              std::vector<float>(static_cast<size_t>((rate + output_rate_ - 1) / output_rate_), 0.0F));
          output.insert(output.end(), padding.begin(), padding.end());
          const auto tail = output_resampler_->flush();
          output.insert(output.end(), tail.begin(), tail.end());
        }
      }
    }
    return to_array(output);
  }

  void reset() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    require_lock(lock);
    require_open();
    initialize();
  }

  void close() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    require_lock(lock);
    destroy_context();
    input_resampler_.reset();
    output_resampler_.reset();
    pending_.clear();
    denoiser_.reset();
  }

  float snr() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    require_lock(lock);
    require_open();
    return denoiser_->GetStreamStat(context_).snr;
  }

 private:
  void initialize() {
    const auto rate = static_cast<uint32_t>(denoiser_->GetModelSampleRate());
    auto input_resampler =
        input_rate_ == rate ? nullptr : std::make_unique<::util::sound::Resampler>(input_rate_, rate, 1);
    auto output_resampler =
        output_rate_ == rate ? nullptr : std::make_unique<::util::sound::Resampler>(rate, output_rate_, 1);
    // Drain rate conversion before finalizing the model, including recordings
    // shorter than soxr's buffering delay. The core always receives complete hops.
    auto* next_context = denoiser_->CreateContext(rate, rate);
    destroy_context();
    context_ = next_context;
    input_resampler_ = std::move(input_resampler);
    output_resampler_ = std::move(output_resampler);
    pending_.clear();
    finalized_ = false;
    processed_ = false;
  }

  void consume_model_samples(const std::vector<float>& samples, bool final, std::vector<float>& output) {
    pending_.insert(pending_.end(), samples.begin(), samples.end());
    const auto hop = static_cast<size_t>(denoiser_->GetModelInputFrameSize());
    if (final && !pending_.empty() && pending_.size() % hop != 0) {
      pending_.resize(((pending_.size() + hop - 1) / hop) * hop, 0.0F);
    }
    const size_t complete = pending_.size() / hop * hop;
    for (size_t offset = 0; offset < complete; offset += hop) {
      std::vector<float> frame(pending_.begin() + static_cast<std::ptrdiff_t>(offset),
                               pending_.begin() + static_cast<std::ptrdiff_t>(offset + hop));
      std::vector<float> enhanced;
      check_code(denoiser_->ProcessChunk(frame, enhanced, context_), "process_chunk");
      output.insert(output.end(), enhanced.begin(), enhanced.end());
      processed_ = true;
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(complete));
  }

  static void require_lock(const std::unique_lock<std::mutex>& lock) {
    if (!lock.owns_lock()) {
      throw std::runtime_error("Concurrent use of one stream is not supported");
    }
  }
  void require_open() const {
    if (context_ == nullptr) {
      throw std::runtime_error("Stream is closed");
    }
  }
  void destroy_context() noexcept {
    if (context_ != nullptr) {
      denoiser_->DestroyContext(&context_);
    }
  }

  std::shared_ptr<Denoiser> denoiser_;
  Context<float>* context_{nullptr};
  uint32_t input_rate_;
  uint32_t output_rate_;
  bool finalized_{false};
  bool processed_{false};
  std::unique_ptr<::util::sound::Resampler> input_resampler_;
  std::unique_ptr<::util::sound::Resampler> output_resampler_;
  std::vector<float> pending_;
  std::mutex mutex_;
};

void bind_denoiser(py::module_& module) {
  module.def("set_log_level", &giga_edge::enhance::set_log_level);
  py::class_<DenoiserModel>(module, "Model", py::module_local())
      .def(py::init<const std::string&, const std::string&, const std::string&, uint32_t>(),
           py::call_guard<py::gil_scoped_release>())
      .def_property_readonly("sample_rate", &DenoiserModel::sample_rate)
      .def_property_readonly("frame_size", &DenoiserModel::frame_size)
      .def_property_readonly("engine_threads", &DenoiserModel::engine_threads)
      .def_property_readonly("latency_frames", &DenoiserModel::latency_frames)
      .def_property_readonly("model_type", &DenoiserModel::model_type);
  py::class_<DenoiserStream>(module, "Stream", py::module_local())
      .def(py::init<const DenoiserModel&, uint32_t, uint32_t>(), py::call_guard<py::gil_scoped_release>())
      .def("process", &DenoiserStream::process, py::arg("audio").noconvert())
      .def("flush", &DenoiserStream::flush)
      .def("reset", &DenoiserStream::reset, py::call_guard<py::gil_scoped_release>())
      .def("close", &DenoiserStream::close, py::call_guard<py::gil_scoped_release>())
      .def_property_readonly("snr", &DenoiserStream::snr);
}

}  // namespace
}  // namespace giga_edge::enhance::python

#ifdef BUILD_TFLITE_ENGINE
PYBIND11_MODULE(_tflite, module) {
  giga_edge::enhance::python::bind_denoiser(module);
  module.attr("backend") = "tflite";
}
#else
PYBIND11_MODULE(_onnx, module) {
  giga_edge::enhance::python::bind_denoiser(module);
  module.attr("backend") = "onnx";
}
#endif
