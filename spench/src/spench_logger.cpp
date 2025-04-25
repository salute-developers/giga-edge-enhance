#include "spench/src/spench_logger.h"


namespace spench {

void InitLogger(const std::string& name,
                LogLevel level,
                const std::string& logfile) {
  (void) level;
  (void) logfile;
  std::vector<spdlog::sink_ptr> sinks;
  // Create logger object once and store in global
  gLogger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
  spdlog::set_default_logger(gLogger);
}

void ShutdownLogger() {
}

struct LoggerInitializer {
  LoggerInitializer() { InitLogger(); }
};
static LoggerInitializer s_loggerInitializer;

}  // namespace spench
