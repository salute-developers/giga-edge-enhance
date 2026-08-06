cmake_minimum_required(VERSION 3.17.3)

include_directories(${CMAKE_SOURCE_DIR}/spench/contrib/tflite)
include_directories(${CMAKE_SOURCE_DIR}/spench/contrib/tflite/flatbuffers/include)

add_library(util__tf_lite STATIC
        tflite_runner.cpp
)

set_property(TARGET util__tf_lite PROPERTY CXX_STANDARD 17)
if (NOT WIN32)
target_compile_options(util__tf_lite
          PRIVATE -Wall
          PRIVATE -Wextra
          PRIVATE -Wshadow
          # PRIVATE -pedantic
          PRIVATE -Werror
          PRIVATE -Wno-noexcept-type
          PRIVATE -O3)
endif()

target_link_libraries(util__tf_lite
    PUBLIC
        tensorflow-lite
        yaml-cpp
)

add_library(util::tf_lite ALIAS util__tf_lite)
