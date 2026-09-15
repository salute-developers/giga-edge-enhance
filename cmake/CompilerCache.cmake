set(GDN_COMPILER_CACHE "AUTO" CACHE STRING
  "Compiler cache launcher: AUTO, SCCACHE, CCACHE, or OFF.")
set_property(CACHE GDN_COMPILER_CACHE PROPERTY STRINGS AUTO SCCACHE CCACHE OFF)

string(TOUPPER "${GDN_COMPILER_CACHE}" _gdn_compiler_cache_mode)

set(_gdn_owns_compiler_launcher OFF)
if(DEFINED GDN_COMPILER_CACHE_LAUNCHER
    AND "${CMAKE_C_COMPILER_LAUNCHER}" STREQUAL "${GDN_COMPILER_CACHE_LAUNCHER}"
    AND "${CMAKE_CXX_COMPILER_LAUNCHER}" STREQUAL "${GDN_COMPILER_CACHE_LAUNCHER}")
  set(_gdn_owns_compiler_launcher ON)
endif()

if(_gdn_compiler_cache_mode STREQUAL "OFF")
  if(_gdn_owns_compiler_launcher)
    unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
    unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
    unset(GDN_COMPILER_CACHE_LAUNCHER CACHE)
  elseif(CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
    set(GDN_COMPILER_CACHE_ACTIVE "CUSTOM" CACHE INTERNAL
      "Active compiler cache launcher" FORCE)
    message(STATUS
      "Compiler cache: caller-provided launcher remains active while GigaDenoiser cache detection is off")
    return()
  endif()
  set(GDN_COMPILER_CACHE_ACTIVE "OFF" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  return()
endif()

if((CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
    AND NOT _gdn_owns_compiler_launcher)
  set(GDN_COMPILER_CACHE_ACTIVE "CUSTOM" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  message(STATUS "Compiler cache: using caller-provided compiler launcher")
  return()
endif()

unset(_gdn_compiler_cache_program CACHE)
if(_gdn_compiler_cache_mode STREQUAL "AUTO")
  find_program(_gdn_compiler_cache_program NAMES sccache ccache)
elseif(_gdn_compiler_cache_mode STREQUAL "SCCACHE")
  find_program(_gdn_compiler_cache_program NAMES sccache)
elseif(_gdn_compiler_cache_mode STREQUAL "CCACHE")
  find_program(_gdn_compiler_cache_program NAMES ccache)
else()
  message(FATAL_ERROR
    "GDN_COMPILER_CACHE must be AUTO, SCCACHE, CCACHE, or OFF; got "
    "'${GDN_COMPILER_CACHE}'.")
endif()

if(_gdn_compiler_cache_program)
  set(CMAKE_C_COMPILER_LAUNCHER "${_gdn_compiler_cache_program}" CACHE FILEPATH
    "C compiler launcher" FORCE)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${_gdn_compiler_cache_program}" CACHE FILEPATH
    "C++ compiler launcher" FORCE)
  set(GDN_COMPILER_CACHE_LAUNCHER "${_gdn_compiler_cache_program}"
    CACHE INTERNAL "Compiler launcher managed by GigaDenoiser" FORCE)
  get_filename_component(_gdn_compiler_cache_name
    "${_gdn_compiler_cache_program}" NAME)
  set(GDN_COMPILER_CACHE_ACTIVE "${_gdn_compiler_cache_name}" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  message(STATUS "Compiler cache: ${_gdn_compiler_cache_program}")
elseif(_gdn_compiler_cache_mode STREQUAL "AUTO")
  if(_gdn_owns_compiler_launcher)
    unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
    unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
    unset(GDN_COMPILER_CACHE_LAUNCHER CACHE)
  endif()
  set(GDN_COMPILER_CACHE_ACTIVE "NOT_FOUND" CACHE INTERNAL
    "Active compiler cache launcher" FORCE)
  message(STATUS "Compiler cache: sccache/ccache not found; continuing without one")
else()
  message(FATAL_ERROR
    "GDN_COMPILER_CACHE=${GDN_COMPILER_CACHE}, but the requested launcher "
    "was not found in PATH.")
endif()
