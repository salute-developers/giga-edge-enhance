find_package(Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 3.0.1 CONFIG REQUIRED)
if(GEE_SHARED_LIBS)
  message(FATAL_ERROR "Python components require an isolated static core")
endif()
if(BUILD_DSP)
  set(module _dsp)
  set(binding_source dsp.cpp)
elseif(BUILD_TFLITE_ENGINE)
  set(module _tflite)
  set(binding_source denoiser.cpp)
else()
  set(module _onnx)
  set(binding_source denoiser.cpp)
endif()
pybind11_add_module(${module} MODULE ${binding_source})
target_link_libraries(${module} PRIVATE giga-edge-enhance gee_build_config yaml-cpp util::sound spdlog_header_only)
if(BUILD_DSP)
  target_link_libraries(${module} PRIVATE util__metrics util__echo dsplib)
endif()
target_compile_features(${module} PRIVATE cxx_std_17)
set_target_properties(${module} PROPERTIES
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(UNIX AND NOT APPLE)
  target_link_options(${module} PRIVATE "-Wl,--exclude-libs=ALL")
endif()
if(APPLE)
  target_link_options(${module} PRIVATE "LINKER:-exported_symbol,_PyInit_${module}")
  target_link_libraries(${module} PRIVATE "-framework Foundation" "-framework CoreFoundation")
  set_target_properties(${module} PROPERTIES
    BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "@loader_path/lib")
elseif(UNIX)
  set_target_properties(${module} PROPERTIES
    BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "$ORIGIN/lib")
endif()
install(TARGETS ${module} LIBRARY DESTINATION giga_edge_enhance COMPONENT python
  RUNTIME DESTINATION giga_edge_enhance COMPONENT python)
foreach(dependency soxr onnxruntime onnxruntime::onnxruntime tensorflow-lite tensorflow-lite::tensorflow-lite)
  if(TARGET ${dependency})
    get_target_property(dependency_type ${dependency} TYPE)
    if(dependency_type STREQUAL "SHARED_LIBRARY")
      install(FILES "$<TARGET_FILE:${dependency}>" DESTINATION giga_edge_enhance/lib COMPONENT python)
      if(UNIX)
        # An imported runtime can leave its SONAME unset. In that case
        # TARGET_SONAME_FILE resolves to its parent directory.
        set(soname "$<TARGET_SONAME_FILE_NAME:${dependency}>")
        install(FILES "$<$<BOOL:${soname}>:$<TARGET_SONAME_FILE:${dependency}>>"
          DESTINATION giga_edge_enhance/lib COMPONENT python)
      endif()
    endif()
  endif()
endforeach()

if(GEE_PYTHON_BUILD_TEST_REFERENCE AND NOT BUILD_DSP)
  add_executable(gee_python_reference ../tests/reference.cpp)
  target_link_libraries(gee_python_reference PRIVATE giga-edge-enhance gee_build_config yaml-cpp)
  if(APPLE)
    target_link_libraries(gee_python_reference PRIVATE "-framework Foundation" "-framework CoreFoundation")
  endif()
  set_target_properties(gee_python_reference PROPERTIES
    OUTPUT_NAME enhance-python-reference
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python-reference"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/python-reference")
  if(WIN32)
    add_custom_command(TARGET gee_python_reference POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:gee_python_reference>
        $<TARGET_FILE_DIR:gee_python_reference> COMMAND_EXPAND_LISTS)
  endif()
endif()

if(GEE_PYTHON_BUILD_TEST_REFERENCE AND BUILD_DSP)
  add_executable(gee_python_components_reference ../tests/reference_components.cpp)
  target_link_libraries(gee_python_components_reference PRIVATE giga-edge-enhance gee_build_config yaml-cpp)
  if(APPLE)
    target_link_libraries(gee_python_components_reference PRIVATE "-framework Foundation" "-framework CoreFoundation")
  endif()
  set_target_properties(gee_python_components_reference PROPERTIES
    OUTPUT_NAME enhance-python-components
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python-reference"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/python-reference")
endif()

if(GEE_PYTHON_BUILD_TEST_REFERENCE AND BUILD_WITH_ADDRESS_SANITIZER AND BUILD_ONNX_ENGINE)
  find_package(Python 3.10 COMPONENTS Interpreter Development.Embed REQUIRED)
  add_executable(gee_python_test_interpreter ../tests/sanitized_python.cpp)
  target_link_libraries(gee_python_test_interpreter PRIVATE Python::Python gee_build_config)
  set_target_properties(gee_python_test_interpreter PROPERTIES
    OUTPUT_NAME enhance-python-sanitized
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python-reference"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/python-reference")
endif()
