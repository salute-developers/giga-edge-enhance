cmake_minimum_required(VERSION 3.17.3)

set(TEST_SRC
    src/test_dsplib_features_and_metrics.cpp
    src/test_fe.cpp
    src/test_fe_runner.cpp
    src/test_hybrid.cpp
    src/test_cpp_api.cpp
    src/test_c_api.cpp
    src/test_config_reader.cpp
    src/test_pipeline_stage.cpp
    src/test_packed_model.cpp
    src/test_audio_handler.cpp
    src/test_audio_processing.cpp
    src/test_voice_utils.cpp
    src/test_voice_activity_detection.cpp
    src/test_loudness_control.cpp
    src/test_logger.cpp
    src/utils.cpp
    src/main.cpp)

if (BUILD_TFLITE_ENGINE)
    set(TEST_SRC
       src/test_tflite_infer.cpp
       ${TEST_SRC}
       )
elseif (BUILD_ONNX_ENGINE)
    set(TEST_SRC
        src/test_onnx_infer.cpp
        ${TEST_SRC}
        )
endif()

add_executable(enhance_tests ${TEST_SRC})
target_include_directories(enhance_tests SYSTEM PRIVATE
  "${Catch2_SOURCE_DIR}/single_include/catch2")

gee_apply_build_config(enhance_tests)

target_link_libraries(enhance_tests PRIVATE
  gigaedge-enhance
  util::packed_model
  yaml-cpp
  util::sound
  util__echo
  util__metrics
  dsplib
  spdlog_header_only
  Catch2::Catch2
  ctpl)

if(BUILD_RESAMPLER)
  target_link_libraries(enhance_tests PRIVATE soxr)
endif()

if(NOT (CMAKE_SYSTEM_NAME MATCHES "Android" OR WIN32))
  target_link_libraries(enhance_tests PRIVATE pthread)
endif()

if (BUILD_TFLITE_ENGINE)
  target_link_libraries(enhance_tests PRIVATE util__tf_lite)
elseif (BUILD_ONNX_ENGINE)
  target_link_libraries(enhance_tests PRIVATE util__onnx)

  if (UNIX AND NOT APPLE)
    target_link_libraries(enhance_tests PRIVATE gomp)
  endif()
endif()

include("${Catch2_SOURCE_DIR}/contrib/Catch.cmake")
catch_discover_tests(enhance_tests)

if (BUILD_SHARED_LIBS AND WIN32)
add_custom_command(TARGET enhance_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:enhance_tests> $<TARGET_FILE_DIR:enhance_tests>
  COMMAND_EXPAND_LISTS)
endif()

add_custom_command(TARGET enhance_tests PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/core/tests/data/ ${CMAKE_CURRENT_BINARY_DIR}/data)
