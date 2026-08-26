cmake_minimum_required(VERSION 3.17)

project(spench VERSION 0.10.0 LANGUAGES C CXX)

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
include(SpenchBuildConfig)
include(SpenchDependencies)

add_subdirectory(spench)

include(CTest)
enable_testing()

if(BUILD_TESTING)
  add_subdirectory(spench/tests)
endif()

if(BUILD_DEMO)
  add_subdirectory(spench/demo)
endif()

if(BUILD_CLI_TOOLS)
  add_subdirectory(spench/cli_tools)
endif()

if(EMSCRIPTEN)
  message(STATUS "Building spench for WebAssembly")
  add_subdirectory(platform_wrappers/wasm)
endif()

include(SpenchInstall)
include(SpenchSummary)
