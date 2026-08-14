if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # Soften iOS SDK warnings for vendored code. ORT still sets
  # COMPILE_WARNING_AS_ERROR ON per-target (adds trailing -Werror); that is
  # cleared after add_subdirectory(contrib) by spench_relax_ios_third_party_warnings().
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shorten-64-to-32 -Wno-error")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shorten-64-to-32 -Wno-error")
  set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION NO)
  set(ONNX_WERROR OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_FORCE_WERROR OFF CACHE BOOL "" FORCE)
endif()

if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(STATUS "Platform: Apple macOS")
  set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
  if(BUILD_APPLE_ARCH STREQUAL "arm")
    message(STATUS "Apple architecture: arm64 (Apple Silicon)")
    set(CMAKE_OSX_ARCHITECTURES arm64)
  elseif(BUILD_APPLE_ARCH STREQUAL "intel")
    message(STATUS "Apple architecture: x86_64 (Intel)")
    set(CMAKE_OSX_ARCHITECTURES x86_64)
  else()
    set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
    message(STATUS "Apple architecture: ${CMAKE_OSX_ARCHITECTURES} (native)")
  endif()
elseif(WIN32)
  message(STATUS "Platform: Windows")
  set(_spench_msvc_disabled_warnings
    "/wd4820 /wd5264 /wd4514 /wd4127 /wd4710 /wd5045 /wd4706 /wd4668 /wd4210 /wd4061 /wd5026 /wd5027 /wd4623 /wd4625 /wd4626 /wd5250 /wd4100 /wd5262 /wd4324 /wd4868"
  )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_spench_msvc_disabled_warnings}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_spench_msvc_disabled_warnings}")
elseif(UNIX AND CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
  message(STATUS "Platform: Linux")
elseif(ANDROID)
  message(STATUS "Platform: Android")
elseif(EMSCRIPTEN)
  message(STATUS "Platform: WebAssembly (Emscripten)")
else()
  message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
endif()

function(_spench_ios_relax_target _spench_tgt)
  get_target_property(_spench_tgt_type "${_spench_tgt}" TYPE)
  if(_spench_tgt_type STREQUAL "INTERFACE_LIBRARY"
     OR _spench_tgt_type STREQUAL "UTILITY")
    return()
  endif()

  # ORT's onnxruntime_set_compile_flags() sets COMPILE_WARNING_AS_ERROR ON,
  # which makes CMake append -Werror after any -Wno-error compile options.
  set_target_properties(${_spench_tgt} PROPERTIES
    COMPILE_WARNING_AS_ERROR OFF
    XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO
    XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION NO
  )
  target_compile_options(${_spench_tgt} PRIVATE
    -Wno-shorten-64-to-32
    -Wno-error
    -Wno-error=shorten-64-to-32
  )
endfunction()

function(spench_relax_ios_third_party_warnings)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    return()
  endif()

  string(REPLACE " -Werror" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REPLACE " -Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shorten-64-to-32 -Wno-error -Wno-error=shorten-64-to-32" PARENT_SCOPE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shorten-64-to-32 -Wno-error -Wno-error=shorten-64-to-32" PARENT_SCOPE)

  # Named targets first (covers the failing onnxruntime_common path).
  set(_spench_named_targets
    onnxruntime_common
    onnxruntime_mlas
    onnxruntime_graph
    onnxruntime_framework
    onnxruntime_util
    onnxruntime_providers
    onnxruntime_session
    onnxruntime_optimizer
    onnxruntime
    onnx
    onnx_proto
  )
  foreach(_spench_tgt IN LISTS _spench_named_targets)
    if(TARGET ${_spench_tgt})
      _spench_ios_relax_target(${_spench_tgt})
    endif()
  endforeach()

  # Only scan cmake directories that were actually add_subdirectory()'d.
  # Paths that exist on disk but were never configured (e.g. TFLite when
  # BUILD_ONNX_ENGINE=ON) must not be passed to get_property(DIRECTORY ...).
  set(_spench_dirs_to_scan)
  if(BUILD_ONNX_ENGINE OR TARGET onnxruntime_common)
    list(APPEND _spench_dirs_to_scan
      "${CMAKE_CURRENT_SOURCE_DIR}/contrib/onnxruntime/cmake"
    )
  endif()
  if(BUILD_TFLITE_ENGINE AND TARGET tensorflow-lite)
    list(APPEND _spench_dirs_to_scan
      "${CMAKE_CURRENT_SOURCE_DIR}/contrib/tflite/tensorflow/lite"
    )
  endif()

  foreach(_spench_dir IN LISTS _spench_dirs_to_scan)
    get_property(_spench_targets DIRECTORY "${_spench_dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_spench_tgt IN LISTS _spench_targets)
      if(TARGET ${_spench_tgt})
        _spench_ios_relax_target(${_spench_tgt})
      endif()
    endforeach()
  endforeach()
endfunction()
