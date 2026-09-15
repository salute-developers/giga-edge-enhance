if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ANDROID)
  return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_LIST_DIR}/InstallTools.cmake")

if(GDN_SHARED_LIBS)
  set(_gdn_config_static OFF)
else()
  set(_gdn_config_static ON)
endif()
ConfigInstallTarget(giga-denoiser)

if(NOT GDN_SHARED_LIBS)
  # Preserve subdirectories because independent dependencies can produce archives
  # with the same filename. Discover them at install time so only built archives
  # are copied and no dependency build-tree directories leak into the package.
  install(CODE "
    cmake_policy(SET CMP0009 NEW)
    file(GLOB_RECURSE _gdn_archives LIST_DIRECTORIES false
      \"${CMAKE_BINARY_DIR}/*${CMAKE_STATIC_LIBRARY_SUFFIX}\")
    foreach(_gdn_archive IN LISTS _gdn_archives)
      file(RELATIVE_PATH _gdn_archive_relative \"${CMAKE_BINARY_DIR}\" \"\${_gdn_archive}\")
      if(_gdn_archive_relative STREQUAL \"core/${CMAKE_STATIC_LIBRARY_PREFIX}giga-denoiser${CMAKE_STATIC_LIBRARY_SUFFIX}\")
        continue()
      endif()
      get_filename_component(_gdn_archive_directory \"\${_gdn_archive_relative}\" DIRECTORY)
      file(INSTALL
        DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/gigadenoiser/deps/\${_gdn_archive_directory}\"
        TYPE FILE
        FILES \"\${_gdn_archive}\")
    endforeach()
  " COMPONENT gdn_runtime)
endif()

if(BUILD_ONNX_ENGINE AND GDN_PREBUILT_ORT_ROOT)
  install(DIRECTORY "${GDN_PREBUILT_ORT_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(DIRECTORY "${GDN_PREBUILT_ORT_ROOT}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
elseif(BUILD_TFLITE_ENGINE AND GDN_RESOLVED_PREBUILT_TFLITE_ROOT)
  install(DIRECTORY "${GDN_RESOLVED_PREBUILT_TFLITE_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(DIRECTORY "${GDN_RESOLVED_PREBUILT_TFLITE_ROOT}/lib/" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

if(GDN_SHARED_LIBS)
  set(_gdn_runtime_dependencies soxr)
  if(BUILD_ONNX_ENGINE)
    if(TARGET onnxruntime::onnxruntime)
      list(APPEND _gdn_runtime_dependencies onnxruntime::onnxruntime)
    elseif(TARGET onnxruntime)
      list(APPEND _gdn_runtime_dependencies onnxruntime)
    endif()
  elseif(BUILD_TFLITE_ENGINE)
    if(TARGET tensorflow-lite::tensorflow-lite)
      list(APPEND _gdn_runtime_dependencies tensorflow-lite::tensorflow-lite)
    elseif(TARGET tensorflow-lite)
      list(APPEND _gdn_runtime_dependencies tensorflow-lite)
    endif()
  endif()

  foreach(_gdn_dependency IN LISTS _gdn_runtime_dependencies)
    if(TARGET ${_gdn_dependency})
      get_target_property(_gdn_dependency_type ${_gdn_dependency} TYPE)
      if(_gdn_dependency_type STREQUAL "SHARED_LIBRARY" OR _gdn_dependency_type STREQUAL "MODULE_LIBRARY")
        if(WIN32)
          set(_gdn_runtime_destination "${CMAKE_INSTALL_BINDIR}")
        else()
          set(_gdn_runtime_destination "${CMAKE_INSTALL_LIBDIR}")
        endif()
        install(FILES "$<TARGET_FILE:${_gdn_dependency}>" DESTINATION "${_gdn_runtime_destination}")
      endif()
    endif()
  endforeach()
endif()

install(
  FILES "${CMAKE_SOURCE_DIR}/core/include/gigadenoiser.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT gdn_dev
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/core/include/gigadenoiser"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT gdn_dev
  FILES_MATCHING PATTERN "*.h"
)

if(TARGET gdn_demo)
  set(_gdn_cli_install_targets gdn_demo)
  foreach(_gdn_legacy_demo demo_cpp demo_c demo_dsp)
    if(TARGET ${_gdn_legacy_demo})
      list(APPEND _gdn_cli_install_targets ${_gdn_legacy_demo})
    endif()
  endforeach()
  install(
    TARGETS ${_gdn_cli_install_targets}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT gdn_demo
  )
endif()

if(TARGET model_packer)
  install(
    TARGETS model_packer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT gdn_tools
  )
endif()

# Ship the project license next to the binaries so an installed or packaged copy carries
# its terms. Third-party dependency notices are tracked separately and are still required
# before publishing binaries that statically link non-MIT dependencies.
install(
  FILES "${CMAKE_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/gigadenoiser"
)

set(CPACK_PACKAGE_NAME giga-denoiser)
set(_gdn_default_artifact_version "${PROJECT_VERSION}")
if(DEFINED ENV{GDN_ARTIFACT_VERSION})
  set(_gdn_default_artifact_version "$ENV{GDN_ARTIFACT_VERSION}")
endif()
set(GDN_ARTIFACT_VERSION "${_gdn_default_artifact_version}" CACHE STRING
    "Version used in distributable artifact names; may include a SemVer prerelease suffix." FORCE)
set(CPACK_PACKAGE_VERSION ${GDN_ARTIFACT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# CPack captures the CPACK_* variables when it is included, so this must stay last. Including
# it earlier silently discarded the GDN_ARTIFACT_VERSION override above.
include(CPack)
