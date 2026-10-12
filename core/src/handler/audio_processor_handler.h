#pragma once

#include <dsplib/agc.h>
#include <dsplib/audio/compressor.h>
#include <dsplib/audio/limiter.h>
#include <dsplib/audio/noise-gate.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "spench/errors.h"
#include "src/handler/base_audio_handler.h"
#include "util/sound/loudness_control.h"
#include "util/sound/range_control.h"
#include "util/sound/stat_collector.h"

namespace gigaedge::spench {

typedef struct Parameters {
  Parameters() = default;
  virtual ~Parameters() = default;
} ParametersStruct;

typedef struct RCParameters : public Parameters {
  RCParameters() = default;
  RCParameters(uint32_t sr, float tgt_level_db, float max_db, float a_time, float r_time, float h_time)
      : sample_rate(sr),
        target_level_db(tgt_level_db),
        max_gain_db(max_db),
        attack_time(a_time),
        release_time(r_time),
        hold_time(h_time) {}
  uint32_t sample_rate = 48000;
  float target_level_db = -3;
  float max_gain_db = 10;
  float attack_time = 1e-5;
  float release_time = 300;
  float hold_time = 5;
} RCParametersStruct;

typedef struct AGCParameters : public Parameters {
  AGCParameters() = default;
  AGCParameters(dsplib::real_t tgt_level, dsplib::real_t max, uint32_t avrg_len, dsplib::real_t t_r, dsplib::real_t t_f)
      : target_level(tgt_level), max_gain(max), average_len(avrg_len), t_rise(t_r), t_fall(t_f) {}
  dsplib::real_t target_level = 1.0;
  dsplib::real_t max_gain = 20.0;
  uint32_t average_len = 48000 / 4;
  dsplib::real_t t_rise = 0.01;
  dsplib::real_t t_fall = 0.01;
} AGCParametersStruct;

typedef struct NGParameters : public Parameters {
  NGParameters() = default;
  NGParameters(uint32_t sr, dsplib::real_t thld, dsplib::real_t a_time, dsplib::real_t r_time, dsplib::real_t h_time)
      : sample_rate(sr), threshold(thld), attack_time(a_time), release_time(r_time), hold_time(h_time) {}
  uint32_t sample_rate = 48000;
  dsplib::real_t threshold = -10.0;
  dsplib::real_t attack_time = 0.05;
  dsplib::real_t release_time = 0.02;
  dsplib::real_t hold_time = 0.05;
} NGParametersStruct;

typedef struct LimParameters : public Parameters {
  LimParameters() = default;
  LimParameters(uint32_t sr, dsplib::real_t thld, dsplib::real_t k_width, dsplib::real_t a_time, dsplib::real_t r_time)
      : sample_rate(sr), threshold(thld), knee_width(k_width), attack_time(a_time), release_time(r_time) {}
  uint32_t sample_rate = 48000;
  dsplib::real_t threshold = -3.0;
  dsplib::real_t knee_width = 0.0;
  dsplib::real_t attack_time = 0.0;
  dsplib::real_t release_time = 0.2;
} LimParametersStruct;

typedef struct DRCParameters : public Parameters {
  DRCParameters() = default;
  DRCParameters(uint32_t sr, dsplib::real_t thld, uint32_t r, dsplib::real_t k_width, dsplib::real_t a_time,
                dsplib::real_t r_time)
      : sample_rate(sr), threshold(thld), ratio(r), knee_width(k_width), attack_time(a_time), release_time(r_time) {}
  uint32_t sample_rate = 48000;
  dsplib::real_t threshold = -10.0;
  uint32_t ratio = 5;
  dsplib::real_t knee_width = 0.0;
  dsplib::real_t attack_time = 0.01;
  dsplib::real_t release_time = 0.2;
} DRCParametersStruct;

typedef struct LoudnessParameters : public Parameters {
  LoudnessParameters() = default;
  LoudnessParameters(uint32_t sr, float tgt, float b_time, float max, bool vad, bool post = false)
      : sample_rate(sr),
        target_level_db(tgt),
        bypass_time(b_time),
        max_gain_db(max),
        use_vad(vad),
        post_processing(post) {}
  uint32_t sample_rate = 48000;
  float target_level_db = -15;
  float bypass_time = 1.0;
  float max_gain_db = 30;
  bool use_vad = false;
  /// When true, run compressor and limiter after the loudness normalization stage (both SIMPLE and VAD modes).
  bool post_processing = false;
} LoudnessParametersStruct;

class AudioProcItem {
 public:
  AudioProcItem() {}
  AudioProcItem(AudioProcItem&& other) = default;
  explicit AudioProcItem(const ParametersStruct* params) {}
  virtual ~AudioProcItem() = default;
  virtual std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) = 0;
  virtual std::vector<float>& operator()(const std::vector<float>& in, std::vector<float>& out) {
    return process(in, out);
  }
  virtual void set_parameters(const ParametersStruct* param) = 0;
  virtual std::string name() = 0;
};

class AudioProcItemWithStats : public AudioProcItem {
 protected:
  using Stat = util::sound::StatCollector::Stat;
  using Options = util::sound::StatCollector::Options;

  struct MetricDef {
    std::string name;
    Options opts;
  };

  void stats_configure(uint32_t sample_rate, double window_seconds, std::initializer_list<MetricDef> metrics);

  // Provide per-call updates; frames == number of *frames* advanced in time
  void stats_update(const std::string& name, std::optional<float> value, size_t frames);

  // Call once per process() to see if all metrics flushed together; if so, logs via virtual hook
  void stats_try_log_window();

  // Derived class provides concrete logging/handling
  virtual void on_stats_window(const std::vector<std::pair<std::string, util::sound::StatCollector::Stat>>& stats) = 0;

  // Helpers for formatting
  static std::string fmt_opt(const std::optional<float>& v, int32_t prec = 2) {
    if (!v.has_value()) return "n/a";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", prec, *v);
    return std::string(buf);
  }

  static std::string pack(const Stat& s, int32_t prec = 2) {
    return fmt_opt(s.min, prec) + " : " + fmt_opt(s.max, prec) + " : " + fmt_opt(s.mean, prec) + " : " +
           fmt_opt(s.median, prec) + " : " + fmt_opt(s.q95, prec) + " : " + fmt_opt(s.q5, prec);
  }

 protected:
  void stats_reset_window() {
    for (auto& kv : metrics_) {
      kv.second.collector->set_window_seconds(window_sec_);  // internal Hard reset
    }
  }

 private:
  struct Metric {
    std::unique_ptr<util::sound::StatCollector> collector;
    bool flush_this_tick{false};
    std::optional<Stat> last;
  };

  Metric* find_metric(const std::string& name) noexcept {
    for (auto& kv : metrics_)
      if (kv.first == name) return &kv.second;
    return nullptr;
  }
  uint32_t sr_{1};
  double window_sec_{10.0};
  std::vector<std::pair<std::string, Metric>> metrics_;
};

class AudioProcAGC : public AudioProcItem {
 private:
  std::unique_ptr<dsplib::Agc> ac_ptr_;

 public:
  AudioProcAGC(AudioProcAGC&& other) = default;
  AudioProcAGC() { ac_ptr_ = std::make_unique<dsplib::Agc>(1.0, 20.0, (48000 / 4)); }
  explicit AudioProcAGC(const ParametersStruct& params) { set_parameters(&params); }
  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) override {
    if (in.size() != 0) {
      dsplib::arr_real agc_input_sig(in);
      auto [sig, gain] = ac_ptr_->process(agc_input_sig);
      out = sig.to_vec();
    } else {
      out = in;
    }
    return out;
  }
  std::string name() override { return std::string("AutoGainController"); }
  void set_parameters(const ParametersStruct* params) override {
    auto parameters = dynamic_cast<const AGCParametersStruct*>(params);
    ac_ptr_.reset(new dsplib::Agc(parameters->target_level, parameters->max_gain, parameters->average_len,
                                  parameters->t_rise, parameters->t_fall));
  }
};

class AudioProcRC : public AudioProcItem {
 private:
  std::unique_ptr<util::sound::RangeControl> rctrl_ptr_;

 public:
  AudioProcRC(AudioProcRC&& other) = default;
  AudioProcRC() { rctrl_ptr_ = std::make_unique<util::sound::RangeControl>(48000); }
  explicit AudioProcRC(const RCParameters& parameters) { set_parameters(&parameters); }
  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) override {
    if (in.size() != 0) {
      auto [sig, gain] = rctrl_ptr_->process(in);
      out.swap(sig);
    } else {
      out = in;
    }
    return out;
  }
  std::string name() override { return std::string("RangeController"); }
  void set_parameters(const ParametersStruct* params) override {
    auto parameters = dynamic_cast<const RCParametersStruct*>(params);
    rctrl_ptr_.reset(new util::sound::RangeControl(parameters->sample_rate, parameters->target_level_db,
                                                   parameters->max_gain_db, parameters->attack_time,
                                                   parameters->release_time, parameters->hold_time));
  }
};

class AudioProcNoiseGate : public AudioProcItem {
 private:
  std::unique_ptr<dsplib::NoiseGate> ng_ptr_;

 public:
  AudioProcNoiseGate(AudioProcNoiseGate&& other) = default;
  AudioProcNoiseGate() { ng_ptr_ = std::make_unique<dsplib::NoiseGate>(48000, -15); }
  explicit AudioProcNoiseGate(const NGParameters& parameters) { set_parameters(&parameters); }
  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) override {
    if (in.size() != 0) {
      dsplib::arr_real ng_input_sig(in);
      auto [sig, gain] = ng_ptr_->process(ng_input_sig);
      out = sig.to_vec();
    } else {
      out = in;
    }
    return out;
  }
  std::string name() override { return std::string("NoiseGate"); }
  void set_parameters(const ParametersStruct* params) override {
    auto parameters = dynamic_cast<const NGParametersStruct*>(params);
    ng_ptr_.reset(new dsplib::NoiseGate(parameters->sample_rate, parameters->threshold, parameters->attack_time,
                                        parameters->release_time, parameters->hold_time));
  }
};

class AudioProcLimiter : public AudioProcItem {
 private:
  std::unique_ptr<dsplib::Limiter> lim_ptr_;

 public:
  AudioProcLimiter(AudioProcLimiter&& other) = default;
  AudioProcLimiter() { lim_ptr_ = std::make_unique<dsplib::Limiter>(48000, -15); }
  explicit AudioProcLimiter(const LimParameters& parameters) { set_parameters(&parameters); }
  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) override {
    if (in.size() != 0) {
      dsplib::arr_real ng_input_sig(in);
      auto [sig, gain] = lim_ptr_->process(ng_input_sig);
      out = sig.to_vec();
    } else {
      out = in;
    }
    return out;
  }
  std::string name() override { return std::string("Limiter"); }
  void set_parameters(const ParametersStruct* params) override {
    auto parameters = dynamic_cast<const LimParametersStruct*>(params);
    lim_ptr_.reset(new dsplib::Limiter(parameters->sample_rate, parameters->threshold, parameters->knee_width,
                                       parameters->attack_time, parameters->release_time));
  }
};

class AudioProcDRCompressor : public AudioProcItem {
 private:
  std::unique_ptr<dsplib::Compressor> drc_ptr_;

 public:
  explicit AudioProcDRCompressor(AudioProcDRCompressor&& other) = default;
  AudioProcDRCompressor() { drc_ptr_ = std::make_unique<dsplib::Compressor>(48000, -15); }
  explicit AudioProcDRCompressor(const DRCParameters& parameters) { set_parameters(&parameters); }
  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) override {
    if (in.empty()) {
      out.clear();
      return out;
    }
    dsplib::arr_real ng_input_sig(in);
    auto [sig, gain] = drc_ptr_->process(ng_input_sig);
    out = sig.to_vec();
    return out;
  }
  std::string name() override { return std::string("DynamicRangeCompressor"); }
  void set_parameters(const ParametersStruct* params) override {
    auto parameters = dynamic_cast<const DRCParametersStruct*>(params);
    drc_ptr_.reset(new dsplib::Compressor(parameters->sample_rate, parameters->threshold, parameters->ratio,
                                          parameters->knee_width, parameters->attack_time, parameters->release_time));
  }
};

class AudioProcLC : public AudioProcItemWithStats {
 private:
  std::unique_ptr<util::sound::ILoudnessControl> lc_;
  uint32_t sr_{48000};
  uint32_t channels_{1};  // IMPORTANT: frames = samples / channels
  double stats_window_sec_{10.0};
  std::atomic<bool> reset_stats_pending_{false};

 public:
  AudioProcLC() {
    lc_ = util::sound::LoudnessControlBuilder()
              .set_sample_rate(sr_)
              .set_method(util::sound::ILoudnessControl::simple)
              .create();

    stats_configure(sr_, stats_window_sec_, {{"input_lufs", Options::all()}, {"gain_db", Options::all()}});
  }

  explicit AudioProcLC(const LoudnessParametersStruct& parameters) { set_parameters(&parameters); }

  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out) override;

  void set_parameters(const ParametersStruct* params) override;

  std::string name() override { return std::string("LoudnessControl"); }

  void on_stats_window(const std::vector<std::pair<std::string, util::sound::StatCollector::Stat>>& stats) override;
};

class AudioProcessorHandler : public BaseAudioHandler {
 public:
  AudioProcessorHandler& operator=(const AudioProcessorHandler&) = delete;
  AudioProcessorHandler(const AudioProcessorHandler&) = delete;

  explicit AudioProcessorHandler(std::shared_ptr<AudioProcessorConfig> config);

  // Convenience API stays
  std::vector<float>& process(const std::vector<float>& in, std::vector<float>& out);
  std::vector<float>& operator()(const std::vector<float>& in, std::vector<float>& out) { return process(in, out); }

  AudioProcessorHandler& add_agc();
  AudioProcessorHandler& add_agc(const AGCParameters& parameters);
  AudioProcessorHandler& add_rc();
  AudioProcessorHandler& add_rc(const RCParameters& parameters);
  AudioProcessorHandler& add_noise_gate();
  AudioProcessorHandler& add_noise_gate(const NGParameters& parameters);
  AudioProcessorHandler& add_limiter();
  AudioProcessorHandler& add_limiter(const LimParameters& parameters);
  AudioProcessorHandler& add_dr_compressor();
  AudioProcessorHandler& add_dr_compressor(const DRCParameters& parameters);
  AudioProcessorHandler& add_lc();
  AudioProcessorHandler& add_lc(const LoudnessParameters& parameters);

  GigaEdgeSpenchRetCode process_frame(struct gigaedge::spench::Context<float>* ctx,
                                      const std::vector<float>& input_audio_frame,
                                      std::vector<float>& enhanced_audio_frame) override;

 protected:
  GigaEdgeSpenchRetCode do_start() override;
  void do_stop() override;

 private:
  std::vector<std::unique_ptr<AudioProcItem>> sequence_vec_;
};
}  // namespace gigaedge::spench
