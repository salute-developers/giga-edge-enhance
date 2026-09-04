add_library(util__packed_model STATIC packed_model.cpp)

# Anchor include paths to the Spench project root (set by project(spench)),
# not CMAKE_SOURCE_DIR. When the Android/iOS wrappers embed Spench via
# add_subdirectory(), CMAKE_SOURCE_DIR points at the wrapper project and the
# public headers under spench/include would not be found.
target_include_directories(util__packed_model
  PUBLIC
    "$<BUILD_INTERFACE:${spench_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${spench_SOURCE_DIR}/spench/include>"
)

set_property(TARGET util__packed_model PROPERTY CXX_STANDARD 17)

add_library(util::packed_model ALIAS util__packed_model)
