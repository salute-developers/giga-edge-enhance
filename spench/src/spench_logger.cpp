#include "spench/src/spench_logger.h"


namespace spench {

void InitLogger(LogLevel level,
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
  gLogger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());

  // Pattern: logger name, level, source file:line, message
  gLogger->set_pattern("%n [%l] [%s:%#] %v");

  spdlog::set_default_logger(gLogger);

  // Map LogLevel to spdlog level
  spdlog::level::level_enum lvl;
  switch (level) {
    case LogLevel::Trace: lvl = spdlog::level::trace; break;
    case LogLevel::Debug: lvl = spdlog::level::debug; break;
    case LogLevel::Info:  lvl = spdlog::level::info;  break;
    case LogLevel::Warn:  lvl = spdlog::level::warn;  break;
    case LogLevel::Error: lvl = spdlog::level::err;   break;
    default:              lvl = spdlog::level::off;   break;
  }
  spdlog::set_level(lvl);
  spdlog::flush_every(std::chrono::seconds(3));
}

void ShutdownLogger() {
  spdlog::shutdown();
  gLogger.reset();
}

struct LoggerInitializer {
  LoggerInitializer() { InitLogger(); }
};
static LoggerInitializer s_loggerInitializer;

}  // namespace spench
