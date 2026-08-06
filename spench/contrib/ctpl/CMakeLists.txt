cmake_minimum_required( VERSION 3.17.3 )

project( CTPL )

add_library(ctpl STATIC ctpl_stl.cc ctpl_stl.h)

set_property(TARGET ctpl PROPERTY CXX_STANDARD 14)

set_target_properties(ctpl PROPERTIES
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)
