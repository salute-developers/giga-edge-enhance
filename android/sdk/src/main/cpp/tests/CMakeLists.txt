cmake_minimum_required(VERSION 3.22.1)
project(gigadenoiser_native_tests LANGUAGES CXX)

set(GDN_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")

set(BUILD_TFLITE_ENGINE ON CACHE BOOL "Build with tflite inference engine." FORCE)
set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries dynamically." FORCE)
set(BUILD_CLI OFF CACHE BOOL "Build command-line tools and demos." FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build tests." FORCE)
set(BUILD_RESAMPLER ON CACHE BOOL "Build with libsoxr audio resampling." FORCE)
if(DEFINED ENV{GDN_PREBUILT_DEPENDENCIES_ROOT})
  set(GDN_PREBUILT_DEPENDENCIES_ROOT "$ENV{GDN_PREBUILT_DEPENDENCIES_ROOT}"
    CACHE PATH "Root containing target-specific prebuilt dependency bundles." FORCE)
endif()

add_subdirectory("${GDN_ROOT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/giga-denoiser-core")

add_executable(
    giga-denoiser-native-tests
    ring_buffer_test.cpp
    processor_async_test.cpp
    ../ring_buffer.cpp
    ../processor.cpp
)

target_include_directories(
    giga-denoiser-native-tests
    PRIVATE
    ../include
    "${GDN_ROOT_DIR}"
)

target_compile_features(giga-denoiser-native-tests PRIVATE cxx_std_20)
target_compile_definitions(
    giga-denoiser-native-tests
    PRIVATE
    GDN_TEST_MODEL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/quality48.gdn"
)

target_link_libraries(
    giga-denoiser-native-tests
    PRIVATE
    giga-denoiser
    util::packed_model
)
