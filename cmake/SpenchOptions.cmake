option(BUILD_WITH_ADDRESS_SANITIZER "Compile with address sanitizer." OFF)
option(BUILD_TFLITE_ENGINE "Build with tflite inference engine." OFF)
option(BUILD_ONNX_ENGINE "Build with onnx inference engine." ON)
option(BUILD_SHARED_LIBS "Build all libraries dynamically." OFF)
option(BUILD_RESAMPLER "Build with libsoxr audio resampling." ON)
option(BUILD_CLI "Build command-line tools and demos." OFF)
option(BUILD_DSP "Build the engine-free DSP configuration." OFF)
option(BUILD_TESTING "Build tests." ON)
set(SPENCH_PREBUILT_ORT_ROOT "" CACHE PATH
  "Install prefix containing a compatible prebuilt ONNX Runtime bundle.")
set(SPENCH_PREBUILT_TFLITE_ROOT "" CACHE PATH
  "Install prefix containing a compatible prebuilt TensorFlow Lite bundle.")
set(SPENCH_PREBUILT_DEPENDENCIES_ROOT "" CACHE PATH
  "Root containing target-specific prebuilt dependency bundles.")
set(SPENCH_ENGINE_BUNDLE_LINKAGE "" CACHE STRING
  "Linkage used while explicitly preparing an inference-engine bundle (shared or static).")
set_property(CACHE SPENCH_ENGINE_BUNDLE_LINKAGE PROPERTY STRINGS "" shared static)

set(BUILD_APPLE_ARCH "" CACHE STRING "Apple target architecture: arm, intel, or empty for native.")

if(BUILD_DSP)
  set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
  set(BUILD_TFLITE_ENGINE OFF CACHE BOOL "Build with tflite inference engine." FORCE)
elseif(BUILD_TFLITE_ENGINE)
  set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
elseif(BUILD_ONNX_ENGINE)
  set(BUILD_TFLITE_ENGINE OFF CACHE BOOL "Build with tflite inference engine." FORCE)
  # ONNX Runtime 1.20 requires FlatBuffers 23.5.9. Do not use the newer
  # Android SDK package when it is present on the host.
  set(CMAKE_DISABLE_FIND_PACKAGE_flatbuffers ON)
  unset(flatbuffers_DIR CACHE)
endif()

# Preserve whether the spench target itself is shared. CPM deps and contrib/
# reset BUILD_SHARED_LIBS in the cache for third-party static linking.
set(_spench_build_shared ${BUILD_SHARED_LIBS})

if(_spench_build_shared)
  set(SPENCH_SHARED_LIBS ON CACHE INTERNAL "Build the spench target as a shared library" FORCE)
  message(STATUS "Library linkage: shared")
else()
  set(SPENCH_SHARED_LIBS OFF CACHE INTERNAL "Build the spench target as a shared library" FORCE)
  message(STATUS "Library linkage: static")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
  message(STATUS "Build type defaulting to Release")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
