cmake_minimum_required(VERSION 3.17.3)

project( CONTRIB )

if(SPENCH_ENGINE_BUNDLE_LINKAGE STREQUAL "shared" AND BUILD_TFLITE_ENGINE)
  # A shared TFLite engine absorbs its upstream static implementation
  # libraries, making the exported bundle independent of their CMake packages.
  set(BUILD_SHARED_LIBS ON)
else()
  set(BUILD_SHARED_LIBS OFF)
endif()

function(spench_find_android_prebuilt_tflite output_variable)
  if(NOT SPENCH_PREBUILT_DEPENDENCIES_ROOT)
    set(${output_variable} "" PARENT_SCOPE)
    return()
  endif()
  if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    set(_spench_android_abi "${CMAKE_ANDROID_ARCH_ABI}")
  else()
    set(_spench_android_abi "${ANDROID_ABI}")
  endif()
  string(TOLOWER "${CMAKE_BUILD_TYPE}" _spench_build_type)
  file(GLOB _spench_candidates LIST_DIRECTORIES true
    "${SPENCH_PREBUILT_DEPENDENCIES_ROOT}/android/${_spench_android_abi}/tflite/${_spench_build_type}/shared/v1-*/prefix")
  list(LENGTH _spench_candidates _spench_candidate_count)
  if(NOT _spench_candidate_count EQUAL 1)
    message(WARNING
      "Ignoring Android prebuilt TensorFlow Lite bundles for ${_spench_android_abi} "
      "(${_spench_build_type}): expected one under ${SPENCH_PREBUILT_DEPENDENCIES_ROOT}, "
      "found ${_spench_candidate_count}. Falling back to CPM.")
    set(${output_variable} "" PARENT_SCOPE)
    return()
  endif()
  list(GET _spench_candidates 0 _spench_prebuilt_root)
  set(${output_variable} "${_spench_prebuilt_root}" PARENT_SCOPE)
endfunction()

if (BUILD_TFLITE_ENGINE)
  add_subdirectory(ctpl EXCLUDE_FROM_ALL)
  set(_spench_tflite_root "${SPENCH_PREBUILT_TFLITE_ROOT}")
  if(NOT _spench_tflite_root AND ANDROID)
    spench_find_android_prebuilt_tflite(_spench_tflite_root)
  endif()
  if(_spench_tflite_root AND EXISTS "${_spench_tflite_root}/lib/cmake/tensorflow-lite/tensorflow-liteConfig.cmake")
    # The selected artifact root is part of the build configuration.  Do not
    # let CMake reuse a package directory cached for an older bundle key.
    unset(tensorflow-lite_DIR CACHE)
    find_package(tensorflow-lite CONFIG REQUIRED
      PATHS "${_spench_tflite_root}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
    set(SPENCH_USING_PREBUILT_TFLITE ON CACHE INTERNAL
      "TensorFlow Lite is provided by a prebuilt dependency bundle." FORCE)
    set(SPENCH_RESOLVED_PREBUILT_TFLITE_ROOT "${_spench_tflite_root}" CACHE INTERNAL
      "Resolved TensorFlow Lite prebuilt dependency bundle root." FORCE)
    if(TARGET tensorflow-lite::tensorflow-lite AND NOT TARGET tensorflow-lite)
      add_library(tensorflow-lite ALIAS tensorflow-lite::tensorflow-lite)
    endif()
  else()
    if(_spench_tflite_root)
      message(WARNING "Ignoring incomplete prebuilt TensorFlow Lite bundle at ${_spench_tflite_root}; falling back to CPM.")
    endif()
    set(SPENCH_USING_PREBUILT_TFLITE OFF CACHE INTERNAL
      "TensorFlow Lite is provided by a prebuilt dependency bundle." FORCE)
    set(SPENCH_RESOLVED_PREBUILT_TFLITE_ROOT "" CACHE INTERNAL
      "Resolved TensorFlow Lite prebuilt dependency bundle root." FORCE)
    CPMGetPackage(tflite)
  endif()
elseif (BUILD_ONNX_ENGINE)
  if(SPENCH_PREBUILT_ORT_ROOT AND EXISTS "${SPENCH_PREBUILT_ORT_ROOT}/lib/cmake/onnxruntime/onnxruntimeConfig.cmake")
    # The selected artifact root is part of the build configuration.  Do not
    # let CMake reuse a package directory cached for an older bundle key.
    unset(onnxruntime_DIR CACHE)
    find_package(onnxruntime CONFIG REQUIRED
      PATHS "${SPENCH_PREBUILT_ORT_ROOT}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  else()
    if(SPENCH_PREBUILT_ORT_ROOT)
      message(WARNING "Ignoring incomplete prebuilt ONNX Runtime bundle at ${SPENCH_PREBUILT_ORT_ROOT}; falling back to CPM.")
    endif()
    set(onnxruntime_BUILD_UNIT_TESTS OFF)

    if (EMSCRIPTEN)
      set(onnxruntime_BUILD_WEBASSEMBLY_STATIC_LIB ON)
      set(onnxruntime_ENABLE_WEBASSEMBLY_THREADS OFF)
      set(onnxruntime_ENABLE_WEBASSEMBLY_SIMD ON)
      set(onnxruntime_ENABLE_TRAINING OFF)
      set(onnxruntime_ENABLE_CPU_FP16_OPS OFF)
      set(onnxruntime_USE_TENSORRT_BUILTIN_PARSER OFF)
    elseif(SPENCH_ENGINE_BUNDLE_LINKAGE STREQUAL "static")
      set(onnxruntime_BUILD_SHARED_LIB OFF)
    else()
      set(onnxruntime_BUILD_SHARED_LIB ON)
    endif()

    CPMGetPackage(onnxruntime)
  endif()
endif()

add_subdirectory(pocketfft EXCLUDE_FROM_ALL)
