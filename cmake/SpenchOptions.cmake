option(BUILD_WITH_ADDRESS_SANITIZER "Compile with address sanitizer." OFF)
option(BUILD_TFLITE_ENGINE "Build with tflite inference engine." OFF)
option(BUILD_ONNX_ENGINE "Build with onnx inference engine." ON)
option(BUILD_JAVA_BIND "Compile java bind library." OFF)
option(BUILD_SHARED_LIBS "Build all libraries dynamically." OFF)
option(BUILD_WITHOUT_SOXR "Build without libsoxr audio resampling." OFF)
option(BUILD_DEMO "Build demo applications." OFF)
option(BUILD_CLI_TOOLS "Build CLI tools." ${BUILD_DEMO})
option(BUILD_DSP_WASM "Build WASM DSP module instead of full spench WASM." OFF)

set(BUILD_APPLE_ARCH "" CACHE STRING "Apple target architecture: arm, intel, or empty for native.")

if(BUILD_TFLITE_ENGINE)
  set(BUILD_ONNX_ENGINE OFF CACHE BOOL "Build with onnx inference engine." FORCE)
elseif(BUILD_ONNX_ENGINE)
  set(BUILD_TFLITE_ENGINE OFF CACHE BOOL "Build with tflite inference engine." FORCE)
endif()

# Preserve whether the spench target itself is shared. CPM deps and contrib/
# reset BUILD_SHARED_LIBS in the cache for third-party static linking.
set(_spench_build_shared ${BUILD_SHARED_LIBS})
if(ANDROID AND BUILD_JAVA_BIND)
  set(_spench_build_shared ON)
endif()

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
