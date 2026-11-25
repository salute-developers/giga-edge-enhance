#include <dsplib/agc.h>
#include <dsplib/audio/compressor.h>
#include <dsplib/audio/limiter.h>
#include <dsplib/audio/noise-gate.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/logger.h"
#include "util/echo/echo_canceler.h"
#include "util/metrics/mute_frame_detector.h"
#include "util/sound/loudness_control.h"
#include "util/sound/loudness_meter.h"
#include "util/sound/range_control.h"
#include "util/sound/resampler.h"
#include "util/sound/rnnoise.h"
#include "util/sound/rnnoise_vad.h"

namespace py = pybind11;

namespace giga_edge::enhance::python {
namespace {
using Parameters = std::map<std::string, double>;
using Metrics = std::map<std::string, std::optional<double>>;

std::vector<float> copy_audio(const py::array_t<float, py::array::c_style>& input) {
  if (input.ndim() != 1) {
    throw std::invalid_argument("Native input must be a contiguous mono float32 array");
  }
  std::vector<float> result(static_cast<size_t>(input.size()));
  if (!result.empty()) {
    std::memcpy(result.data(), input.data(), result.size() * sizeof(float));
  }
  if (!std::all_of(result.begin(), result.end(), [](float value) { return std::isfinite(value); })) {
    throw std::invalid_argument("Samples must be finite");
  }
  return result;
}

py::array_t<float> array(const std::vector<float>& values) {
  py::array_t<float> result(values.size());
  if (!values.empty()) {
    std::memcpy(result.mutable_data(), values.data(), values.size() * sizeof(float));
  }
  return result;
}

class Component {
 public:
  Component(std::string kind, uint32_t sample_rate, Parameters parameters)
      : kind_(std::move(kind)), sample_rate_(sample_rate), parameters_(std::move(parameters)) {
    if (sample_rate < 100 || sample_rate > 100000) {
      throw std::invalid_argument("sample_rate must be in [100, 100000]");
    }
    for (const auto& [name, value] : parameters_) {
      if (!std::isfinite(value)) {
        throw std::invalid_argument("Nonfinite parameter: " + name);
      }
    }
    initialize();
  }

  py::tuple process(const py::array_t<float, py::array::c_style>& input, const py::object& reference) {
    const auto samples = copy_audio(input);
    const auto playback = reference.is_none() ? std::vector<float>{}
                                              : copy_audio(reference.cast<py::array_t<float, py::array::c_style>>());
    std::vector<float> output;
    std::vector<float> gain;
    Metrics metrics;
    {
      py::gil_scoped_release release;
      auto lock = acquire();
      require_open();
      if (finalized_) {
        throw std::runtime_error("Component has been flushed; reset before processing");
      }
      if (!samples.empty()) {
        if (agc_) {
          auto result = agc_->process(dsplib::arr_real(samples));
          output = result.out.to_vec();
          gain = result.gain.to_vec();
        } else if (range_) {
          auto result = range_->process(samples);
          output = std::move(result.out);
          gain = std::move(result.gain);
        } else if (gate_) {
          auto result = gate_->process(dsplib::arr_real(samples));
          output = result.out.to_vec();
          gain = result.gain.to_vec();
        } else if (limiter_) {
          auto result = limiter_->process(dsplib::arr_real(samples));
          output = result.out.to_vec();
          gain = result.gain.to_vec();
        } else if (compressor_) {
          auto result = compressor_->process(dsplib::arr_real(samples));
          output = result.out.to_vec();
          gain = result.gain.to_vec();
        } else if (loudness_) {
          output = loudness_->process(samples);
        } else if (rnnoise_) {
          auto result = rnnoise_->process(samples);
          output = result.out.to_vec();
          last_metrics_["score"] = result.vad;
        } else if (vad_) {
          auto state = vad_->process(samples);
          last_metrics_ = {{"score", state.score}, {"speech", state.speech}};
          output = samples;
        } else if (meter_) {
          meter_->update(samples);
          output = samples;
        } else if (mute_) {
          if (std::any_of(samples.begin(), samples.end(), [](float value) { return std::abs(value) > 1.01F; })) {
            throw std::invalid_argument("Mute detector expects normalized PCM samples");
          }
          output = mute_->process(samples);
        } else if (resampler_) {
          output = resampler_->process(samples);
        } else if (echo_) {
          if (samples.size() != frame_size_ || playback.size() != samples.size()) {
            throw std::invalid_argument("AEC requires equal microphone/reference frame lengths");
          }
          const auto outside_pcm = [](float value) { return value < -32768.0F / 32767.0F || value > 1.0F; };
          if (std::any_of(samples.begin(), samples.end(), outside_pcm) ||
              std::any_of(playback.begin(), playback.end(), outside_pcm)) {
            throw std::invalid_argument("AEC expects normalized PCM samples");
          }
          echo_->process(samples, playback, output);
        }
      }
      metrics = snapshot();
    }
    return py::make_tuple(array(output), array(gain), metrics);
  }

  py::array_t<float> flush() {
    std::vector<float> output;
    {
      py::gil_scoped_release release;
      auto lock = acquire();
      require_open();
      if (!finalized_) {
        finalized_ = true;
        if (resampler_) {
          output = resampler_->flush();
        }
      }
    }
    return array(output);
  }

  void reset() {
    auto lock = acquire();
    require_open();
    initialize();
  }

  void close() {
    auto lock = acquire();
    agc_.reset();
    range_.reset();
    gate_.reset();
    limiter_.reset();
    compressor_.reset();
    loudness_.reset();
    rnnoise_.reset();
    vad_.reset();
    meter_.reset();
    mute_.reset();
    resampler_.reset();
    echo_.reset();
    closed_ = true;
  }

  Metrics state() {
    auto lock = acquire();
    require_open();
    return snapshot();
  }

  void set_io_ratio(double ratio) {
    auto lock = acquire();
    require_open();
    if (finalized_ || !resampler_ || !std::isfinite(ratio) || ratio <= 0 || !resampler_->set_io_ratio(ratio)) {
      throw std::invalid_argument("Unsupported resampling ratio or finalized resampler");
    }
  }

  uint32_t frame_size() const { return frame_size_; }
  uint32_t latency_samples() const { return latency_samples_; }

 private:
  double p(const std::string& name) const { return parameters_.at(name); }

  void initialize() {
    finalized_ = false;
    last_metrics_.clear();
    if (kind_ == "agc") {
      agc_ = std::make_unique<dsplib::Agc>(p("target_power_level"), p("max_gain_db"), p("average_length"),
                                           p("rise_step"), p("fall_step"));
    } else if (kind_ == "range_control") {
      range_ = std::make_unique<::util::sound::RangeControl>(sample_rate_, p("target_level_db"), p("max_gain_db"),
                                                             p("attack_time"), p("release_time"), p("hold_time"));
    } else if (kind_ == "noise_gate") {
      gate_ = std::make_unique<dsplib::NoiseGate>(sample_rate_, p("threshold_db"), p("attack_time"), p("release_time"),
                                                  p("hold_time"));
    } else if (kind_ == "limiter") {
      limiter_ = std::make_unique<dsplib::Limiter>(sample_rate_, p("threshold_db"), p("knee_width_db"),
                                                   p("attack_time"), p("release_time"));
    } else if (kind_ == "compressor") {
      compressor_ = std::make_unique<dsplib::Compressor>(sample_rate_, p("threshold_db"), p("ratio"),
                                                         p("knee_width_db"), p("attack_time"), p("release_time"));
    } else if (kind_ == "loudness_control") {
      loudness_ = ::util::sound::LoudnessControlBuilder()
                      .set_sample_rate(sample_rate_)
                      .set_method(p("use_vad") ? ::util::sound::ILoudnessControl::vadbase
                                               : ::util::sound::ILoudnessControl::simple)
                      .set_target_level(p("target_level_db"))
                      .set_max_gain(p("max_gain_db"))
                      .set_bypass_time(p("bypass_time"))
                      .set_post_processing(p("post_processing"))
                      .create();
      frame_size_ = sample_rate_ / 100;
    } else if (kind_ == "rnnoise") {
      rnnoise_ = std::make_unique<::util::sound::RNNoise>(sample_rate_);
      frame_size_ = rnnoise_->stride();
      last_metrics_["score"] = 0;
      latency_samples_ = frame_size_;
      if (sample_rate_ != 48000) {
        // Include both native FIR converters' delay, measured in output samples.
        // Their rational-rate delay() estimates are not exact for all ratios.
        dsplib::FIRResampler input(48000, sample_rate_);
        dsplib::FIRResampler output(sample_rate_, 48000);
        auto impulse = dsplib::zeros(sample_rate_ / 10);
        impulse[0] = 1;
        const auto response = output.process(input.process(impulse)).to_vec();
        const auto peak = std::max_element(response.begin(), response.end(),
                                           [](float a, float b) { return std::abs(a) < std::abs(b); });
        latency_samples_ += static_cast<uint32_t>(std::distance(response.begin(), peak));
      }
    } else if (kind_ == "vad") {
      ::util::sound::VadConfig config;
      config.sample_rate = sample_rate_;
      config.threshold = p("threshold");
      vad_ = std::make_unique<::util::sound::RNNoiseVad>(config);
      frame_size_ = vad_->frame_len();
    } else if (kind_ == "loudness_meter") {
      meter_ = std::make_unique<::util::sound::LoudnessMeter>(sample_rate_);
    } else if (kind_ == "mute_frame_detector") {
      mute_ = std::make_unique<::util::metrics::MuteFrameDetector>(p("chunk_length"), p("threshold_db"),
                                                                   p("left_right_range_db"), p("minimum_rms_db"));
    } else if (kind_ == "resampler") {
      resampler_ = std::make_unique<::util::sound::Resampler>(sample_rate_, p("output_sample_rate"), 1);
    } else if (kind_ == "echo_canceller") {
      ::util::echo::EchoCancelerConfig config;
      config.sample_rate = sample_rate_;
      config.frame_len = p("frame_size");
      config.filter_len = p("filter_length");
      config.delay_compensator = p("delay_compensation");
      config.echo_suppression = p("residual_suppression");
      config.echo_premute = p("premute_seconds");
      echo_ = ::util::echo::EchoCancelerBuilder(config).create();
      frame_size_ = config.frame_len;
    } else {
      throw std::invalid_argument("Unknown DSP component: " + kind_);
    }
  }

  Metrics snapshot() const {
    Metrics result = last_metrics_;
    if (loudness_) {
      const auto state = loudness_->state();
      result = {{"gain_db", state.current_gain_db_smoothed},
                {"short_term_lufs", state.last_short_term_lufs},
                {"maximum_lufs", state.longterm_max_lufs},
                {"minimum_lufs", state.longterm_min_lufs},
                {"bypass_active", state.bypass_active}};
    } else if (meter_) {
      const auto state = meter_->state();
      result = {{"momentary_lufs", state.momentary},
                {"short_term_lufs", state.short_term},
                {"sample_peak_dbfs", state.true_peak}};
    } else if (vad_) {
      const auto state = vad_->state();
      result = {{"score", state.score}, {"speech", state.speech}};
    } else if (resampler_) {
      result = {{"delay_ms", resampler_->accumulated_delay_ms()}};
    }
    return result;
  }

  std::unique_lock<std::mutex> acquire() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      throw std::runtime_error("Concurrent use of one component is not supported");
    }
    return lock;
  }
  void require_open() const {
    if (closed_) {
      throw std::runtime_error("Component is closed");
    }
  }

  std::string kind_;
  uint32_t sample_rate_;
  Parameters parameters_;
  uint32_t frame_size_{1};
  uint32_t latency_samples_{0};
  bool closed_{false};
  bool finalized_{false};
  Metrics last_metrics_;
  std::mutex mutex_;
  std::unique_ptr<dsplib::Agc> agc_;
  std::unique_ptr<::util::sound::RangeControl> range_;
  std::unique_ptr<dsplib::NoiseGate> gate_;
  std::unique_ptr<dsplib::Limiter> limiter_;
  std::unique_ptr<dsplib::Compressor> compressor_;
  std::unique_ptr<::util::sound::ILoudnessControl> loudness_;
  std::unique_ptr<::util::sound::RNNoise> rnnoise_;
  std::unique_ptr<::util::sound::RNNoiseVad> vad_;
  std::unique_ptr<::util::sound::LoudnessMeter> meter_;
  std::unique_ptr<::util::metrics::MuteFrameDetector> mute_;
  std::unique_ptr<::util::sound::Resampler> resampler_;
  std::unique_ptr<::util::echo::IEchoCanceler> echo_;
};
}  // namespace
}  // namespace giga_edge::enhance::python

PYBIND11_MODULE(_dsp, module) {
  using giga_edge::enhance::python::Component;
  py::class_<Component>(module, "Component", py::module_local())
      .def(py::init<std::string, uint32_t, giga_edge::enhance::python::Parameters>(),
           py::call_guard<py::gil_scoped_release>())
      .def("process", &Component::process, py::arg("audio").noconvert(), py::arg("reference") = py::none())
      .def("flush", &Component::flush)
      .def("reset", &Component::reset, py::call_guard<py::gil_scoped_release>())
      .def("close", &Component::close, py::call_guard<py::gil_scoped_release>())
      .def("set_io_ratio", &Component::set_io_ratio, py::call_guard<py::gil_scoped_release>())
      .def_property_readonly("frame_size", &Component::frame_size)
      .def_property_readonly("latency_samples", &Component::latency_samples)
      .def_property_readonly("state", &Component::state);
  module.def("set_log_level", &giga_edge::enhance::set_log_level);
}
