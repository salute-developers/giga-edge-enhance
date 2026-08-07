# CPM Package Lock
# This file should be committed to version control

include(cmake/string_option.cmake)

# base dependencies

string_option(DSPLIB_URL "https://github.com/vitalsong/dsplib.git")
CPMDeclarePackage(dsplib
  NAME dsplib
  VERSION 0.55.3
  GIT_REPOSITORY ${DSPLIB_URL}
  OPTIONS
    "DSPLIB_USE_FLOAT32 ON"
    "BUILD_SHARED_LIBS OFF" # use only static linking for deps
  EXCLUDE_FROM_ALL ON
)

string_option(RNNOISE_URL "https://github.com/xiph/rnnoise.git")
CPMDeclarePackage(rnnoise
  NAME rnnoise
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/rnnoise.patch"
  GIT_TAG 70f1d256acd4b34a572f999a05c87bf00b67730d
  GIT_REPOSITORY ${RNNOISE_URL}
  OPTIONS
    "RNNOISE_X86_SIMD OFF" #TODO(vayulis): enable for x86 desktop
    "BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

string_option(SPEEXDSP_URL "https://github.com/xiph/speexdsp.git")
CPMDeclarePackage(speexdsp_spench
  NAME speexdsp_spench
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/speex.patch"
  GIT_TAG "SpeexDSP-1.2.1"
  GIT_REPOSITORY ${SPEEXDSP_URL}
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

string_option(YAMLCPP_URL "https://github.com/jbeder/yaml-cpp.git")
CPMDeclarePackage(yaml-cpp
  NAME yaml-cpp
  GIT_TAG 0.8.0
  GIT_REPOSITORY ${YAMLCPP_URL}
  OPTIONS
    "YAML_BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

# dev dependencies

string_option(ARGPARSE_URL "https://github.com/p-ranav/argparse")
CPMDeclarePackage(argparse
  NAME argparse
  GIT_TAG v3.0
  GIT_REPOSITORY ${ARGPARSE_URL}
  EXCLUDE_FROM_ALL ON
)

string_option(FMT_URL "https://github.com/fmtlib/fmt.git")
CPMDeclarePackage(fmt
  NAME fmt
  GIT_TAG 9.0.0
  GIT_REPOSITORY ${FMT_URL}
  EXCLUDE_FROM_ALL ON
)

string_option(SPDLOG_URL "https://github.com/gabime/spdlog.git")
CPMDeclarePackage(spdlog
  NAME spdlog
  GIT_TAG v1.15.2
  GIT_REPOSITORY ${SPDLOG_URL}
  EXCLUDE_FROM_ALL ON
)

string_option(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime.git")
CPMDeclarePackage(onnxruntime
  NAME onnxruntime
  GIT_TAG v1.20.1
  GIT_REPOSITORY ${ONNXRUNTIME_URL}
  SOURCE_SUBDIR cmake
  EXCLUDE_FROM_ALL OFF
)

string_option(TENSORFLOW_URL "https://github.com/microsoft/onnxruntime.git")
CPMDeclarePackage(tflite
  NAME tflite
  GIT_TAG v2.13.0
  GIT_REPOSITORY ${TENSORFLOW_URL}
  SOURCE_SUBDIR tensorflow/lite
  EXCLUDE_FROM_ALL ON
)

string_option(SOXR_URL "https://github.com/chirlu/soxr.git")
CPMDeclarePackage(soxr
  NAME soxr
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/soxr.patch"
  GIT_TAG 0.1.3
  GIT_REPOSITORY ${SOXR_URL}
  EXCLUDE_FROM_ALL ON
)
