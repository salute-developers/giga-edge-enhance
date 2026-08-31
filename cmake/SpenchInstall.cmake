if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_LIST_DIR}/InstallTools.cmake")

set(_spench_config_build_onnx "${BUILD_ONNX_ENGINE}")
set(_spench_config_build_tflite "${BUILD_TFLITE_ENGINE}")
ConfigInstallTarget(spench)

install(
  FILES "${CMAKE_SOURCE_DIR}/spench/include/spench.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT spench_dev
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/spench/include/spench"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT spench_dev
  FILES_MATCHING PATTERN "*.h"
)

if(TARGET spench_tests)
  install(
    TARGETS spench_tests
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT spench_tools
  )
endif()

if(TARGET spench_demo)
  install(
    TARGETS spench_demo demo_cpp demo_c demo_dsp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT spench_demo
  )
endif()

if(TARGET model_packer)
  install(
    TARGETS model_packer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT spench_tools
  )
endif()

include(CPack)
set(CPACK_PACKAGE_NAME spench)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
