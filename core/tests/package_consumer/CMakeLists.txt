cmake_minimum_required(VERSION 3.17)
project(enhance_package_consumer LANGUAGES CXX)

find_package(giga-edge-enhance CONFIG REQUIRED)

add_executable(enhance_package_consumer main.cpp)
target_link_libraries(enhance_package_consumer PRIVATE giga-edge::enhance)
target_compile_features(enhance_package_consumer PRIVATE cxx_std_17)
