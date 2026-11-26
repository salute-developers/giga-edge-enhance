cmake_minimum_required(VERSION 3.17)

# CMake 4 no longer accepts dependency projects that declare compatibility
# below 3.5 unless the policy floor is explicit. Several pinned third-party
# projects still carry those old declarations.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION_NUMBER" VERSION_NUMBER
     LIMIT_COUNT 1 REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+$")
if(NOT VERSION_NUMBER)
  message(FATAL_ERROR "VERSION_NUMBER must contain a SemVer core version (for example 1.0.0).")
endif()

project(giga-edge-enhance VERSION "${VERSION_NUMBER}" LANGUAGES C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0115 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0127 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0075 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

find_package(Git QUIET)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Options.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerSettings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Platform.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Profile.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerCache.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildConfig.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake")

if(GEE_PROFILE STREQUAL "model-packer")
  add_subdirectory(core/util/packed_model)
  add_subdirectory(core/cli/model_packer)
else()
  add_subdirectory(core)

  if(GEE_BUILD_PYTHON_BINDINGS)
    add_subdirectory(python/bindings)
  endif()

  include(CTest)
  enable_testing()

  if(BUILD_TESTING)
    add_subdirectory(core/tests)
  endif()

  if(BUILD_CLI)
    add_subdirectory(core/cli)
  endif()

  if(EMSCRIPTEN)
    message(STATUS "Building giga-edge-enhance for WebAssembly")
    add_subdirectory(web/packages/web-sdk/wasm)
  endif()
endif()

if(NOT GEE_PROFILE STREQUAL "model-packer" AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Install.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Summary.cmake")
