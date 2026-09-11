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

if(TARGET spench_demo)
  set(_spench_cli_install_targets spench_demo)
  foreach(_spench_legacy_demo demo_cpp demo_c demo_dsp)
    if(TARGET ${_spench_legacy_demo})
      list(APPEND _spench_cli_install_targets ${_spench_legacy_demo})
    endif()
  endforeach()
  install(
    TARGETS ${_spench_cli_install_targets}
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

# Ship the project license next to the binaries so an installed or packaged copy carries
# its terms. Third-party dependency notices are tracked separately and are still required
# before publishing binaries that statically link non-MIT dependencies.
install(
  FILES "${CMAKE_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/spench"
)

set(CPACK_PACKAGE_NAME spench)
set(_spench_default_artifact_version "${PROJECT_VERSION}")
if(DEFINED ENV{SPENCH_ARTIFACT_VERSION})
  set(_spench_default_artifact_version "$ENV{SPENCH_ARTIFACT_VERSION}")
endif()
set(SPENCH_ARTIFACT_VERSION "${_spench_default_artifact_version}" CACHE STRING
    "Version used in distributable artifact names; may include a SemVer prerelease suffix.")
set(CPACK_PACKAGE_VERSION ${SPENCH_ARTIFACT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# CPack captures the CPACK_* variables when it is included, so this must stay last. Including
# it earlier silently discarded the SPENCH_ARTIFACT_VERSION override above.
include(CPack)
