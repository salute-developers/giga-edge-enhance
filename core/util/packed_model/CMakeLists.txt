add_library(util__packed_model STATIC packed_model.cpp)

# Anchor include paths to the GigaDenoiser project root (set by project(giga-denoiser)),
# not CMAKE_SOURCE_DIR. When the Android/iOS wrappers embed GigaDenoiser via
# add_subdirectory(), CMAKE_SOURCE_DIR points at the wrapper project and the
# public headers under core/include would not be found.
target_include_directories(util__packed_model
  PUBLIC
    "$<BUILD_INTERFACE:${giga-denoiser_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${giga-denoiser_SOURCE_DIR}/core/include>"
  PRIVATE
    "$<BUILD_INTERFACE:${giga-denoiser_SOURCE_DIR}/core>"
)

set_property(TARGET util__packed_model PROPERTY CXX_STANDARD 17)

add_library(util::packed_model ALIAS util__packed_model)
