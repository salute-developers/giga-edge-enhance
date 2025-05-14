#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declarations to avoid including spdlog headers
namespace spdlog {
class logger;
namespace sinks { class sink; }
using sink_ptr = std::shared_ptr<sinks::sink>;
namespace level { enum level_enum : int32_t; }
}  // namespace spdlog

namespace spench {

// LogLevel definitions (mapped to uint values):
// 0: Trace  Very detailed, potentially high-volume logs for diagnosing issues.
// 1: Debug  Detailed information on the flow through the system.
// 2: Info   Normal operational messages that highlight the progress of the application.
// 3: Warn   Potentially harmful situations which still allow the application to continue running.
// 4: Error  Error events that might still allow the application to continue running.
// 5: Off    Disable all logging.

enum class LogLevel : uint32_t { Trace = 0, Debug, Info, Warn, Error, Off };

// Global logger instance (static, inline) to be used directly: use spdlog::info(), etc. after Init.
inline std::shared_ptr<spdlog::logger> gLogger;

// Initialize the global logger (call once at startup)
// name: logger name, level: minimum log level to output, logfile: optional file path
void InitLogger(uint32_t level = static_cast<unsigned int>(LogLevel::Info),
                const std::string& name = "Spench",
                const std::string& logfile = {});

// Shutdown and flush all loggers
void ShutdownLogger();

// Change the log level at runtime (0=Trace ... 5=Off)
void SetLogLevel(uint32_t level);
}  // namespace spench
