cmake_minimum_required(VERSION 3.22.1)
project(spench_sdk LANGUAGES CXX)

set(SPENCH_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

set(BUILD_TFLITE_ENGINE ON CACHE BOOL "Build with tflite inference engine." FORCE)
set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
set(BUILD_JAVA_BIND OFF CACHE BOOL "Compile java bind library." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries dynamically." FORCE)
set(BUILD_DEMO OFF CACHE BOOL "Build with demo." FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build tests." FORCE)
set(BUILD_WITHOUT_SOXR OFF CACHE BOOL "Build with or without libsoxr library for audio resampling." FORCE)
set(SPENCH_PROFILE android-tflite CACHE STRING "Supported product profile" FORCE)
set(CPM_SOURCE_CACHE "${SPENCH_ROOT_DIR}/out/cache/cpm" CACHE PATH "CPM source cache" FORCE)
add_subdirectory("${SPENCH_ROOT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spench-core")

add_library(
    spench-sdk
    SHARED
    src/main/cpp/spench_jni.cpp
    src/main/cpp/spench_processor.cpp
    src/main/cpp/ring_buffer.cpp
)

target_include_directories(
    spench-sdk
    PRIVATE
    src/main/cpp/include
)

target_compile_features(spench-sdk PRIVATE cxx_std_20)
target_compile_options(spench-sdk PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(
    spench-sdk
    PRIVATE
    spench
    ${log-lib}
    ${android-lib}
)
