#pragma once

#include <string>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
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

// LogLevel definitions:
// Trace: Very detailed, potentially high-volume logs for diagnosing issues.
// Debug: Detailed information on the flow through the system.
// Info:  Normal operational messages that highlight the progress of the application.
// Warn:  Potentially harmful situations which still allow the application to continue running.
// Error: Error events that might still allow the application to continue running.
// Off:   Disable all logging.
enum class LogLevel { Trace, Debug, Info, Warn, Error, Off };

// Global logger instance (static, inline) to be used directly: use spdlog::info(), etc. after Init.
inline std::shared_ptr<spdlog::logger> gLogger;

// Initialize the global logger (call once at startup)
// name: logger name, level: minimum log level to output, logfile: optional file path
void InitLogger(LogLevel level = LogLevel::Info,
                const std::string& name = "Spench",
                const std::string& logfile = {});

// Shutdown and flush all loggers
void ShutdownLogger();

}  // namespace spench
