set(GEE_COMPILER_CACHE "AUTO" CACHE STRING
  "Compiler cache launcher: AUTO, SCCACHE, CCACHE, or OFF.")
set_property(CACHE GEE_COMPILER_CACHE PROPERTY STRINGS AUTO SCCACHE CCACHE OFF)

string(TOUPPER "${GEE_COMPILER_CACHE}" _gee_compiler_cache_mode)

set(_gee_owns_compiler_launcher OFF)
if(DEFINED GEE_COMPILER_CACHE_LAUNCHER
    AND "${CMAKE_C_COMPILER_LAUNCHER}" STREQUAL "${GEE_COMPILER_CACHE_LAUNCHER}"
    AND "${CMAKE_CXX_COMPILER_LAUNCHER}" STREQUAL "${GEE_COMPILER_CACHE_LAUNCHER}")
  set(_gee_owns_compiler_launcher ON)
endif()

if(_gee_compiler_cache_mode STREQUAL "OFF")
  if(_gee_owns_compiler_launcher)
    unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
    unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
    unset(GEE_COMPILER_CACHE_LAUNCHER CACHE)
  elseif(CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
    set(GEE_COMPILER_CACHE_ACTIVE "CUSTOM" CACHE INTERNAL
      "Active compiler cache launcher" FORCE)
    message(STATUS
      "Compiler cache: caller-provided launcher remains active while GigaEdgeEnhance cache detection is off")
    return()
  endif()
  set(GEE_COMPILER_CACHE_ACTIVE "OFF" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  return()
endif()

if((CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
    AND NOT _gee_owns_compiler_launcher)
  set(GEE_COMPILER_CACHE_ACTIVE "CUSTOM" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  message(STATUS "Compiler cache: using caller-provided compiler launcher")
  return()
endif()

unset(_gee_compiler_cache_program CACHE)
if(_gee_compiler_cache_mode STREQUAL "AUTO")
  find_program(_gee_compiler_cache_program NAMES sccache ccache)
elseif(_gee_compiler_cache_mode STREQUAL "SCCACHE")
  find_program(_gee_compiler_cache_program NAMES sccache)
elseif(_gee_compiler_cache_mode STREQUAL "CCACHE")
  find_program(_gee_compiler_cache_program NAMES ccache)
else()
  message(FATAL_ERROR
    "GEE_COMPILER_CACHE must be AUTO, SCCACHE, CCACHE, or OFF; got "
    "'${GEE_COMPILER_CACHE}'.")
endif()

if(_gee_compiler_cache_program)
  set(CMAKE_C_COMPILER_LAUNCHER "${_gee_compiler_cache_program}" CACHE FILEPATH
    "C compiler launcher" FORCE)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${_gee_compiler_cache_program}" CACHE FILEPATH
    "C++ compiler launcher" FORCE)
  set(GEE_COMPILER_CACHE_LAUNCHER "${_gee_compiler_cache_program}"
    CACHE INTERNAL "Compiler launcher managed by GigaEdgeEnhance" FORCE)
  get_filename_component(_gee_compiler_cache_name
    "${_gee_compiler_cache_program}" NAME)
  set(GEE_COMPILER_CACHE_ACTIVE "${_gee_compiler_cache_name}" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  message(STATUS "Compiler cache: ${_gee_compiler_cache_program}")
elseif(_gee_compiler_cache_mode STREQUAL "AUTO")
  if(_gee_owns_compiler_launcher)
    unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
    unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
    unset(GEE_COMPILER_CACHE_LAUNCHER CACHE)
  endif()
  set(GEE_COMPILER_CACHE_ACTIVE "NOT_FOUND" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  message(STATUS "Compiler cache: sccache/ccache not found; continuing without one")
else()
  message(FATAL_ERROR
    "GEE_COMPILER_CACHE=${GEE_COMPILER_CACHE}, but the requested launcher "
    "was not found in PATH.")
endif()
