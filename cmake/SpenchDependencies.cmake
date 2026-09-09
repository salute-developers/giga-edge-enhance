get_filename_component(_spench_root_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED CPM_SOURCE_CACHE)
  set(CPM_SOURCE_CACHE "${_spench_root_dir}/out/cache/cpm" CACHE PATH
      "Shared Spench CPM source cache")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
CPMUsePackageLock("${_spench_root_dir}/package-lock.cmake")

if(SPENCH_PROFILE STREQUAL "model-packer")
  CPMGetPackage(yaml-cpp)
  CPMGetPackage(argparse)
  return()
endif()

if(NOT TARGET dsplib)
  CPMGetPackage(dsplib)
endif()

CPMGetPackage(speexdsp_spench)
CPMGetPackage(spdlog)

if(NOT TARGET yaml-cpp)
  CPMGetPackage(yaml-cpp)
endif()

CPMGetPackage(rnnoise)
CPMGetPackage(pocketfft)
if(NOT TARGET pocketfft)
  add_library(pocketfft INTERFACE)
  target_include_directories(pocketfft SYSTEM INTERFACE
    $<BUILD_INTERFACE:${pocketfft_SOURCE_DIR}>)
endif()

if(BUILD_ONNX_ENGINE OR BUILD_TFLITE_ENGINE)
  CPMGetPackage(ctpl)
  if(NOT TARGET ctpl)
    add_library(ctpl INTERFACE)
    target_compile_features(ctpl INTERFACE cxx_std_14)
    target_include_directories(ctpl SYSTEM INTERFACE
      $<BUILD_INTERFACE:${ctpl_SOURCE_DIR}>)
  endif()
endif()

if(BUILD_TESTING)
  CPMGetPackage(Catch2)
endif()

function(spench_define_dependency_bundle_target)
  set(options)
  set(one_value_args)
  set(multi_value_args REQUIRED OPTIONAL)
  cmake_parse_arguments(SPENCH_BUNDLE
    "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  add_custom_target(spench_dependency_bundle)
  foreach(_spench_target IN LISTS SPENCH_BUNDLE_REQUIRED SPENCH_BUNDLE_OPTIONAL)
    if(NOT TARGET ${_spench_target})
      if(_spench_target IN_LIST SPENCH_BUNDLE_REQUIRED)
        message(FATAL_ERROR
          "Required dependency bundle target '${_spench_target}' does not exist.")
      endif()
      continue()
    endif()

    add_dependencies(spench_dependency_bundle ${_spench_target})
  endforeach()
endfunction()

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
  # Android Gradle Plugin configures release native code as RelWithDebInfo.
  # It consumes the ABI-compatible Release engine bundle built by CI.
  if(_spench_build_type STREQUAL "relwithdebinfo")
    set(_spench_build_type "release")
  endif()
  file(GLOB _spench_candidates LIST_DIRECTORIES true
    "${SPENCH_PREBUILT_DEPENDENCIES_ROOT}/android/${_spench_android_abi}/tflite/${_spench_build_type}/shared/v*-*/prefix")
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

if(BUILD_TFLITE_ENGINE)
  set(_spench_tflite_root "${SPENCH_PREBUILT_TFLITE_ROOT}")
  if(NOT _spench_tflite_root AND ANDROID)
    spench_find_android_prebuilt_tflite(_spench_tflite_root)
  endif()
  if(_spench_tflite_root AND EXISTS "${_spench_tflite_root}/lib/cmake/tensorflow-lite/tensorflow-liteConfig.cmake")
    # The selected artifact root is part of the build configuration. Do not
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
    # Keep TFLite's implementation dependencies static. The patched upstream
    # target selects its own linkage from SPENCH_ENGINE_BUNDLE_LINKAGE, so a
    # shared tensorflow-lite contains these PIC objects without exporting a
    # large graph of private CMake targets.
    set(_spench_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(_spench_position_independent_code "${CMAKE_POSITION_INDEPENDENT_CODE}")
    if(SPENCH_ENGINE_BUNDLE_LINKAGE STREQUAL "shared" OR
       (NOT SPENCH_ENGINE_BUNDLE_LINKAGE AND BUILD_SHARED_LIBS))
      set(SPENCH_TFLITE_LIBRARY_TYPE SHARED)
    else()
      set(SPENCH_TFLITE_LIBRARY_TYPE STATIC)
    endif()
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    CPMGetPackage(tflite)
    set(BUILD_SHARED_LIBS "${_spench_build_shared_libs}")
    set(CMAKE_POSITION_INDEPENDENT_CODE "${_spench_position_independent_code}")
    if(SPENCH_PREPARE_DEPENDENCY_BUNDLE)
      spench_define_dependency_bundle_target(REQUIRED tensorflow-lite)
    endif()
    if(ANDROID)
      # XNNPACK calls Android logging APIs directly. When TFLite is collected
      # into a shared dependency bundle, propagate liblog to the XNNPACK
      # shared target instead of relying on the final application link.
      find_library(_spench_android_log_library log REQUIRED)
      foreach(_spench_xnnpack_target XNNPACK xnnpack)
        if(TARGET ${_spench_xnnpack_target})
          target_link_libraries(${_spench_xnnpack_target}
            PRIVATE "${_spench_android_log_library}")
        endif()
      endforeach()
    endif()
  endif()
elseif(BUILD_ONNX_ENGINE)
  if(SPENCH_PREBUILT_ORT_ROOT AND EXISTS "${SPENCH_PREBUILT_ORT_ROOT}/lib/cmake/onnxruntime/onnxruntimeConfig.cmake")
    # The selected artifact root is part of the build configuration. Do not
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

    if(EMSCRIPTEN)
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

    if(MSVC)
      # Protobuf defaults to the static MSVC runtime even when it is linked into
      # ONNX Runtime's DLL. Match CMake's /MD default for the complete graph.
      set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
    endif()

    # Keep ONNX Runtime's own dependencies static. onnxruntime_BUILD_SHARED_LIB
    # already selects the engine linkage, and the bundle ships only the engine
    # library, so onnx and onnx_proto have to end up inside it. A shared
    # onnx_proto also fails to link on Windows: the generated protobuf
    # `*_default_instance_` symbols are data, which CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS
    # cannot export.
    set(_spench_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    CPMGetPackage(onnxruntime)
    set(BUILD_SHARED_LIBS "${_spench_build_shared_libs}")
    if(SPENCH_PREPARE_DEPENDENCY_BUNDLE AND NOT EMSCRIPTEN)
      if(SPENCH_ENGINE_BUNDLE_LINKAGE STREQUAL "static")
        spench_define_dependency_bundle_target(
          REQUIRED
            onnxruntime_common
            onnxruntime_flatbuffers
            onnxruntime_framework
            onnxruntime_graph
            onnxruntime_lora
            onnxruntime_mlas
            onnxruntime_optimizer
            onnxruntime_providers
            onnxruntime_session
            onnxruntime_util
            re2
            nsync
            nsync_cpp
          OPTIONAL
            cpuinfo
            cpuinfo_internals
            onnxruntime_mlas_arm64
            onnxruntime_mlas_x86_64
        )
      else()
        spench_define_dependency_bundle_target(REQUIRED onnxruntime)
      endif()
    endif()
  endif()
endif()
