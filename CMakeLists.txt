cmake_minimum_required(VERSION 3.17)

project(spench VERSION 0.10.0)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0115 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0127 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0075 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (WIN32)
add_compile_definitions(_USE_MATH_DEFINES)
endif()

if (WIN32 AND BUILD_SHARED_LIBS)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shorten-64-to-32 -Wno-error")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shorten-64-to-32 -Wno-error")
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/spench
)

find_package(Git QUIET)

option(BUILD_WITH_ADDRESS_SANITIZER "Compile with address sanitizer." OFF)
option(CMAKE_BUILD_TYPE "Build mode." Release)
option(BUILD_TFLITE_ENGINE "Build with tflite inference engine." OFF)
option(BUILD_ONNX_ENGINE "Build with onnx inference engine." ON)
option(BUILD_JAVA_BIND "Compile java bind library." OFF)
option(BUILD_SHARED_LIBS "Build all libraries dynamically." OFF)
option(BUILD_WITHOUT_SOXR "Build with or without libsoxr library for audio resampling." OFF)
option(BUILD_DEMO "Build with demo." OFF)
option(BUILD_CLI_TOOLS "Build CLI tools." ${BUILD_DEMO})
option(BUILD_APPLE_ARCH "Build for a certain apple architecture (intel or arm)" "")

if (BUILD_TFLITE_ENGINE)
  set(BUILD_ONNX_ENGINE OFF)
  add_definitions(-DBUILD_TFLITE_ENGINE)
  include_directories(${CMAKE_SOURCE_DIR}/spench/contrib/tflite)
  include_directories(${CMAKE_SOURCE_DIR}/spench/contrib/tflite/flatbuffers/include)
elseif (BUILD_ONNX_ENGINE)
  set(BUILD_TFLITE_ENGINE OFF)
  add_definitions(-DBUILD_ONNX_ENGINE)
  # ONNX Runtime 1.20 requires FlatBuffers 23.5.9; do not pick up Android SDK's newer package.
  set(CMAKE_DISABLE_FIND_PACKAGE_flatbuffers ON)
  unset(flatbuffers_DIR CACHE)
endif()

if (BUILD_WITHOUT_SOXR MATCHES "ON")
  message("Building without soxr library")
  set(BUILD_WITHOUT_SOXR True)
  add_definitions(-DBUILD_WITHOUT_SOXR)
else()
  set(BUILD_WITHOUT_SOXR False)
endif()

if (BUILD_SHARED_LIBS)
  message("Building libs in shared mode")
  set(BUILD_SHARED_LIBS True)
  if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  endif()
else()
  message("Building libs in static mode")
  set(BUILD_SHARED_LIBS False)
endif()

if (NOT CMAKE_BUILD_TYPE)
  message("Building in RELEASE mode")
  set(CMAKE_BUILD_TYPE Release)
endif()

message("Building in ${CMAKE_BUILD_TYPE} mode")
if (${CMAKE_BUILD_TYPE} STREQUAL "Release" AND NOT WIN32)
  add_link_options($<$<CONFIG:RELEASE>:-s>)
endif()

if (BUILD_WITH_ADDRESS_SANITIZER)
  if (WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  endif()
  add_definitions(-DBUILD_WITH_ADDRESS_SANITIZER)
endif()

if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message("Building for apple platform")
  # ONNX Runtime 1.20 transitively includes headers using pre-C++11 UDL spacing.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-literal-operator -Wno-error=deprecated-pragma")
  set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
  if (BUILD_APPLE_ARCH STREQUAL "arm")
    message("Building for apple silicon architecture")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
  elseif (BUILD_APPLE_ARCH STREQUAL "intel")
    message("Building for intel architecture")
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
  else()
    set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
  endif()
elseif (WIN32)
  message("Building for windows platform")
  # Most annoying and useless ones
  set(MSVC_DISABLED_WARNINGS "/wd4820 /wd5264 /wd4514 /wd4127 /wd4710 /wd5045 /wd4706 /wd4668 /wd4210 /wd4061 /wd5026 /wd5027 /wd4623 /wd4625 /wd4626 /wd5250 /wd4100 /wd5262 /wd4324 /wd4868")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MSVC_DISABLED_WARNINGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest ${MSVC_DISABLED_WARNINGS}")
elseif (UNIX AND ${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
  message("Building for linux like platform")
else()
  message("Building for android or wasm platform")
endif()

if (NOT WIN32)
  string(ASCII 27 Esc)
  set(Clear "${Esc}[m")
  set(Bold  "${Esc}[1m")
  set(Red         "${Esc}[31m")
  set(Green       "${Esc}[32m")
  set(Yellow      "${Esc}[33m")
  set(Blue        "${Esc}[34m")
  set(Magenta     "${Esc}[35m")
  set(Cyan        "${Esc}[36m")
  set(White       "${Esc}[37m")
  set(BoldRed     "${Esc}[1;31m")
  set(BoldGreen   "${Esc}[1;32m")
  set(BoldYellow  "${Esc}[1;33m")
  set(BoldBlue    "${Esc}[1;34m")
  set(BoldMagenta "${Esc}[1;35m")
  set(BoldCyan    "${Esc}[1;36m")
  set(BoldWhite   "${Esc}[1;37m")
endif()

if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

# add CPM submodules
include(cmake/CPM.cmake)
CPMUsePackageLock(package-lock.cmake)

if (NOT TARGET dsplib)
  CPMGetPackage(dsplib)
endif()

CPMGetPackage(speexdsp_spench)
CPMGetPackage(spdlog)

if (NOT TARGET yaml-cpp)
  CPMGetPackage(yaml-cpp)
endif()

CPMGetPackage(rnnoise)

add_compile_options(-Wall)
add_subdirectory(spench)

include(CTest)
enable_testing()

if (BUILD_TESTING)
   add_subdirectory(spench/tests)
endif()

if (BUILD_DEMO)
   add_subdirectory(spench/demo)
endif()

if (BUILD_CLI_TOOLS)
   add_subdirectory(spench/cli_tools)
endif()

if (BUILD_JAVA_BIND)
    message("Compile JAVA binding")
    add_subdirectory(platform_wrappers/jazz_android)
endif()

if (EMSCRIPTEN)
    message(STATUS "Compile spench for wasm (WEB)")
    add_subdirectory(platform_wrappers/wasm)
endif ()

# TODO: fix BUILD_INTERFACE bug on windows
if (NOT WIN32)
  include(GNUInstallDirs)
  include(cmake/InstallTools.cmake)

  ConfigInstallTarget(spench)

  install(FILES "spench/include/spench.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY "spench/include/spench"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
  )

  include(CPack)
  set(CPACK_PACKAGE_NAME "spench")

endif()
