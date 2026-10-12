cmake_minimum_required(VERSION 3.22.1)
project(spench_sdk LANGUAGES CXX)

set(GES_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

set(BUILD_TFLITE_ENGINE ON CACHE BOOL "Build with tflite inference engine." FORCE)
set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries dynamically." FORCE)
set(BUILD_CLI OFF CACHE BOOL "Build command-line tools and demos." FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build tests." FORCE)
set(BUILD_RESAMPLER ON CACHE BOOL "Build with libsoxr audio resampling." FORCE)
set(GES_PROFILE android-tflite CACHE STRING "Supported product profile" FORCE)
add_subdirectory("${GES_ROOT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/gigaedge-spench-core")

add_library(
    gigaedge-spench-sdk
    SHARED
    src/main/cpp/jni.cpp
    src/main/cpp/processor.cpp
    src/main/cpp/ring_buffer.cpp
)

target_include_directories(
    gigaedge-spench-sdk
    PRIVATE
    src/main/cpp/include
)

target_compile_features(gigaedge-spench-sdk PRIVATE cxx_std_20)
target_compile_options(gigaedge-spench-sdk PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(
    gigaedge-spench-sdk
    PRIVATE
    gigaedge-spench
    ${log-lib}
    ${android-lib}
)
