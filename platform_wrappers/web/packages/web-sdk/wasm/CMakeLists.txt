cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 17)

if(BUILD_DSP)
    set(MODULE_NAME "spench.dsp")
    add_definitions(-DBUILD_DSP)
else()
    set(MODULE_NAME "spench")
endif()

add_executable(spench_wasm src/main.cpp
                           src/elements.cpp)

set_target_properties(spench_wasm PROPERTIES OUTPUT_NAME ${MODULE_NAME})
target_include_directories(spench_wasm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR})

target_link_libraries(spench_wasm PRIVATE spench)

if(BUILD_DSP)
    set(spench_wasm_common_compile_and_link_options
        -O3
        -msimd128
        --closure=1
        -flto
        --emit-tsd=${MODULE_NAME}.d.ts
        --extern-pre-js=${CMAKE_CURRENT_SOURCE_DIR}/dsp_shim.js
        # DEBUG:
        # -O0
        # -g
        # -fsanitize=address
        # -fsanitize=undefined
        # -sASSERTIONS=2
        # -fexceptions
        # -sNO_DISABLE_EXCEPTION_CATCHING
    )
else()
    set(spench_wasm_common_compile_and_link_options
        -O3
        -msimd128
        -flto
        --emit-tsd=${MODULE_NAME}.d.ts
        # --closure=1 # TODO error missing require — pass parameters to closure compiler
        # DEBUG:
        # -O0
        # -g
        # -fsanitize=address
        # -fsanitize=undefined
        # -sASSERTIONS=2
        # -fexceptions
        # -sNO_DISABLE_EXCEPTION_CATCHING
    )
endif()

target_compile_options(spench_wasm PUBLIC
        ${spench_wasm_common_compile_and_link_options}
)

if(BUILD_DSP)
    target_link_options(spench_wasm PUBLIC
            ${spench_wasm_common_compile_and_link_options}
            -sMODULARIZE=1
            -sEXPORT_ES6=1
            # This prevents from usage "import.meta.url" which is not supported for a web worker.
            -sUSE_ES6_IMPORT_META=0
            -sENVIRONMENT=worker
            -sINITIAL_HEAP=16777216
            -sALLOW_MEMORY_GROWTH=1
            -sFILESYSTEM=0
            -sSTACK_SIZE=524288
            -sEXPORTED_FUNCTIONS=_malloc,_free
            -lembind
            # This is ok because compilation will be outside of main thread (in worker).
            -sWASM_ASYNC_COMPILATION=0 # TODO check as this can block audio thread
    )
else()
    target_link_options(spench_wasm PUBLIC
            ${spench_wasm_common_compile_and_link_options}
            # Creates module from glue code for better integration.
            -sMODULARIZE=1
            # Jazz uses es6 import
            -sEXPORT_ES6=1
            # This prevents from usage "import.meta.url" which is not supported for a web worker.
            -sUSE_ES6_IMPORT_META=0
            -sENVIRONMENT=worker
            -sFILESYSTEM=0
            -sINITIAL_HEAP=67108864
            -sALLOW_MEMORY_GROWTH=1
            -sSTACK_SIZE=524288
            -sEXPORTED_FUNCTIONS=_malloc,_free
            -lembind
            # This is ok because compilation will be outside of main thread (in worker).
            -sWASM_ASYNC_COMPILATION=0
    )
endif()
