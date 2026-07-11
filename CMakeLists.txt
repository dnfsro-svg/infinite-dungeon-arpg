cmake_minimum_required(VERSION 3.25)

project(infinite_dungeon_arpg VERSION 0.1.0 LANGUAGES C CXX)
include(cmake/NormalizeMsvcShowIncludes.cmake)

option(ARPG_BUILD_GRAPHICS "Build the raylib host and game executable" ON)

if(NOT MSVC)
    message(FATAL_ERROR "Stage 0 requires MSVC")
endif()

if(NOT MSVC_VERSION EQUAL 1944)
    message(FATAL_ERROR
        "Stage 0 requires MSVC 19.44; detected "
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Stage 0 requires an x64 target")
endif()

string(REGEX REPLACE "[/\\\\]+$" "" ARPG_WINDOWS_SDK "$ENV{WindowsSDKVersion}")
if(NOT ARPG_WINDOWS_SDK STREQUAL "10.0.26100.0")
    message(FATAL_ERROR
        "Stage 0 requires Windows SDK 10.0.26100.0; detected "
        "'${ARPG_WINDOWS_SDK}'")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

function(arpg_enable_project_warnings target_name)
    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /W4 /permissive-)
    endif()
endfunction()

include(CTest)
add_subdirectory(src/core)
add_subdirectory(src/combat)

if(ARPG_BUILD_GRAPHICS)
    include(cmake/Dependencies.cmake)
    add_subdirectory(src/platform/raylib)
    add_subdirectory(src/app)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests/core)
    add_subdirectory(tests/combat)
    add_subdirectory(tests/platform)
endif()
