# CPM Package Lock
# This file should be committed to version control

include(cmake/string_option.cmake)

# base dependencies

string_option(DSPLIB_URL "https://github.com/example/dsplib.git")
CPMDeclarePackage(dsplib
  NAME dsplib
  VERSION 0.55.3
  GIT_REPOSITORY ${DSPLIB_URL}
  OPTIONS
    "DSPLIB_USE_FLOAT32 ON"
    "BUILD_SHARED_LIBS OFF" # use only static linking for deps
  EXCLUDE_FROM_ALL ON
)

string_option(RNNOISE_URL "https://github.com/example/rnnoise.git")
CPMDeclarePackage(rnnoise
  NAME rnnoise
  GIT_TAG "sdvoice/0.2.0"
  GIT_REPOSITORY ${RNNOISE_URL}
  OPTIONS
    "RNNOISE_X86_SIMD OFF" #TODO(vayulis): enable for x86 desktop
    "BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

string_option(SPEEXDSP_URL "https://github.com/example/speexdsp.git")
CPMDeclarePackage(speexdsp_spench
  NAME speexdsp_spench
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/speex.patch"
  GIT_TAG "SpeexDSP-1.2.1"
  GIT_REPOSITORY ${SPEEXDSP_URL}
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

string_option(YAMLCPP_URL "https://github.com/example/yaml-cpp.git")
CPMDeclarePackage(yaml-cpp
  NAME yaml-cpp
  GIT_TAG 0.8.0
  GIT_REPOSITORY ${YAMLCPP_URL}
  OPTIONS
    "YAML_BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

# dev dependencies

string_option(ARGPARSE_URL "https://github.com/example/argparse.git")
CPMDeclarePackage(argparse
  NAME argparse
  GIT_TAG v3.0
  GIT_REPOSITORY ${ARGPARSE_URL}
  EXCLUDE_FROM_ALL ON
)

string_option(AUDIOSUP_URL "https://github.com/example/audiosup.git")
CPMDeclarePackage(audiosup
  NAME audiosup
  GIT_TAG v0.1.9
  GIT_REPOSITORY ${AUDIOSUP_URL}
  OPTIONS
    "AUDIOSUP_RWFILE_ENABLED ON"
    "AUDIOSUP_RWDEV_ENABLED OFF"
    "AUDIOSUP_STREAM_ENABLED OFF"
    "BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

string_option(FMT_URL "https://github.com/example/fmt.git")
CPMDeclarePackage(fmt
    NAME fmt
    GIT_REPOSITORY ${FMT_URL}
    GIT_TAG 9.0.0
    EXCLUDE_FROM_ALL ON
)

string_option(SPDLOG_URL "https://github.com/example/spdlog.git")
CPMDeclarePackage(spdlog
  NAME spdlog
  GIT_TAG v1.15.2
  GIT_REPOSITORY ${SPDLOG_URL}
  EXCLUDE_FROM_ALL ON
)
