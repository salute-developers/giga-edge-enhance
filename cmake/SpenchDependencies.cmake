include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
CPMUsePackageLock("${CMAKE_SOURCE_DIR}/package-lock.cmake")

if(NOT TARGET dsplib)
  CPMGetPackage(dsplib)
endif()

CPMGetPackage(speexdsp_spench)
CPMGetPackage(spdlog)

if(NOT TARGET yaml-cpp)
  CPMGetPackage(yaml-cpp)
endif()

CPMGetPackage(rnnoise)
