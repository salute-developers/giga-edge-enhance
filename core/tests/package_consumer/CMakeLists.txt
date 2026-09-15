cmake_minimum_required(VERSION 3.17)
project(gigadenoiser_package_consumer LANGUAGES CXX)

find_package(giga-denoiser CONFIG REQUIRED)

add_executable(gigadenoiser_package_consumer main.cpp)
target_link_libraries(gigadenoiser_package_consumer PRIVATE giga-denoiser::giga-denoiser)
target_compile_features(gigadenoiser_package_consumer PRIVATE cxx_std_17)
