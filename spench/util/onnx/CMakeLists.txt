cmake_minimum_required(VERSION 3.17.3)

add_library(util__onnx STATIC
        onnx_env.cpp
        base_onnx_runner.cpp
        denoise_onnx_runner.cpp
        dereverb_onnx_runner.cpp
)

set_property(TARGET util__onnx PROPERTY CXX_STANDARD 14)
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

target_link_libraries(util__onnx PRIVATE yaml-cpp spdlog_header_only)

target_link_libraries(util__onnx PUBLIC yaml-cpp)

add_library(util::onnx ALIAS util__onnx)
