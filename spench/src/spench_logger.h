#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <utility>

namespace spench {

// LogLevel definitions (mapped to uint values):
// 0: Trace  Very detailed, potentially high-volume logs for diagnosing issues.
// 1: Debug  Detailed information on the flow through the system.
// 2: Info   Normal operational messages that highlight the progress of the application.
// 3: Warn   Potentially harmful situations which still allow the application to continue running.
// 4: Error  Error events that might still allow the application to continue running.
// 5: Off    Disable all logging.

enum class LogLevel : uint32_t { trace = 0, debug, info, warn, error, off };

// Initialize Spench's logger without changing the host application's logger.
// name: logger name, level: minimum log level to output, logfile: optional file path
void init_logger(uint32_t level = static_cast<unsigned int>(LogLevel::info), const std::string& name = "Spench",
                 const std::string& logfile = {});

// Flush and release only Spench's logger.
void shutdown_logger();

// Change the log level at runtime (0=Trace ... 5=Off)
void set_log_level(uint32_t level);

std::shared_ptr<::spdlog::logger> get_logger();

// Keep existing internal spdlog-style calls scoped to Spench's own logger.
namespace spdlog {  // NOLINT(readability-identifier-naming)
template <typename... Args>
void trace(Args&&... args) {
  get_logger()->trace(std::forward<Args>(args)...);
}

template <typename... Args>
void debug(Args&&... args) {
  get_logger()->debug(std::forward<Args>(args)...);
}

template <typename... Args>
void info(Args&&... args) {
  get_logger()->info(std::forward<Args>(args)...);
}

template <typename... Args>
void warn(Args&&... args) {
  get_logger()->warn(std::forward<Args>(args)...);
}

template <typename... Args>
void error(Args&&... args) {
  get_logger()->error(std::forward<Args>(args)...);
}

template <typename... Args>
void critical(Args&&... args) {
  get_logger()->critical(std::forward<Args>(args)...);
}
}  // namespace spdlog
}  // namespace spench
