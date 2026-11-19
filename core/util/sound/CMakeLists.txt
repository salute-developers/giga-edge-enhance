project(util__sound)

add_library(${PROJECT_NAME} STATIC
    sound_stream.cpp
    range_control.cpp
    loudness_meter.cpp
    loudness_control.cpp
    rnnoise.cpp
    common.cpp
    rnnoise_vad.cpp
    voice_activity.cpp
    resampler.cpp
)

gee_apply_build_config(${PROJECT_NAME})

if (NOT WIN32)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wno-sign-compare)
endif()


target_include_directories(${PROJECT_NAME} PRIVATE ${soxr_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog_header_only dsplib rnnoise soxr)

add_library(util::sound ALIAS ${PROJECT_NAME})
