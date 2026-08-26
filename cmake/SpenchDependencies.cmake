include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
get_filename_component(_spench_root_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
CPMUsePackageLock("${_spench_root_dir}/package-lock.cmake")

if(NOT TARGET dsplib)
  CPMGetPackage(dsplib)
endif()

CPMGetPackage(speexdsp_spench)
CPMGetPackage(spdlog)

if(NOT TARGET yaml-cpp)
  CPMGetPackage(yaml-cpp)
endif()

CPMGetPackage(rnnoise)
