cmake_minimum_required(VERSION 3.17.3)

if(NOT TARGET tensorflow-lite AND NOT TARGET tensorflow-lite::tensorflow-lite)
  CPMGetPackage(tflite)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT GES_USING_PREBUILT_TFLITE)
    # TensorFlow Lite 2.13's spectrogram source omits the uint32_t declaration.
    target_compile_options(tensorflow-lite PRIVATE -include cstdint)
endif()

add_library(util__tf_lite STATIC
        tflite_runner.cpp
)

ges_apply_build_config(util__tf_lite)

set_property(TARGET util__tf_lite PROPERTY CXX_STANDARD 17)
if (NOT WIN32)
target_compile_options(util__tf_lite
          PRIVATE -Wall
          PRIVATE -Wextra
          PRIVATE -Wshadow
          # PRIVATE -pedantic
          PRIVATE -Werror
          PRIVATE -Wno-error=shadow
          PRIVATE -Wno-error=unused-parameter
          PRIVATE -Wno-error=comment
          PRIVATE -Wno-noexcept-type
          PRIVATE -O3)
endif()

if(TARGET tensorflow-lite::tensorflow-lite)
  target_link_libraries(util__tf_lite PUBLIC tensorflow-lite::tensorflow-lite yaml-cpp)
else()
  target_link_libraries(util__tf_lite PUBLIC tensorflow-lite yaml-cpp)
endif()

if(GES_USING_PREBUILT_TFLITE)
  target_link_directories(util__tf_lite PUBLIC
    "${GES_RESOLVED_PREBUILT_TFLITE_ROOT}/lib")
endif()

add_library(util::tf_lite ALIAS util__tf_lite)
