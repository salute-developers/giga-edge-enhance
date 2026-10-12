set(GES_DEPS
  util__echo
  util::sound
  yaml-cpp
  spdlog_header_only
  dsplib
)

ges_relax_ios_third_party_warnings()

if(BUILD_RESAMPLER)
  if(EMSCRIPTEN)
    set(WITH_LSR_BINDINGS OFF)
  endif()
  set(_ges_build_shared_libs "${BUILD_SHARED_LIBS}")
  CPMGetPackage(soxr)
  # Clang -O1 and above resample differently from Debug and from the GCC Release
  # that produced the golden waveforms. -O0 is last so it overrides Release -O3
  # for this target only.
  if(TARGET soxr AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(soxr PRIVATE -O0)
  endif()
  set(BUILD_SHARED_LIBS "${_ges_build_shared_libs}" CACHE BOOL "Build all libraries dynamically." FORCE)
endif()

add_subdirectory(util EXCLUDE_FROM_ALL)

set(GES_SOURCES
  src/config_reader.cpp
  src/handler/base_audio_handler.cpp
  src/handler/acoustic_echo_cancelation_handler.cpp
  src/handler/audio_processor_handler.cpp
  src/audio_processor.cpp
  src/echo_canceller.cpp
  src/logger.cpp
  src/pipeline_stage.cpp
  src/task.cpp
  src/feature_params.cpp
)

if(BUILD_TFLITE_ENGINE OR BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND GES_SOURCES
    src/denoiser_model_adviser.cpp
    src/handler/neural_network_audio_handler.cpp
    src/handler/denoiser_audio_handler.cpp
    src/windowing.cpp
    src/feature_extractor.cpp
    src/feature_extractor_runner.cpp
    src/ifft_calculator.cpp
    src/fft_calculator.cpp
    src/inference_runner.cpp
    src/denoiser.cpp
    src/c_api.cpp
  )
  list(APPEND GES_DEPS util__packed_model ctpl pocketfft)
endif()

if(BUILD_TFLITE_ENGINE)
  list(APPEND GES_DEPS util__tf_lite)
elseif(BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND GES_DEPS util__onnx)
endif()

if(BUILD_RESAMPLER)
  list(APPEND GES_DEPS soxr)
endif()

if(GES_SHARED_LIBS)
  add_library(gigaedge-spench SHARED ${GES_SOURCES})
else()
  add_library(gigaedge-spench STATIC ${GES_SOURCES})
endif()

if(APPLE)
  set_target_properties(gigaedge-spench PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(gigaedge-spench PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

add_library(gigaedge-spench::gigaedge-spench ALIAS gigaedge-spench)

target_include_directories(gigaedge-spench
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
  target_link_libraries(gigaedge-spench PRIVATE ges_build_config ${GES_DEPS})
else()
  target_link_libraries(gigaedge-spench PRIVATE $<BUILD_INTERFACE:ges_build_config>)
  foreach(_ges_dep IN LISTS GES_DEPS)
    target_link_libraries(gigaedge-spench PRIVATE $<BUILD_INTERFACE:${_ges_dep}>)
  endforeach()
endif()

if(ANDROID)
  message(STATUS "Applying 16KB page alignment for Android (gigaedge-spench)")
  target_link_options(gigaedge-spench PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(GES_SHARED_LIBS)
  if(BUILD_TESTING)
    message(WARNING "Disabling -fvisibility=hidden for shared library build with tests")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(gigaedge-spench PRIVATE -fvisibility=hidden)
    target_link_options(gigaedge-spench PRIVATE "-Wl,--exclude-libs=ALL")
    target_compile_definitions(gigaedge-spench PUBLIC GES_DLL=1 GES_DLL_EXPORTS=1)
  endif()
endif()
