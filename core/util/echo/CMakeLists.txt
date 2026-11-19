project(util__echo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# implement FFT backend for speexdsp
add_library(spx_fft STATIC speex_fftwrap.cpp)
gee_apply_build_config(spx_fft)
target_link_libraries(spx_fft PRIVATE pocketfft)
target_link_libraries(speexdsp_gdn PRIVATE spx_fft)

add_library(${PROJECT_NAME} STATIC echo_canceler.cpp)
gee_apply_build_config(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE
    dsplib 
    speexdsp_gdn
)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
