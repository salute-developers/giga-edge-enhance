#include "spench/src/spench_logger.h"
#include <spdlog/spdlog-inl.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#ifdef _WIN32
  #include <spdlog/sinks/msvc_sink.h>
#endif

#ifdef __ANDROID__
  #include <spdlog/sinks/android_sink.h>
#endif

#ifdef __linux__
  #include <spdlog/sinks/syslog_sink.h>
#endif

namespace spench {

// Helper: map uint to spdlog level_enum
static spdlog::level::level_enum map_level(uint32_t lvl) {
  switch (lvl) {
    case 0: return spdlog::level::trace;
    case 1: return spdlog::level::debug;
    case 2: return spdlog::level::info;
    case 3: return spdlog::level::warn;
    case 4: return spdlog::level::err;
    default: return spdlog::level::off;
  }
}

void init_logger(uint32_t level,
                 const std::string& name,
                 const std::string& logfile) {
  std::vector<spdlog::sink_ptr> sinks;

  // Colored console output (all platforms)
  sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

  #ifdef _WIN32
  // Visual Studio Output window
  sinks.emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
  #endif

  #ifdef __ANDROID__
  // Android logcat
  sinks.emplace_back(std::make_shared<spdlog::sinks::android_sink_mt>());
  #endif

  #ifdef __linux__
  // Syslog on Linux
  sinks.emplace_back(
    std::make_shared<spdlog::sinks::syslog_sink_mt>(name, 0, LOG_USER, true));
  #endif

  // Optional file sink
  if (!logfile.empty()) {
    sinks.emplace_back(
      std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true));
  }

  // Create logger and configure
  g_logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());

  // Pattern: logger name, level, source file:line, message
  g_logger->set_pattern("%n [%l] [%s:%#] %v");

  spdlog::set_default_logger(g_logger);

  // Set initial level
  spdlog::level::level_enum lvl = map_level(level);
  spdlog::set_level(lvl);
  spdlog::flush_every(std::chrono::seconds(3));
}

void shutdown_logger() {
  spdlog::shutdown();
  g_logger.reset();
}

void set_log_level(unsigned int level) {
  auto lvl = map_level(level);
  spdlog::set_level(lvl);
  spdlog::flush_on(lvl);
}

#ifndef __EMSCRIPTEN__
struct LoggerInitializer {
  LoggerInitializer() { init_logger(); }
};
static LoggerInitializer s_logger_initializer;
#endif

}  // namespace spench
