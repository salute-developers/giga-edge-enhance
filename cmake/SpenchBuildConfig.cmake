add_library(spench_build_config INTERFACE)
get_filename_component(_spench_root_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

target_include_directories(spench_build_config INTERFACE
  "$<BUILD_INTERFACE:${_spench_root_dir}>"
  "$<BUILD_INTERFACE:${_spench_root_dir}/spench>"
  "$<BUILD_INTERFACE:${_spench_root_dir}/spench/include>"
)

if(BUILD_TFLITE_ENGINE)
  target_compile_definitions(spench_build_config INTERFACE BUILD_TFLITE_ENGINE)
elseif(BUILD_ONNX_ENGINE)
  target_compile_definitions(spench_build_config INTERFACE BUILD_ONNX_ENGINE)
endif()

if(BUILD_RESAMPLER)
  target_compile_definitions(spench_build_config INTERFACE BUILD_RESAMPLER)
endif()

if(BUILD_WITH_ADDRESS_SANITIZER)
  target_compile_definitions(spench_build_config INTERFACE BUILD_WITH_ADDRESS_SANITIZER)
endif()

if(BUILD_DSP)
  target_compile_definitions(spench_build_config INTERFACE BUILD_DSP)
endif()

function(spench_apply_build_config target)
  target_link_libraries(${target} PRIVATE spench_build_config)
endfunction()
