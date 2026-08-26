set(SPENCH_DEPS
  util__echo
  util::sound
  yaml-cpp
  spdlog_header_only
  dsplib
)

add_subdirectory(contrib)
spench_relax_ios_third_party_warnings()

if(NOT BUILD_WITHOUT_SOXR)
  if(EMSCRIPTEN)
    set(WITH_LSR_BINDINGS OFF)
  endif()
  set(_spench_build_shared_libs "${BUILD_SHARED_LIBS}")
  CPMGetPackage(soxr)
  set(BUILD_SHARED_LIBS "${_spench_build_shared_libs}" CACHE BOOL "Build all libraries dynamically." FORCE)
endif()

add_subdirectory(util EXCLUDE_FROM_ALL)

set(SPENCH_SOURCES
  src/spench_config_reader.cpp
  src/handler/base_audio_handler.cpp
  src/handler/acoustic_echo_cancelation_handler.cpp
  src/handler/audio_processor_handler.cpp
  src/spench_audio_process.cpp
  src/spench_audio_echo_cancelation.cpp
  src/spench_logger.cpp
  src/spench_task.cpp
  src/feature_params.cpp
)

if(BUILD_TFLITE_ENGINE OR BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND SPENCH_SOURCES
    src/denoiser_model_adviser.cpp
    src/handler/neural_network_audio_handler.cpp
    src/handler/denoiser_audio_handler.cpp
    src/handler/dereverberator_audio_handler.cpp
    src/windowing.cpp
    src/feature_extractor.cpp
    src/feature_extractor_runner.cpp
    src/ifft_calculator.cpp
    src/fft_calculator.cpp
    src/inference_runner.cpp
    src/spench_audio_denoise.cpp
    src/spench_audio_dereverb.cpp
    src/spench_c.cpp
    src/packed_model.cpp
  )
endif()

if(BUILD_TFLITE_ENGINE)
  list(APPEND SPENCH_DEPS util__tf_lite)
elseif(BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  list(APPEND SPENCH_DEPS util__onnx)
endif()

if(NOT BUILD_WITHOUT_SOXR)
  list(APPEND SPENCH_DEPS soxr)
endif()

if(SPENCH_SHARED_LIBS)
  add_library(spench SHARED ${SPENCH_SOURCES})
else()
  add_library(spench STATIC ${SPENCH_SOURCES})
endif()

add_library(spench::spench ALIAS spench)

target_include_directories(spench
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
  target_link_libraries(spench PRIVATE spench_build_config ${SPENCH_DEPS})
else()
  target_link_libraries(spench PRIVATE $<BUILD_INTERFACE:spench_build_config>)
  foreach(_spench_dep IN LISTS SPENCH_DEPS)
    target_link_libraries(spench PRIVATE $<BUILD_INTERFACE:${_spench_dep}>)
  endforeach()
endif()

if(ANDROID)
  message(STATUS "Applying 16KB page alignment for Android (spench)")
  target_link_options(spench PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(SPENCH_SHARED_LIBS)
  if(BUILD_TESTING)
    message(WARNING "Disabling -fvisibility=hidden for shared library build with tests")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(spench PRIVATE -fvisibility=hidden)
    target_link_options(spench PRIVATE "-Wl,--exclude-libs=ALL")
    target_compile_definitions(spench PUBLIC SPENCH_DLL=1 SPENCH_DLL_EXPORTS=1)
  endif()
endif()
