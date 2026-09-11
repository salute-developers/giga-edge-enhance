if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_LIST_DIR}/InstallTools.cmake")

if(SPENCH_SHARED_LIBS)
  set(_spench_config_static OFF)
else()
  set(_spench_config_static ON)
endif()
ConfigInstallTarget(spench)

if(NOT SPENCH_SHARED_LIBS)
  # Preserve subdirectories because independent dependencies can produce archives
  # with the same filename. Discover them at install time so only built archives
  # are copied and no dependency build-tree directories leak into the package.
  install(CODE "
    cmake_policy(SET CMP0009 NEW)
    file(GLOB_RECURSE _spench_archives LIST_DIRECTORIES false
      \"${CMAKE_BINARY_DIR}/*${CMAKE_STATIC_LIBRARY_SUFFIX}\")
    foreach(_spench_archive IN LISTS _spench_archives)
      file(RELATIVE_PATH _spench_archive_relative \"${CMAKE_BINARY_DIR}\" \"\${_spench_archive}\")
      if(_spench_archive_relative STREQUAL \"spench/${CMAKE_STATIC_LIBRARY_PREFIX}spench${CMAKE_STATIC_LIBRARY_SUFFIX}\")
        continue()
      endif()
      get_filename_component(_spench_archive_directory \"\${_spench_archive_relative}\" DIRECTORY)
      file(INSTALL
        DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/spench/deps/\${_spench_archive_directory}\"
        TYPE FILE
        FILES \"\${_spench_archive}\")
    endforeach()
  " COMPONENT spench_runtime)
endif()

if(BUILD_ONNX_ENGINE AND SPENCH_PREBUILT_ORT_ROOT)
  install(DIRECTORY "${SPENCH_PREBUILT_ORT_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(DIRECTORY "${SPENCH_PREBUILT_ORT_ROOT}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
elseif(BUILD_TFLITE_ENGINE AND SPENCH_RESOLVED_PREBUILT_TFLITE_ROOT)
  install(DIRECTORY "${SPENCH_RESOLVED_PREBUILT_TFLITE_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(DIRECTORY "${SPENCH_RESOLVED_PREBUILT_TFLITE_ROOT}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

if(SPENCH_SHARED_LIBS)
  set(_spench_runtime_dependencies soxr)
  if(BUILD_ONNX_ENGINE)
    if(TARGET onnxruntime::onnxruntime)
      list(APPEND _spench_runtime_dependencies onnxruntime::onnxruntime)
    elseif(TARGET onnxruntime)
      list(APPEND _spench_runtime_dependencies onnxruntime)
    endif()
  elseif(BUILD_TFLITE_ENGINE)
    if(TARGET tensorflow-lite::tensorflow-lite)
      list(APPEND _spench_runtime_dependencies tensorflow-lite::tensorflow-lite)
    elseif(TARGET tensorflow-lite)
      list(APPEND _spench_runtime_dependencies tensorflow-lite)
    endif()
  endif()

  foreach(_spench_dependency IN LISTS _spench_runtime_dependencies)
    if(TARGET ${_spench_dependency})
      get_target_property(_spench_dependency_type ${_spench_dependency} TYPE)
      if(_spench_dependency_type STREQUAL "SHARED_LIBRARY" OR _spench_dependency_type STREQUAL "MODULE_LIBRARY")
        if(WIN32)
          set(_spench_runtime_destination "${CMAKE_INSTALL_BINDIR}")
        else()
          set(_spench_runtime_destination "${CMAKE_INSTALL_LIBDIR}")
        endif()
        install(FILES "$<TARGET_FILE:${_spench_dependency}>" DESTINATION "${_spench_runtime_destination}")
      endif()
    endif()
  endforeach()
endif()

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
