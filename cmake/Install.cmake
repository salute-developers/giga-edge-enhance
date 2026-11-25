if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ANDROID)
  return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_LIST_DIR}/InstallTools.cmake")

if(GEE_SHARED_LIBS)
  set(_gee_config_static OFF)
else()
  set(_gee_config_static ON)
endif()
ConfigInstallTarget(giga-edge-enhance)

if(NOT GEE_SHARED_LIBS)
  # Preserve subdirectories because independent dependencies can produce archives
  # with the same filename. Discover them at install time so only built archives
  # are copied and no dependency build-tree directories leak into the package.
  install(CODE "
    cmake_policy(SET CMP0009 NEW)
    file(GLOB_RECURSE _gee_archives LIST_DIRECTORIES false
      \"${CMAKE_BINARY_DIR}/*${CMAKE_STATIC_LIBRARY_SUFFIX}\")
    foreach(_gee_archive IN LISTS _gee_archives)
      file(RELATIVE_PATH _gee_archive_relative \"${CMAKE_BINARY_DIR}\" \"\${_gee_archive}\")
      if(_gee_archive_relative STREQUAL \"core/${CMAKE_STATIC_LIBRARY_PREFIX}giga-edge-enhance${CMAKE_STATIC_LIBRARY_SUFFIX}\")
        continue()
      endif()
      get_filename_component(_gee_archive_directory \"\${_gee_archive_relative}\" DIRECTORY)
      file(INSTALL
        DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/enhance/deps/\${_gee_archive_directory}\"
        TYPE FILE
        FILES \"\${_gee_archive}\")
    endforeach()
  " COMPONENT gee_runtime)
endif()

if(BUILD_ONNX_ENGINE AND GEE_PREBUILT_ORT_ROOT)
  install(DIRECTORY "${GEE_PREBUILT_ORT_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(DIRECTORY "${GEE_PREBUILT_ORT_ROOT}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
elseif(BUILD_TFLITE_ENGINE AND GEE_RESOLVED_PREBUILT_TFLITE_ROOT)
  install(DIRECTORY "${GEE_RESOLVED_PREBUILT_TFLITE_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(DIRECTORY "${GEE_RESOLVED_PREBUILT_TFLITE_ROOT}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

if(GEE_SHARED_LIBS)
  set(_gee_runtime_dependencies soxr)
  if(BUILD_ONNX_ENGINE)
    if(TARGET onnxruntime::onnxruntime)
      list(APPEND _gee_runtime_dependencies onnxruntime::onnxruntime)
    elseif(TARGET onnxruntime)
      list(APPEND _gee_runtime_dependencies onnxruntime)
    endif()
  elseif(BUILD_TFLITE_ENGINE)
    if(TARGET tensorflow-lite::tensorflow-lite)
      list(APPEND _gee_runtime_dependencies tensorflow-lite::tensorflow-lite)
    elseif(TARGET tensorflow-lite)
      list(APPEND _gee_runtime_dependencies tensorflow-lite)
    endif()
  endif()

  foreach(_gee_dependency IN LISTS _gee_runtime_dependencies)
    if(TARGET ${_gee_dependency})
      get_target_property(_gee_dependency_type ${_gee_dependency} TYPE)
      if(_gee_dependency_type STREQUAL "SHARED_LIBRARY" OR _gee_dependency_type STREQUAL "MODULE_LIBRARY")
        if(WIN32)
          set(_gee_runtime_destination "${CMAKE_INSTALL_BINDIR}")
        else()
          set(_gee_runtime_destination "${CMAKE_INSTALL_LIBDIR}")
        endif()
        install(FILES "$<TARGET_FILE:${_gee_dependency}>" DESTINATION "${_gee_runtime_destination}")
      endif()
    endif()
  endforeach()
endif()

install(
  FILES "${CMAKE_SOURCE_DIR}/core/include/giga-edge-enhance.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT gee_dev
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/core/include/enhance"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT gee_dev
  FILES_MATCHING PATTERN "*.h"
)

if(TARGET gee_demo)
  set(_gee_cli_install_targets gee_demo)
  foreach(_gee_legacy_demo demo_cpp demo_c demo_dsp)
    if(TARGET ${_gee_legacy_demo})
      list(APPEND _gee_cli_install_targets ${_gee_legacy_demo})
    endif()
  endforeach()
  install(
    TARGETS ${_gee_cli_install_targets}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT gee_demo
  )
endif()

if(TARGET model_packer)
  install(
    TARGETS model_packer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT gee_tools
  )
endif()

# Ship the project license next to the binaries so an installed or packaged copy carries
# its terms. Third-party dependency notices are tracked separately and are still required
# before publishing binaries that statically link non-MIT dependencies.
install(
  FILES "${CMAKE_SOURCE_DIR}/LICENSE.txt"
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/enhance"
)

set(CPACK_PACKAGE_NAME giga-edge-enhance)
set(_gee_default_artifact_version "${PROJECT_VERSION}")
if(DEFINED ENV{GEE_ARTIFACT_VERSION})
  set(_gee_default_artifact_version "$ENV{GEE_ARTIFACT_VERSION}")
endif()
set(GEE_ARTIFACT_VERSION "${_gee_default_artifact_version}" CACHE STRING
    "Version used in distributable artifact names; may include a SemVer prerelease suffix." FORCE)
set(CPACK_PACKAGE_VERSION ${GEE_ARTIFACT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

# CPack captures the CPACK_* variables when it is included, so this must stay last. Including
# it earlier silently discarded the GEE_ARTIFACT_VERSION override above.
include(CPack)
