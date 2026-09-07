project(spench_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DEMO_LIBS spench yaml-cpp util::sound)

# legacy demo app
set(DEMO_SRC_CPP
    1-cpp_denoiser_api_demo.cpp
    utils.cpp
    )

set(DEMO_SRC_C
    2-c_denoiser_api_demo.c
    utils_c.c
    )

set(DEMO_DSP_SRC_CPP
    3-cpp_dsp_api_demo.cpp
    utils.cpp
    )

if(NOT WIN32)
  # These legacy examples use POSIX getopt and Unix file APIs. The supported
  # `spench` CLI below is the cross-platform command-line application.
  add_executable(demo_cpp ${DEMO_SRC_CPP})
  add_executable(demo_c ${DEMO_SRC_C})
  add_executable(demo_dsp ${DEMO_DSP_SRC_CPP})

  foreach(_demo_target demo_cpp demo_c demo_dsp)
    spench_apply_build_config(${_demo_target})
    target_include_directories(${_demo_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
  endforeach()
endif()

if(BUILD_RESAMPLER)
  list(APPEND DEMO_LIBS soxr)
endif()

if (BUILD_TFLITE_ENGINE)
  list(APPEND DEMO_LIBS util__tf_lite)
elseif (BUILD_ONNX_ENGINE)
  list(APPEND DEMO_LIBS util__onnx)
  if (UNIX AND NOT APPLE)
    list(APPEND DEMO_LIBS gomp)
  endif()
endif()

if(NOT WIN32)
  target_link_libraries(demo_cpp PRIVATE ${DEMO_LIBS})
  target_link_libraries(demo_c PRIVATE ${DEMO_LIBS})
  target_link_libraries(demo_dsp PRIVATE ${DEMO_LIBS})

  add_custom_command(TARGET demo_cpp PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/spench/tests/data/ ${CMAKE_CURRENT_BINARY_DIR}/data)
endif()

# new demo app

CPMGetPackage(argparse)
CPMGetPackage(fmt)

add_executable(${PROJECT_NAME} spench_cli.cpp utils.cpp)
spench_apply_build_config(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(${PROJECT_NAME} PRIVATE ${DEMO_LIBS} argparse dsplib fmt)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "spench")
