project(model_packer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

CPMGetPackage(argparse)

add_executable(${PROJECT_NAME} model_packer.cpp)
ges_apply_build_config(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE util::packed_model yaml-cpp argparse)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "model_packer")
