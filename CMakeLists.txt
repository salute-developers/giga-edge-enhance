cmake_minimum_required(VERSION 3.17)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/SPENCH_VERSION" SPENCH_VERSION
     LIMIT_COUNT 1 REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+$")
if(NOT SPENCH_VERSION)
  message(FATAL_ERROR "SPENCH_VERSION must contain a SemVer core version (for example 1.0.0).")
endif()

project(spench VERSION "${SPENCH_VERSION}" LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0115 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0127 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0075 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

find_package(Git QUIET)

include(SpenchOptions)
include(SpenchCompilerSettings)
include(SpenchPlatform)
include(SpenchProfile)
include(SpenchCompilerCache)
include(SpenchBuildConfig)
include(SpenchDependencies)

if(SPENCH_PROFILE STREQUAL "model-packer")
  add_subdirectory(spench/util/packed_model)
  add_subdirectory(spench/cli/model_packer)
else()
  add_subdirectory(spench)

  include(CTest)
  enable_testing()

  if(BUILD_TESTING)
    add_subdirectory(spench/tests)
  endif()

  if(BUILD_CLI)
    add_subdirectory(spench/cli)
  endif()

  if(EMSCRIPTEN)
    message(STATUS "Building spench for WebAssembly")
    add_subdirectory(platform_wrappers/web/packages/web-sdk/wasm)
  endif()
endif()

if(NOT SPENCH_PROFILE STREQUAL "model-packer")
  include(SpenchInstall)
endif()

include(SpenchSummary)
