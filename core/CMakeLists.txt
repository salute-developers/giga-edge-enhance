set(GEE_DEPS
  util__echo
  util__packed_model
  util::sound
  yaml-cpp
  spdlog_header_only
  dsplib
)

gee_relax_ios_third_party_warnings()

if(BUILD_RESAMPLER)
  if(EMSCRIPTEN)
    set(WITH_LSR_BINDINGS OFF)
  endif()
  set(_gee_build_shared_libs "${BUILD_SHARED_LIBS}")
  CPMGetPackage(soxr)
  # Clang -O1 and above resample differently from Debug and from the GCC Release
  # that produced the golden waveforms. -O0 is last so it overrides Release -O3
  # for this target only.
  if(TARGET soxr AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(soxr PRIVATE -O0)
  endif()
  set(BUILD_SHARED_LIBS "${_gee_build_shared_libs}" CACHE BOOL "Build all libraries dynamically." FORCE)
endif()

add_subdirectory(util EXCLUDE_FROM_ALL)

set(GEE_SOURCES
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
  list(APPEND GEE_SOURCES
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
  list(APPEND GEE_DEPS pocketfft)
endif()

if(BUILD_TFLITE_ENGINE)
  list(APPEND GEE_DEPS util__tf_lite)
elseif(BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND GEE_DEPS util__onnx)
endif()

if(BUILD_RESAMPLER)
  list(APPEND GEE_DEPS soxr)
endif()

if(GEE_SHARED_LIBS)
  add_library(giga-edge-enhance SHARED ${GEE_SOURCES})
else()
  add_library(giga-edge-enhance STATIC ${GEE_SOURCES})
endif()

if(APPLE)
  set_target_properties(giga-edge-enhance PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(giga-edge-enhance PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

add_library(giga-edge::enhance ALIAS giga-edge-enhance)

target_include_directories(giga-edge-enhance
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
  target_link_libraries(giga-edge-enhance PRIVATE gee_build_config ${GEE_DEPS})
else()
  target_link_libraries(giga-edge-enhance PRIVATE $<BUILD_INTERFACE:gee_build_config>)
  foreach(_gee_dep IN LISTS GEE_DEPS)
    target_link_libraries(giga-edge-enhance PRIVATE $<BUILD_INTERFACE:${_gee_dep}>)
  endforeach()
endif()

if(ANDROID)
  message(STATUS "Applying 16KB page alignment for Android (giga-edge-enhance)")
  target_link_options(giga-edge-enhance PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(GEE_SHARED_LIBS)
  if(BUILD_TESTING)
    message(WARNING "Disabling -fvisibility=hidden for shared library build with tests")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(giga-edge-enhance PRIVATE -fvisibility=hidden)
    target_link_options(giga-edge-enhance PRIVATE "-Wl,--exclude-libs=ALL")
    target_compile_definitions(giga-edge-enhance PUBLIC GEE_DLL=1 GEE_DLL_EXPORTS=1)
  endif()
endif()
