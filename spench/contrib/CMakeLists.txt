cmake_minimum_required(VERSION 3.17.3)

project( CONTRIB )

set(BUILD_SHARED_LIBS OFF)

if (BUILD_TFLITE_ENGINE)
  # TODO: enable BUILD_SHARED_LIBS for tflite
  add_subdirectory(ctpl EXCLUDE_FROM_ALL)
  CPMGetPackage(tflite)
elseif (BUILD_ONNX_ENGINE)
  set(onnxruntime_BUILD_UNIT_TESTS OFF)

  if (EMSCRIPTEN)
    set(onnxruntime_BUILD_WEBASSEMBLY_STATIC_LIB ON)
    set(onnxruntime_ENABLE_WEBASSEMBLY_THREADS OFF)
    set(onnxruntime_ENABLE_WEBASSEMBLY_SIMD ON)
    set(onnxruntime_ENABLE_TRAINING OFF)
    set(onnxruntime_ENABLE_CPU_FP16_OPS OFF)
    set(onnxruntime_USE_TENSORRT_BUILTIN_PARSER OFF)
  else()
    set(onnxruntime_BUILD_SHARED_LIB ON)
  endif()

  CPMGetPackage(onnxruntime)
endif()

add_subdirectory(pocketfft EXCLUDE_FROM_ALL)
