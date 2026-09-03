cmake_minimum_required(VERSION 3.22.1)
project(spench_native_tests LANGUAGES CXX)

set(SPENCH_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../..")

set(BUILD_TFLITE_ENGINE ON CACHE BOOL "Build with tflite inference engine." FORCE)
set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries dynamically." FORCE)
set(BUILD_CLI OFF CACHE BOOL "Build command-line tools and demos." FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build tests." FORCE)
set(BUILD_RESAMPLER ON CACHE BOOL "Build with libsoxr audio resampling." FORCE)
if(DEFINED ENV{SPENCH_PREBUILT_DEPENDENCIES_ROOT})
  set(SPENCH_PREBUILT_DEPENDENCIES_ROOT "$ENV{SPENCH_PREBUILT_DEPENDENCIES_ROOT}"
    CACHE PATH "Root containing target-specific prebuilt dependency bundles." FORCE)
endif()

add_subdirectory("${SPENCH_ROOT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spench-core")

add_executable(
    spench-native-tests
    ring_buffer_test.cpp
    spench_processor_async_test.cpp
    ../ring_buffer.cpp
    ../spench_processor.cpp
)

target_include_directories(
    spench-native-tests
    PRIVATE
    ../include
    "${SPENCH_ROOT_DIR}"
)

target_compile_features(spench-native-tests PRIVATE cxx_std_20)
target_compile_definitions(
    spench-native-tests
    PRIVATE
    SPENCH_TEST_MODEL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/full48k.spench"
)

target_link_libraries(
    spench-native-tests
    PRIVATE
    spench
    util::packed_model
)
