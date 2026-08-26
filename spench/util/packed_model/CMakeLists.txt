add_library(util__packed_model STATIC packed_model.cpp)

target_include_directories(util__packed_model
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/spench/include>"
)

set_property(TARGET util__packed_model PROPERTY CXX_STANDARD 17)

add_library(util::packed_model ALIAS util__packed_model)
