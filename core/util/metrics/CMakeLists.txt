cmake_minimum_required(VERSION 3.17.3)

add_library(util__metrics STATIC
        mute_frame_detector.cpp
)

ges_apply_build_config(util__metrics)

set_property(TARGET util__metrics PROPERTY CXX_STANDARD 17)
if (NOT WIN32)
target_compile_options(util__metrics
          PRIVATE -Wall
          PRIVATE -Wextra
          PRIVATE -Werror
          PRIVATE -Wno-noexcept-type
          PRIVATE -O3)
endif()

target_link_libraries(util__metrics PRIVATE dsplib)

add_library(util::metrics ALIAS util__metrics)
