cmake_minimum_required(VERSION 3.16)
project(ugaris-radar-mod C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MOD_OUTPUT_NAME "bmod")

add_library(${MOD_OUTPUT_NAME} SHARED
    src/main.c
)

target_include_directories(${MOD_OUTPUT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# The SDK headers pull in the client's memory.h; build against libc, not mimalloc.
target_compile_definitions(${MOD_OUTPUT_NAME} PRIVATE USE_MIMALLOC=0)

if(NOT WIN32)
    target_link_libraries(${MOD_OUTPUT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

# Platform-specific settings. Mods reference symbols exported by the game
# executable. On Linux/macOS the dynamic loader resolves them at load time;
# on Windows the mod MUST link the client's import library (lib/moac.a from
# the client release's mod-sdk.zip) or every client API call is a null
# pointer and the game crashes the moment the mod runs.
if(WIN32)
    set_target_properties(${MOD_OUTPUT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/moac.a" AND NOT MSVC)
        target_link_libraries(${MOD_OUTPUT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/lib/moac.a")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/lib/moac.lib")
        target_link_libraries(${MOD_OUTPUT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/lib/moac.lib")
    else()
        message(FATAL_ERROR
            "Windows builds need the client import library. Download mod-sdk.zip "
            "from an astonia_community_client release and copy lib/moac.a and "
            "lib/moac.lib into ${CMAKE_SOURCE_DIR}/lib/ first.")
    endif()
elseif(APPLE)
    set_target_properties(${MOD_OUTPUT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".dylib"
    )
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")
    endif()
    target_link_options(${MOD_OUTPUT_NAME} PRIVATE -undefined dynamic_lookup)
else()
    set_target_properties(${MOD_OUTPUT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".so"
    )
    target_compile_options(${MOD_OUTPUT_NAME} PRIVATE -fvisibility=hidden)
    target_link_options(${MOD_OUTPUT_NAME} PRIVATE -Wl,--allow-shlib-undefined)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${MOD_OUTPUT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${MOD_OUTPUT_NAME} PRIVATE -O2)
    endif()
endif()

install(TARGETS ${MOD_OUTPUT_NAME}
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)
