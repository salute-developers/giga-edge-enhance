get_filename_component(_ges_root_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED CPM_SOURCE_CACHE)
  set(CPM_SOURCE_CACHE "${_ges_root_dir}/out/cache/cpm" CACHE PATH
      "Shared GigaEdgeSpench CPM source cache")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
CPMUsePackageLock("${_ges_root_dir}/package-lock.cmake")

if(GES_PROFILE STREQUAL "model-packer")
  CPMGetPackage(yaml-cpp)
  CPMGetPackage(argparse)
  return()
endif()

if(NOT TARGET dsplib)
  CPMGetPackage(dsplib)
endif()

CPMGetPackage(speexdsp_ges_core)
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

function(ges_define_dependency_bundle_target)
  set(options)
  set(one_value_args)
  set(multi_value_args REQUIRED OPTIONAL)
  cmake_parse_arguments(GES_BUNDLE
    "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  add_custom_target(ges_dependency_bundle)
  foreach(_ges_target IN LISTS GES_BUNDLE_REQUIRED GES_BUNDLE_OPTIONAL)
    if(NOT TARGET ${_ges_target})
      if(_ges_target IN_LIST GES_BUNDLE_REQUIRED)
        message(FATAL_ERROR
          "Required dependency bundle target '${_ges_target}' does not exist.")
      endif()
      continue()
    endif()

    add_dependencies(ges_dependency_bundle ${_ges_target})
  endforeach()
endfunction()

function(ges_find_android_prebuilt_tflite output_variable)
  if(NOT GES_PREBUILT_DEPENDENCIES_ROOT)
    set(${output_variable} "" PARENT_SCOPE)
    return()
  endif()
  if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    set(_ges_android_abi "${CMAKE_ANDROID_ARCH_ABI}")
  else()
    set(_ges_android_abi "${ANDROID_ABI}")
  endif()
  string(TOLOWER "${CMAKE_BUILD_TYPE}" _ges_build_type)
  # Android Gradle Plugin configures release native code as RelWithDebInfo.
  # It consumes the ABI-compatible Release engine bundle built by CI.
  if(_ges_build_type STREQUAL "relwithdebinfo")
    set(_ges_build_type "release")
  endif()
  file(GLOB _ges_candidates LIST_DIRECTORIES true
    "${GES_PREBUILT_DEPENDENCIES_ROOT}/android/${_ges_android_abi}/tflite/${_ges_build_type}/shared/v*-*/prefix")
  list(LENGTH _ges_candidates _ges_candidate_count)
  if(NOT _ges_candidate_count EQUAL 1)
    message(WARNING
      "Ignoring Android prebuilt TensorFlow Lite bundles for ${_ges_android_abi} "
      "(${_ges_build_type}): expected one under ${GES_PREBUILT_DEPENDENCIES_ROOT}, "
      "found ${_ges_candidate_count}. Falling back to CPM.")
    set(${output_variable} "" PARENT_SCOPE)
    return()
  endif()
  list(GET _ges_candidates 0 _ges_prebuilt_root)
  set(${output_variable} "${_ges_prebuilt_root}" PARENT_SCOPE)
endfunction()

if(BUILD_TFLITE_ENGINE)
  set(_ges_tflite_root "${GES_PREBUILT_TFLITE_ROOT}")
  if(NOT _ges_tflite_root AND ANDROID)
    ges_find_android_prebuilt_tflite(_ges_tflite_root)
  endif()
  if(_ges_tflite_root AND EXISTS "${_ges_tflite_root}/lib/cmake/tensorflow-lite/tensorflow-liteConfig.cmake")
    # The selected artifact root is part of the build configuration. Do not
    # let CMake reuse a package directory cached for an older bundle key.
    unset(tensorflow-lite_DIR CACHE)
    find_package(tensorflow-lite CONFIG REQUIRED
      PATHS "${_ges_tflite_root}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
    set(GES_USING_PREBUILT_TFLITE ON CACHE INTERNAL
      "TensorFlow Lite is provided by a prebuilt dependency bundle." FORCE)
    set(GES_RESOLVED_PREBUILT_TFLITE_ROOT "${_ges_tflite_root}" CACHE INTERNAL
      "Resolved TensorFlow Lite prebuilt dependency bundle root." FORCE)
    if(TARGET tensorflow-lite::tensorflow-lite AND NOT TARGET tensorflow-lite)
      add_library(tensorflow-lite ALIAS tensorflow-lite::tensorflow-lite)
    endif()
  else()
    if(_ges_tflite_root)
      message(WARNING "Ignoring incomplete prebuilt TensorFlow Lite bundle at ${_ges_tflite_root}; falling back to CPM.")
    endif()
    set(GES_USING_PREBUILT_TFLITE OFF CACHE INTERNAL
      "TensorFlow Lite is provided by a prebuilt dependency bundle." FORCE)
    set(GES_RESOLVED_PREBUILT_TFLITE_ROOT "" CACHE INTERNAL
      "Resolved TensorFlow Lite prebuilt dependency bundle root." FORCE)
    # Keep TFLite's implementation dependencies static. The patched upstream
    # target selects its own linkage from GES_ENGINE_BUNDLE_LINKAGE, so a
    # shared tensorflow-lite contains these PIC objects without exporting a
    # large graph of private CMake targets.
    set(_ges_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(_ges_position_independent_code "${CMAKE_POSITION_INDEPENDENT_CODE}")
    if(GES_ENGINE_BUNDLE_LINKAGE STREQUAL "shared" OR
       (NOT GES_ENGINE_BUNDLE_LINKAGE AND BUILD_SHARED_LIBS))
      set(GES_TFLITE_LIBRARY_TYPE SHARED)
    else()
      set(GES_TFLITE_LIBRARY_TYPE STATIC)
    endif()
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    CPMGetPackage(tflite)
    set(BUILD_SHARED_LIBS "${_ges_build_shared_libs}")
    set(CMAKE_POSITION_INDEPENDENT_CODE "${_ges_position_independent_code}")
    if(GES_PREPARE_DEPENDENCY_BUNDLE)
      ges_define_dependency_bundle_target(REQUIRED tensorflow-lite)
    endif()
    if(ANDROID)
      # XNNPACK calls Android logging APIs directly. When TFLite is collected
      # into a shared dependency bundle, propagate liblog to the XNNPACK
      # shared target instead of relying on the final application link.
      find_library(_ges_android_log_library log REQUIRED)
      foreach(_ges_xnnpack_target XNNPACK xnnpack)
        if(TARGET ${_ges_xnnpack_target})
          target_link_libraries(${_ges_xnnpack_target}
            PRIVATE "${_ges_android_log_library}")
        endif()
      endforeach()
    endif()
  endif()
elseif(BUILD_ONNX_ENGINE)
  if(GES_PREBUILT_ORT_ROOT AND EXISTS "${GES_PREBUILT_ORT_ROOT}/lib/cmake/onnxruntime/onnxruntimeConfig.cmake")
    # The selected artifact root is part of the build configuration. Do not
    # let CMake reuse a package directory cached for an older bundle key.
    unset(onnxruntime_DIR CACHE)
    find_package(onnxruntime CONFIG REQUIRED
      PATHS "${GES_PREBUILT_ORT_ROOT}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  else()
    if(GES_PREBUILT_ORT_ROOT)
      message(WARNING "Ignoring incomplete prebuilt ONNX Runtime bundle at ${GES_PREBUILT_ORT_ROOT}; falling back to CPM.")
    endif()
    set(onnxruntime_BUILD_UNIT_TESTS OFF)

    if(EMSCRIPTEN)
      set(onnxruntime_BUILD_WEBASSEMBLY_STATIC_LIB ON)
      set(onnxruntime_ENABLE_WEBASSEMBLY_THREADS OFF)
      set(onnxruntime_ENABLE_WEBASSEMBLY_SIMD ON)
      set(onnxruntime_ENABLE_TRAINING OFF)
      set(onnxruntime_ENABLE_CPU_FP16_OPS OFF)
      set(onnxruntime_USE_TENSORRT_BUILTIN_PARSER OFF)
    elseif(GES_ENGINE_BUNDLE_LINKAGE STREQUAL "static")
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
    set(_ges_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    CPMGetPackage(onnxruntime)
    set(BUILD_SHARED_LIBS "${_ges_build_shared_libs}")
    if(GES_PREPARE_DEPENDENCY_BUNDLE AND NOT EMSCRIPTEN)
      if(GES_ENGINE_BUNDLE_LINKAGE STREQUAL "static")
        ges_define_dependency_bundle_target(
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
        ges_define_dependency_bundle_target(REQUIRED onnxruntime)
      endif()
    endif()
  endif()
endif()
