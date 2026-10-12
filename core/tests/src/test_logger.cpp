#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <catch.hpp>

#include "src/logger.h"

TEST_CASE("GigaEdgeSpench logger does not modify host logging state [logger]") {
  auto host_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  auto host_logger = std::make_shared<spdlog::logger>("gigaedge-spench-test-host", host_sink);
  host_logger->set_level(spdlog::level::warn);
  spdlog::set_default_logger(host_logger);

  gigaedge::spench::init_logger(static_cast<uint32_t>(gigaedge::spench::LogLevel::off), "gigaedge-spench-test");
  REQUIRE(spdlog::default_logger() == host_logger);
  REQUIRE(host_logger->level() == spdlog::level::warn);

  gigaedge::spench::shutdown_logger();
  REQUIRE(spdlog::default_logger() == host_logger);
  REQUIRE(host_logger->level() == spdlog::level::warn);
}
