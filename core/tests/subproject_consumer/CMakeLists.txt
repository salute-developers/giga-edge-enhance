cmake_minimum_required(VERSION 3.17)
project(enhance_subproject_consumer LANGUAGES C CXX)

find_package(onnxruntime CONFIG REQUIRED)
# The parent SDK defines this target for text-to-speech.
add_library(ctpl INTERFACE)
set(BUILD_ONNX_ENGINE OFF)
set(BUILD_TFLITE_ENGINE OFF)
set(BUILD_RESAMPLER OFF)
set(BUILD_TESTING OFF)
add_subdirectory(../../.. enhance)

add_executable(subproject_consumer ../package_consumer/main.cpp)
target_link_libraries(subproject_consumer PRIVATE giga-edge::enhance)
target_compile_features(subproject_consumer PRIVATE cxx_std_17)

enable_testing()
add_test(NAME subproject_consumer COMMAND subproject_consumer)
