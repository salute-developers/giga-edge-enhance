cmake_minimum_required(VERSION 3.22.1)
project(spench_native_tests LANGUAGES CXX)

set(SPENCH_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../..")
set(CPM_SOURCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.cpm" CACHE PATH "CPM source cache")

set(BUILD_TFLITE_ENGINE ON CACHE BOOL "Build with tflite inference engine." FORCE)
set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
set(BUILD_JAVA_BIND OFF CACHE BOOL "Compile java bind library." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries dynamically." FORCE)
set(BUILD_DEMO OFF CACHE BOOL "Build with demo." FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build tests." FORCE)
set(BUILD_WITHOUT_MODEL_ENCRYPTION ON CACHE BOOL "Build with or without model encryption supporting." FORCE)
set(BUILD_WITHOUT_SOXR OFF CACHE BOOL "Build with or without libsoxr library for audio resampling." FORCE)
set(GIT_SUBMODULE OFF CACHE BOOL "Check submodules during build" FORCE)
set(TENSORFLOW_SOURCE_DIR "${SPENCH_ROOT_DIR}/spench/contrib/tflite" CACHE PATH "Directory that contains the TensorFlow project" FORCE)

add_subdirectory("${SPENCH_ROOT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spench-core")

add_executable(
    spench-native-tests
    ring_buffer_test.cpp
    spench_processor_async_test.cpp
    ../ring_buffer.cpp
    ../spench_processor.cpp
    "${SPENCH_ROOT_DIR}/spench/src/packed_model.cpp"
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
)
