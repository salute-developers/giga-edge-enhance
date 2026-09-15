cmake_minimum_required(VERSION 3.17.3)

add_library(util__onnx STATIC
        onnx_env.cpp
        base_onnx_runner.cpp
        denoise_onnx_runner.cpp
)

gdn_apply_build_config(util__onnx)

set_property(TARGET util__onnx PROPERTY CXX_STANDARD 17)
set_property(TARGET util__onnx PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
target_compile_options(util__onnx
          PRIVATE -Wall
          PRIVATE -Wextra
          PRIVATE -Werror
          PRIVATE -Wno-noexcept-type
          PRIVATE -O3)
endif()

if (EMSCRIPTEN)
  target_link_libraries(util__onnx
    PUBLIC onnxruntime_webassembly
  )
else()
  if (TARGET onnxruntime::onnxruntime)
    target_link_libraries(util__onnx
      PUBLIC onnxruntime::onnxruntime
    )
  else()
    target_link_libraries(util__onnx
      PUBLIC onnxruntime
    )
  endif()
endif()

if (EMSCRIPTEN)
  set(onnxruntime_target onnxruntime_webassembly)
else()
  set(onnxruntime_target onnxruntime)
endif()

if(GDN_PREBUILT_ORT_ROOT)
  target_include_directories(util__onnx PUBLIC "${GDN_PREBUILT_ORT_ROOT}/include")
  # CMake may reduce the imported dylib/archive to -lonnxruntime when this
  # static library is consumed by another target.  Keep its bundle directory
  # on the consumer link line in that case.
  target_link_directories(util__onnx PUBLIC "${GDN_PREBUILT_ORT_ROOT}/lib")
elseif (TARGET ${onnxruntime_target})
  get_target_property(onnxruntime_cmake_dir ${onnxruntime_target} SOURCE_DIR)
  target_include_directories(util__onnx PUBLIC "${onnxruntime_cmake_dir}/../include")
endif()

target_link_libraries(util__onnx PRIVATE yaml-cpp spdlog_header_only)

target_link_libraries(util__onnx PUBLIC yaml-cpp)

add_library(util::onnx ALIAS util__onnx)
