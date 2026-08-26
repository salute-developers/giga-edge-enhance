set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
  add_compile_definitions(_USE_MATH_DEFINES)
  if(BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
endif()

if(BUILD_SHARED_LIBS AND NOT WIN32)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WIN32 AND NOT APPLE AND NOT EMSCRIPTEN
   AND NOT BUILD_WITH_ADDRESS_SANITIZER)
  add_link_options($<$<CONFIG:Release>:-s>)
endif()

if(BUILD_WITH_ADDRESS_SANITIZER)
  if(WIN32)
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT APPLE)
    execute_process(
      COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
      OUTPUT_VARIABLE clang_resource_dir
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(
      -fsanitize=address
      -shared-libasan
      "-Wl,-rpath,${clang_resource_dir}/lib/linux"
    )
  else()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
  endif()
endif()

if(NOT WIN32)
  add_compile_options(-Wall)
endif()
