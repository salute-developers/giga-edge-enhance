#include "spench/src/spench_logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>
#include <utility>
#include <vector>

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

namespace {

std::mutex logger_mutex;
std::shared_ptr<::spdlog::logger> logger_instance;

// Helper: map uint to spdlog level_enum
::spdlog::level::level_enum map_level(uint32_t lvl) {
  switch (lvl) {
    case 0:
      return ::spdlog::level::trace;
    case 1:
      return ::spdlog::level::debug;
    case 2:
      return ::spdlog::level::info;
    case 3:
      return ::spdlog::level::warn;
    case 4:
      return ::spdlog::level::err;
    default:
      return ::spdlog::level::off;
  }
}

std::shared_ptr<::spdlog::logger> make_logger(uint32_t level, const std::string& name, const std::string& logfile) {
  std::vector<::spdlog::sink_ptr> sinks;

  sinks.emplace_back(std::make_shared<::spdlog::sinks::stdout_color_sink_mt>());

#ifdef _WIN32
  sinks.emplace_back(std::make_shared<::spdlog::sinks::msvc_sink_mt>());
#endif

#ifdef __ANDROID__
  sinks.emplace_back(std::make_shared<::spdlog::sinks::android_sink_mt>());
#endif

#ifdef __linux__
  sinks.emplace_back(std::make_shared<::spdlog::sinks::syslog_sink_mt>(name, 0, LOG_USER, true));
#endif

  if (!logfile.empty()) {
    sinks.emplace_back(std::make_shared<::spdlog::sinks::basic_file_sink_mt>(logfile, true));
  }

  auto result = std::make_shared<::spdlog::logger>(name, sinks.begin(), sinks.end());
  result->set_pattern("%n [%l] [%s:%#] %v");
  result->set_level(map_level(level));
  return result;
}

}  // namespace

void init_logger(uint32_t level, const std::string& name, const std::string& logfile) {
  auto replacement = make_logger(level, name, logfile);
  std::lock_guard<std::mutex> lock(logger_mutex);
  logger_instance = std::move(replacement);
}

void shutdown_logger() {
  std::shared_ptr<::spdlog::logger> logger;
  {
    std::lock_guard<std::mutex> lock(logger_mutex);
    logger = std::move(logger_instance);
  }
  if (logger) {
    logger->flush();
  }
}

void set_log_level(unsigned int level) {
  auto lvl = map_level(level);
  auto logger = get_logger();
  logger->set_level(lvl);
  logger->flush_on(lvl);
}

std::shared_ptr<::spdlog::logger> get_logger() {
  std::lock_guard<std::mutex> lock(logger_mutex);
  if (!logger_instance) {
    logger_instance = make_logger(static_cast<uint32_t>(LogLevel::info), "Spench", {});
  }
  return logger_instance;
}

}  // namespace spench
