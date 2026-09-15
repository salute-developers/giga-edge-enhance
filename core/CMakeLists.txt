set(GDN_DEPS
  util__echo
  util::sound
  yaml-cpp
  spdlog_header_only
  dsplib
)

gdn_relax_ios_third_party_warnings()

if(BUILD_RESAMPLER)
  if(EMSCRIPTEN)
    set(WITH_LSR_BINDINGS OFF)
  endif()
  set(_gdn_build_shared_libs "${BUILD_SHARED_LIBS}")
  CPMGetPackage(soxr)
  set(BUILD_SHARED_LIBS "${_gdn_build_shared_libs}" CACHE BOOL "Build all libraries dynamically." FORCE)
endif()

add_subdirectory(util EXCLUDE_FROM_ALL)

set(GDN_SOURCES
  src/config_reader.cpp
  src/handler/base_audio_handler.cpp
  src/handler/acoustic_echo_cancelation_handler.cpp
  src/handler/audio_processor_handler.cpp
  src/audio_processor.cpp
  src/echo_canceller.cpp
  src/logger.cpp
  src/task.cpp
  src/feature_params.cpp
)

if(BUILD_TFLITE_ENGINE OR BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND GDN_SOURCES
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
  list(APPEND GDN_DEPS util__packed_model ctpl pocketfft)
endif()

if(BUILD_TFLITE_ENGINE)
  list(APPEND GDN_DEPS util__tf_lite)
elseif(BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND GDN_DEPS util__onnx)
endif()

if(BUILD_RESAMPLER)
  list(APPEND GDN_DEPS soxr)
endif()

if(GDN_SHARED_LIBS)
  add_library(giga-denoiser SHARED ${GDN_SOURCES})
else()
  add_library(giga-denoiser STATIC ${GDN_SOURCES})
endif()

if(APPLE)
  set_target_properties(giga-denoiser PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(giga-denoiser PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

add_library(giga-denoiser::giga-denoiser ALIAS giga-denoiser)

target_include_directories(giga-denoiser
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
  target_link_libraries(giga-denoiser PRIVATE gdn_build_config ${GDN_DEPS})
else()
  target_link_libraries(giga-denoiser PRIVATE $<BUILD_INTERFACE:gdn_build_config>)
  foreach(_gdn_dep IN LISTS GDN_DEPS)
    target_link_libraries(giga-denoiser PRIVATE $<BUILD_INTERFACE:${_gdn_dep}>)
  endforeach()
endif()

if(ANDROID)
  message(STATUS "Applying 16KB page alignment for Android (giga-denoiser)")
  target_link_options(giga-denoiser PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(GDN_SHARED_LIBS)
  if(BUILD_TESTING)
    message(WARNING "Disabling -fvisibility=hidden for shared library build with tests")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(giga-denoiser PRIVATE -fvisibility=hidden)
    target_link_options(giga-denoiser PRIVATE "-Wl,--exclude-libs=ALL")
    target_compile_definitions(giga-denoiser PUBLIC GDN_DLL=1 GDN_DLL_EXPORTS=1)
  endif()
endif()
