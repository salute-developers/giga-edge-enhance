cmake_minimum_required(VERSION 3.17)
project(spench_package_consumer LANGUAGES CXX)

find_package(gigaedge-spench CONFIG REQUIRED)

add_executable(spench_package_consumer main.cpp)
target_link_libraries(spench_package_consumer PRIVATE gigaedge-spench::gigaedge-spench)
target_compile_features(spench_package_consumer PRIVATE cxx_std_17)
