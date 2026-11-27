cmake_minimum_required(VERSION 3.25)
project(giga_edge_enhance_python LANGUAGES C CXX)

include(ExternalProject)
find_package(Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 3.0.1 CONFIG REQUIRED)
get_filename_component(GEE_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GEE_PYTHON_COMPONENTS "onnx;tflite;dsp" CACHE STRING "Components included in the wheel")
set(GEE_PYTHON_JOBS 2 CACHE STRING "Parallel jobs per native backend")
option(GEE_PYTHON_BUILD_TEST_REFERENCE "Build the independent C++ parity consumer" OFF)
option(GEE_PYTHON_SANITIZE "Instrument native adapters and core with AddressSanitizer" OFF)

# Multi-config generators select the backend configuration at build and install time.
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
  set(backend_config_args --config "$<CONFIG>")
endif()

foreach(backend IN LISTS GEE_PYTHON_COMPONENTS)
  if(NOT backend MATCHES "^(onnx|tflite|dsp)$")
    message(FATAL_ERROR "Unknown Python backend: ${backend}")
  endif()
  set(backend_args)
  set(backend_targets _${backend})
  if(GEE_PYTHON_BUILD_TEST_REFERENCE AND backend STREQUAL "dsp")
    list(APPEND backend_targets gee_python_components_reference)
  endif()
  if(GEE_PYTHON_BUILD_TEST_REFERENCE AND NOT backend STREQUAL "dsp")
    list(APPEND backend_targets gee_python_reference)
    if(GEE_PYTHON_SANITIZE AND backend STREQUAL "onnx")
      list(APPEND backend_targets gee_python_test_interpreter)
    endif()
  endif()
  if(backend STREQUAL "dsp")
    list(APPEND backend_args -DBUILD_DSP=ON -DBUILD_ONNX_ENGINE=OFF -DBUILD_TFLITE_ENGINE=OFF)
  elseif(backend STREQUAL "onnx")
    list(APPEND backend_args -DBUILD_DSP=OFF -DBUILD_ONNX_ENGINE=ON -DBUILD_TFLITE_ENGINE=OFF)
  else()
    list(APPEND backend_args -DBUILD_DSP=OFF -DBUILD_ONNX_ENGINE=OFF -DBUILD_TFLITE_ENGINE=ON)
  endif()
  foreach(setting GEE_PREBUILT_ORT_ROOT GEE_PREBUILT_TFLITE_ROOT)
    if(DEFINED ${setting})
      list(APPEND backend_args "-D${setting}=${${setting}}")
    endif()
  endforeach()
  foreach(setting CPM_SOURCE_CACHE
      CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
      CMAKE_TOOLCHAIN_FILE CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)
    if(DEFINED ${setting} AND NOT "${${setting}}" STREQUAL "")
      list(APPEND backend_args "-D${setting}=${${setting}}")
    endif()
  endforeach()
  ExternalProject_Add(python_${backend}
    SOURCE_DIR "${GEE_SOURCE_ROOT}"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${backend}"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS ${backend_args}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DCMAKE_C_VISIBILITY_PRESET=hidden
      -DCMAKE_CXX_VISIBILITY_PRESET=hidden
      -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON
      -DBUILD_SHARED_LIBS=OFF
      -DBUILD_TESTING=OFF
      -DBUILD_CLI=OFF
      -DBUILD_WITH_ADDRESS_SANITIZER=${GEE_PYTHON_SANITIZE}
      -DGEE_BUILD_PYTHON_BINDINGS=ON
      -DGEE_PYTHON_BUILD_TEST_REFERENCE=${GEE_PYTHON_BUILD_TEST_REFERENCE}
      -DPython_EXECUTABLE=${Python_EXECUTABLE}
      -Dpybind11_DIR=${pybind11_DIR}
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR>
      --target ${backend_targets} ${backend_config_args} --parallel ${GEE_PYTHON_JOBS}
    INSTALL_COMMAND "${CMAKE_COMMAND}" --install <BINARY_DIR>
      --prefix "${CMAKE_CURRENT_BINARY_DIR}/stage" --component python ${backend_config_args}
    BUILD_ALWAYS TRUE
  )
endforeach()
if(GEE_PYTHON_BUILD_TEST_REFERENCE)
  file(RELATIVE_PATH reference_root "${GEE_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}")
  execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "from workflow import interpreter_key; print(interpreter_key())"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
    OUTPUT_VARIABLE interpreter_key OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  file(GENERATE OUTPUT "${GEE_SOURCE_ROOT}/out/python/references/${interpreter_key}.txt"
    CONTENT "${reference_root}\n")
endif()
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage/giga_edge_enhance/"
  DESTINATION giga_edge_enhance COMPONENT python)
