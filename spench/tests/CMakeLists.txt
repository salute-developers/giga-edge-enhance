cmake_minimum_required(VERSION 3.17.3)

set(TEST_SRC
    src/test_dsplib_features_and_metrics.cpp
    src/test_fe.cpp
    src/test_fe_runner.cpp
    src/test_cpp_api.cpp
    src/test_c_api.cpp
    src/test_config_reader.cpp
    src/test_packed_model.cpp
    src/test_audio_handler.cpp
    src/test_audio_processing.cpp
    src/test_voice_utils.cpp
    src/test_voice_activity_detection.cpp
    src/test_loudness_control.cpp
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

include_directories(${CMAKE_SOURCE_DIR}/spench/contrib/Catch2)

add_executable(spench_tests ${TEST_SRC})

target_link_libraries(spench_tests spench yaml-cpp util::sound util__echo util__metrics dsplib spdlog_header_only)

if (NOT BUILD_WITHOUT_SOXR)
  target_link_libraries(spench_tests soxr)
endif()

if(NOT (CMAKE_SYSTEM_NAME MATCHES "Android" OR WIN32))
  target_link_libraries(spench_tests pthread)
endif()

if (BUILD_TFLITE_ENGINE)
  target_link_libraries(spench_tests tensorflow-lite util__tf_lite)
elseif (BUILD_ONNX_ENGINE)
  target_link_libraries(spench_tests onnxruntime util__onnx)

  if (NOT APPLE)
    target_link_libraries(spench_tests gomp)
  endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/spench/contrib/Catch2/contrib/")
include(Catch)
catch_discover_tests(spench_tests)

if (BUILD_SHARED_LIBS AND WIN32)
add_custom_command(TARGET spench_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:spench_tests> $<TARGET_FILE_DIR:spench_tests>
  COMMAND_EXPAND_LISTS)
endif()

add_custom_command(TARGET spench_tests PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/spench/tests/data/ ${CMAKE_CURRENT_BINARY_DIR}/data)
