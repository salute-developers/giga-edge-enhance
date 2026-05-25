#include "spench/src/handler/audio_processor_handler.h"

#include <spdlog/spdlog.h>
#include "spench/src/spench_logger.h"

namespace spench {

void AudioProcItemWithStats::stats_configure(uint32_t sample_rate,
                                             double window_seconds,
                                             std::initializer_list<MetricDef> metrics) {
  m_sr = (sample_rate == 0 ? 1u : sample_rate);
  m_window_sec = (window_seconds > 0.0 ? window_seconds : 1.0);

  m_metrics.clear();
  m_metrics.reserve(metrics.size());

  for (const auto& def : metrics) {
    auto col = std::make_unique<util::sound::StatCollector>(
        def.name, m_sr, m_window_sec, def.opts);
    // pair<name, Metric>
    m_metrics.emplace_back(def.name, Metric{std::move(col), false, std::nullopt});
  }
}

void AudioProcItemWithStats::stats_update(const std::string& name,
                                          std::optional<float> value,
                                          size_t frames) {
  for (auto& kv : m_metrics) {
    if (kv.first == name) {
      auto& metric = kv.second;
      std::optional<util::sound::StatCollector::Stat> flushed;
      if (value.has_value()) {
        flushed = metric.collector->update(*value, frames);
      } else {
        flushed = metric.collector->advance(frames);
      }
      metric.m_flush_this_tick = static_cast<bool>(flushed);
      if (flushed) {
        metric.last = *flushed;
      }
      break;
    }
  }
}

void AudioProcItemWithStats::stats_try_log_window() {
  if (m_metrics.empty()) {
    return;
  }

  bool all_flushed = true;
  for (auto& kv : m_metrics) {
    all_flushed &= kv.second.m_flush_this_tick;
  }

  if (all_flushed) {
    std::vector<std::pair<std::string, util::sound::StatCollector::Stat>> snapshot;
    snapshot.reserve(m_metrics.size());
    for (auto& kv : m_metrics) {
      snapshot.emplace_back(kv.first, kv.second.last.value_or(util::sound::StatCollector::Stat{}));
    }
    on_stats_window(snapshot);
  }

  for (auto& kv : m_metrics) {
    kv.second.m_flush_this_tick = false;
  }
}

std::vector<float>& AudioProcLC::process(const std::vector<float>& in,
                                         std::vector<float>& out) {
  if (m_reset_stats_pending.exchange(false, std::memory_order_acq_rel)) {
    stats_reset_window();
  }

  if (!in.empty()) {
    out = m_lc->process(in);
  } else {
    out = in;
  }

  // --- Stats update (windowed at base class) ---
  const size_t frames = (m_channels > 0) ? (in.size() / m_channels) : in.size();

  const auto st = m_lc->state();  // one snapshot

  // LUFS before gain
  if (st.last_short_term_lufs.has_value()) {
    stats_update("input_lufs", *st.last_short_term_lufs, frames);
  } else {
    stats_update("input_lufs", std::nullopt, frames);
  }

  stats_update("gain_db", st.current_gain_db_smoothed, frames);
  stats_try_log_window();
  return out;
}

void AudioProcLC::SetParameters(const ParametersStruct* params) {
  const auto* p = dynamic_cast<const LoudnessParametersStruct*>(params);
  if (!p) {
    return;
  }

  m_sr = p->sample_rate;
  // m_stats_window_sec: if you add it in the future, wire it here. For now keep default.

  m_lc = util::sound::LoudnessControlBuilder()
              .set_sample_rate(m_sr)
              .set_method(p->use_vad ?
                          util::sound::ILoudnessControl::vadbase :
                          util::sound::ILoudnessControl::simple)
              .set_max_gain(p->max_gain_db)
              .set_target_level(p->target_level_db)
              .set_post_processing(p->post_processing)
              .set_bypass_time(p->bypass_time)
              .create();

  stats_configure(m_sr,
                  m_stats_window_sec,
                  { {"input_lufs", Options::All()}, {"gain_db", Options::All()} });
  m_reset_stats_pending.store(true, std::memory_order_release);
}

void AudioProcLC::on_stats_window(
  const std::vector<std::pair<std::string,
                                util::sound::StatCollector::Stat>>& stats) {
  const util::sound::StatCollector::Stat* in = nullptr;
  const util::sound::StatCollector::Stat* gn = nullptr;

  for (const auto& kv : stats) {
    if (kv.first == "input_lufs") {
      in = &kv.second;
    } else if (kv.first == "gain_db") {
      gn = &kv.second;
    }
  }
  if (!in || !gn) {
    return;
  }

  const auto st = m_lc->state();

  spdlog::info(
      "[LoudnessControl] window={}s input LUFS [min:max:mean:median:q95:q5] = [{}]\n"
      "[LoudnessControl] window={}s gain dB [min:max:mean:median:q95:q5] = [{}]\n"
      "[LoudnessControl] window={}s LoudnessStat[max={}, min={}]",
      m_stats_window_sec, pack(*in),
      m_stats_window_sec, pack(*gn),
      m_stats_window_sec, st.longterm_max_lufs, st.longterm_min_lufs);
}

AudioProcessorHandler::AudioProcessorHandler(std::shared_ptr<AudioProcessorConfig> config)
    : BaseAudioHandler(std::move(config)) {
  SpenchRetCode ret_code = Restart();

  if (ret_code != SpenchRetCode::SUCCESS) {
    spdlog::error("Due to errors an audio processing handler module could not be created! ret_code: ",
                                        std::to_string(static_cast<int32_t>(ret_code)));
    throw std::runtime_error("Due to errors an audio processing handler module could not be created! ret_code: " +
                                        std::to_string(static_cast<int32_t>(ret_code)));
  }
  spdlog::debug("Audio Processing container created!");
}

SpenchRetCode AudioProcessorHandler::DoStart() {
  return SpenchRetCode::SUCCESS;
}

void AudioProcessorHandler::DoStop() {
  // Keep the chain by default; clear if you want a pristine Restart():
  m_sequence_vec.clear();
}

// ---- Builders (now always read via params()["sound_processing"]) ----

AudioProcessorHandler& AudioProcessorHandler::addAGC() {
  YAML::Node sp = params()["sound_processing"];
  if (sp && sp["auto_gain_ctrl"]) {
    AGCParameters agc_params;
    YAML::Node agc = sp["auto_gain_ctrl"];
    agc_params.target_level = agc["target_level"].as<dsplib::real_t>();
    agc_params.max_gain = agc["max_gain"].as<dsplib::real_t>();
    agc_params.average_len = agc["average_len"].as<uint32_t>();
    agc_params.t_rise = agc["step_size_rise"].as<dsplib::real_t>();
    agc_params.t_fall = agc["step_size_fall"].as<dsplib::real_t>();

    m_sequence_vec.emplace_back(std::make_unique<AudioProcAGC>(agc_params));
    spdlog::info("New Auto Gain Control element created. target_level: {}, max_gain: {}, average_len: {}, "
                 "t_rise: {}, t_fall: {}",
                 agc_params.target_level, agc_params.max_gain, agc_params.average_len,
                 agc_params.t_rise, agc_params.t_fall);
  } else {
    m_sequence_vec.emplace_back(std::make_unique<AudioProcAGC>());
    spdlog::debug("New Auto Gain Control element created with params by default");
  }
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addAGC(const AGCParameters& p) {
  m_sequence_vec.emplace_back(std::make_unique<AudioProcAGC>(p));
  spdlog::info("New Auto Gain Control element created. target_level: {}, max_gain: {}, average_len: {}, "
               "t_rise: {}, t_fall: {}",
               p.target_level, p.max_gain, p.average_len, p.t_rise, p.t_fall);
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addRC() {
  YAML::Node sp = params()["sound_processing"];
  if (sp && sp["range_ctrl"]) {
    RCParameters rc;
    YAML::Node n = sp["range_ctrl"];
    rc.sample_rate = n["sample_rate"].as<uint32_t>();
    rc.target_level_db = n["target_level_db"].as<float>();
    rc.max_gain_db = n["max_gain_db"].as<float>();
    rc.attack_time = n["attack_time"].as<float>();
    rc.release_time = n["release_time"].as<float>();
    rc.hold_time = n["hold_time"].as<float>();

    m_sequence_vec.emplace_back(std::make_unique<AudioProcRC>(rc));
    spdlog::info("New Range Control element created. sr: {}, target_level_db: {}, max_gain_db: {}, "
                 "attack_time: {}, release_time {}, hold_time {}",
                 rc.sample_rate, rc.target_level_db, rc.max_gain_db,
                 rc.attack_time, rc.release_time, rc.hold_time);
  } else {
    m_sequence_vec.emplace_back(std::make_unique<AudioProcRC>());
    spdlog::debug("New Range Control element created with params by default");
  }
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addRC(const RCParameters& p) {
  m_sequence_vec.emplace_back(std::make_unique<AudioProcRC>(p));
  spdlog::info("New Range Control element created. sr: {}, target_level_db: {}, max_gain_db: {}, "
               "attack_time: {}, release_time {}, hold_time {}",
               p.sample_rate, p.target_level_db, p.max_gain_db,
               p.attack_time, p.release_time, p.hold_time);
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addNoiseGate() {
  YAML::Node sp = params()["sound_processing"];
  if (sp && sp["noise_gate_ctrl"]) {
    NGParameters ng;
    YAML::Node n = sp["noise_gate_ctrl"];
    ng.sample_rate = n["sample_rate"].as<uint32_t>();
    ng.threshold = n["threshold"].as<dsplib::real_t>();
    ng.attack_time = n["attack_time"].as<dsplib::real_t>();
    ng.release_time = n["release_time"].as<dsplib::real_t>();
    ng.hold_time = n["hold_time"].as<dsplib::real_t>();

    m_sequence_vec.emplace_back(std::make_unique<AudioProcNoiseGate>(ng));
    spdlog::info("New Noise Gate element created. sr: {}, threshold: {}, attack_time: {}, release_time {}, "
                 "hold_time {}",
                 ng.sample_rate, ng.threshold, ng.attack_time, ng.release_time, ng.hold_time);
  } else {
    m_sequence_vec.emplace_back(std::make_unique<AudioProcNoiseGate>());
    spdlog::debug("New Noise Gate element created with params by default");
  }
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addNoiseGate(const NGParameters& p) {
  m_sequence_vec.emplace_back(std::make_unique<AudioProcNoiseGate>(p));
  spdlog::info("New Noise Gate element created. sr: {}, threshold: {}, attack_time: {}, release_time {}, "
               "hold_time {}",
               p.sample_rate, p.threshold, p.attack_time, p.release_time, p.hold_time);
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addLimiter() {
  YAML::Node sp = params()["sound_processing"];
  if (sp && sp["limiter_ctrl"]) {
    LimParameters lp;
    YAML::Node n = sp["limiter_ctrl"];
    lp.sample_rate = n["sample_rate"].as<uint32_t>();
    lp.threshold = n["threshold"].as<dsplib::real_t>();
    lp.knee_width = n["knee_width"].as<dsplib::real_t>();
    lp.attack_time = n["attack_time"].as<dsplib::real_t>();
    lp.release_time = n["release_time"].as<dsplib::real_t>();

    spdlog::info("New Limiter element created. sr: {}, threshold: {}, knee_width: {}, attack_time {}, "
                 "release_time {}",
                 lp.sample_rate, lp.threshold, lp.knee_width, lp.attack_time, lp.release_time);
    m_sequence_vec.emplace_back(std::make_unique<AudioProcLimiter>(lp));
  } else {
    spdlog::debug("New Limiter element created with params by default");
    m_sequence_vec.emplace_back(std::make_unique<AudioProcLimiter>());
  }
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addLimiter(const LimParameters& p) {
  spdlog::info("New Limiter element created. sr: {}, threshold: {}, knee_width: {}, attack_time {}, "
               "release_time {}",
               p.sample_rate, p.threshold, p.knee_width, p.attack_time, p.release_time);
  m_sequence_vec.emplace_back(std::make_unique<AudioProcLimiter>(p));
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addDRCompressor() {
  YAML::Node sp = params()["sound_processing"];
  if (sp && sp["dynamic_range_compressor_ctrl"]) {
    DRCParameters drc;
    YAML::Node n = sp["dynamic_range_compressor_ctrl"];
    drc.sample_rate = n["sample_rate"].as<uint32_t>();
    drc.threshold = n["threshold"].as<dsplib::real_t>();
    drc.ratio = n["ratio"].as<uint32_t>();
    drc.knee_width = n["knee_width"].as<dsplib::real_t>();
    drc.attack_time = n["attack_time"].as<dsplib::real_t>();
    drc.release_time = n["release_time"].as<dsplib::real_t>();

    m_sequence_vec.emplace_back(std::make_unique<AudioProcDRCompressor>(drc));
    spdlog::info("New Dynamic Range Compressor element created. sr: {}, threshold: {}, ratio: {}, "
                 "knee_width: {}, attack_time {}, release_time {}",
                 drc.sample_rate, drc.threshold, drc.ratio, drc.knee_width,
                 drc.attack_time, drc.release_time);
  } else {
    m_sequence_vec.emplace_back(std::make_unique<AudioProcDRCompressor>());
    spdlog::debug("New Dynamic Range Compressor element created with params by default");
  }
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addDRCompressor(const DRCParameters& p) {
  m_sequence_vec.emplace_back(std::make_unique<AudioProcDRCompressor>(p));
  spdlog::info("New Dynamic Range Compressor element created. sr: {}, threshold: {}, ratio: {}, "
               "knee_width: {}, attack_time {}, release_time {}",
               p.sample_rate, p.threshold, p.ratio, p.knee_width, p.attack_time, p.release_time);
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addLC() {
  YAML::Node sp = params()["sound_processing"];
  if (sp && sp["loudness_ctrl"]) {
    LoudnessParameters lc;
    YAML::Node n = sp["loudness_ctrl"];
    lc.sample_rate = n["sample_rate"].as<uint32_t>();
    lc.target_level_db = n["target_level_db"].as<float>();
    lc.max_gain_db = n["max_gain_db"].as<float>();
    lc.bypass_time = n["bypass_time"].as<float>();
    lc.use_vad = n["use_vad"].as<bool>();
    lc.post_processing = n["post_processing"] ? n["post_processing"].as<bool>() : false;

    m_sequence_vec.emplace_back(std::make_unique<AudioProcLC>(lc));
    spdlog::info("New Loudness Control element created. sr: {}, target_level_db: {}, max_gain_db: {}, "
                 "bypass_time: {}, vad enabled: {}, post_processing: {}",
                 lc.sample_rate, lc.target_level_db, lc.max_gain_db, lc.bypass_time, lc.use_vad,
                 lc.post_processing);
  } else {
    m_sequence_vec.emplace_back(std::make_unique<AudioProcLC>());
    spdlog::debug("New Loudness Control element created with params by default");
  }
  return *this;
}

AudioProcessorHandler& AudioProcessorHandler::addLC(const LoudnessParameters& p) {
  m_sequence_vec.emplace_back(std::make_unique<AudioProcLC>(p));
  spdlog::info("New Loudness Control element created. sr: {}, target_level_db: {}, max_gain_db: {}, "
               "bypass_time: {}, vad enabled: {}, post_processing: {}",
               p.sample_rate, p.target_level_db, p.max_gain_db, p.bypass_time, p.use_vad, p.post_processing);
  return *this;
}

std::vector<float>& AudioProcessorHandler::process(const std::vector<float>& in,
                                                   std::vector<float>& out) {
  std::vector<float> res(in);
  for (auto& item : m_sequence_vec) {
    res = item->process(res, res);
  }
  res.swap(out);
  return out;
}

SpenchRetCode AudioProcessorHandler::ProcessFrame(struct spench::SpenchContext<float>*,
                                                  const std::vector<float>& input_audio_frame,
                                                  std::vector<float>& enhanced_audio_frame) {
  (void)process(input_audio_frame, enhanced_audio_frame);
  return SpenchRetCode::SUCCESS;
}
}  // namespace spench
