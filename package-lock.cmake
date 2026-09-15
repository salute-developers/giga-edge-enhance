# CPM Package Lock
# This file should be committed to version control

function(string_option name value)
  if(NOT DEFINED ${name})
    set(${name} ${value} PARENT_SCOPE)
  endif()
endfunction()

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
CPMDeclarePackage(speexdsp_gdn_core
  NAME speexdsp_gdn_core
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

string_option(CTPL_URL "https://github.com/vit-vit/CTPL.git")
CPMDeclarePackage(ctpl
  NAME ctpl
  VERSION 0.0.2
  GIT_TAG v.0.0.2
  GIT_REPOSITORY ${CTPL_URL}
  DOWNLOAD_ONLY YES
)

string_option(POCKETFFT_URL "https://github.com/mreineck/pocketfft.git")
CPMDeclarePackage(pocketfft
  NAME pocketfft
  GIT_TAG c90e55b3d529f8efa40ed01a20de22405f45fc65
  GIT_REPOSITORY ${POCKETFFT_URL}
  DOWNLOAD_ONLY YES
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
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/onnxruntime.patch"
  ONNXRUNTIME_PATCH_SHA256)
CPMDeclarePackage(onnxruntime
  NAME onnxruntime
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/onnxruntime.patch"
  GIT_TAG v1.20.1
  GIT_REPOSITORY ${ONNXRUNTIME_URL}
  CUSTOM_CACHE_KEY "v1.20.1-${ONNXRUNTIME_PATCH_SHA256}"
  SOURCE_SUBDIR cmake
  EXCLUDE_FROM_ALL OFF
)

string_option(TENSORFLOW_URL "https://github.com/tensorflow/tensorflow.git")
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/tflite.patch"
  TFLITE_PATCH_SHA256)
CPMDeclarePackage(tflite
  NAME tflite
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/tflite.patch"
  GIT_TAG v2.13.0
  GIT_REPOSITORY ${TENSORFLOW_URL}
  CUSTOM_CACHE_KEY "v2.13.0-${TFLITE_PATCH_SHA256}"
  SOURCE_SUBDIR tensorflow/lite
  EXCLUDE_FROM_ALL ON
)

string_option(SOXR_URL "https://github.com/chirlu/soxr.git")
CPMDeclarePackage(soxr
  NAME soxr
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/soxr.patch"
  GIT_TAG 0.1.3
  GIT_REPOSITORY ${SOXR_URL}
  OPTIONS
    "WITH_OPENMP OFF"
    "BUILD_TESTS OFF"
    "BUILD_SHARED_LIBS OFF"
  EXCLUDE_FROM_ALL ON
)

string_option(CATCH2_URL "https://github.com/catchorg/Catch2.git")
CPMDeclarePackage(Catch2
  NAME Catch2
  VERSION 2.13.10
  GIT_TAG v2.13.10
  GIT_REPOSITORY ${CATCH2_URL}
  OPTIONS
    "CATCH_BUILD_TESTING OFF"
    "CATCH_INSTALL_DOCS OFF"
    "CATCH_INSTALL_HELPERS OFF"
  EXCLUDE_FROM_ALL ON
)
