cmake_minimum_required(VERSION 3.20)

project(rip_node_modules C)

if(MSVC)
    set(CMAKE_C_STANDARD 17)
else()
    set(CMAKE_C_STANDARD 23)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(rip-node_modules
    src/main.c
    src/platform.c
    src/report.c
    src/scanner.c
)

target_include_directories(rip-node_modules PRIVATE src)

if(MSVC)
    target_compile_options(rip-node_modules PRIVATE /W4 /permissive- /utf-8 /std:clatest)
    target_compile_definitions(rip-node_modules PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(rip-node_modules PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_definitions(rip-node_modules PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(rip-node_modules PRIVATE /O2)
    else()
        target_compile_options(rip-node_modules PRIVATE -O3)
    endif()
endif()

if(WIN32)
    target_link_libraries(rip-node_modules PRIVATE Shlwapi)
endif()
