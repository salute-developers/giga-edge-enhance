get_filename_component(_spench_root_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED CPM_SOURCE_CACHE)
  set(CPM_SOURCE_CACHE "${_spench_root_dir}/out/cache/cpm" CACHE PATH
      "Shared Spench CPM source cache")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
CPMUsePackageLock("${_spench_root_dir}/package-lock.cmake")

if(SPENCH_PROFILE STREQUAL "model-packer")
  CPMGetPackage(yaml-cpp)
  CPMGetPackage(argparse)
else()
  if(NOT TARGET dsplib)
    CPMGetPackage(dsplib)
  endif()

  CPMGetPackage(speexdsp_spench)
  CPMGetPackage(spdlog)

  if(NOT TARGET yaml-cpp)
    CPMGetPackage(yaml-cpp)
  endif()

  CPMGetPackage(rnnoise)
endif()
