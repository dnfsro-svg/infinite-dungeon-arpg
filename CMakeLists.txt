cmake_minimum_required(VERSION 3.25)

project(infinite_dungeon_arpg VERSION 0.1.0 LANGUAGES C CXX)
include(cmake/NormalizeMsvcShowIncludes.cmake)
include(cmake/AssertTargetDependencyBoundary.cmake)

option(ARPG_BUILD_GRAPHICS "Build the raylib host and game executable" ON)
option(ARPG_BUILD_PERFORMANCE_PROBES "Build local performance probes" OFF)

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
add_subdirectory(src/abyss)
add_subdirectory(src/skills)
add_subdirectory(src/modifiers)
add_subdirectory(src/items)
add_subdirectory(src/progression)
add_subdirectory(src/passives)
add_subdirectory(src/checkpoint)
add_subdirectory(src/combat)
add_subdirectory(src/dungeon)
add_subdirectory(src/persistence)
add_subdirectory(src/platform/settings)

if(ARPG_BUILD_PERFORMANCE_PROBES)
    add_subdirectory(tools/performance)
endif()

if(ARPG_BUILD_GRAPHICS)
    include(cmake/Dependencies.cmake)
    add_subdirectory(src/platform/raylib)
    add_subdirectory(src/app)
endif()

add_subdirectory(src/launcher)

if(BUILD_TESTING)
    add_subdirectory(tests/core)
    add_subdirectory(tests/abyss)
    add_subdirectory(tests/skills)
    add_subdirectory(tests/checkpoint)
    add_subdirectory(tests/combat)
    add_subdirectory(tests/dungeon)
    add_subdirectory(tests/persistence)
    add_subdirectory(tests/platform)
    add_subdirectory(tests/progression)
    add_subdirectory(tests/modifiers)
    add_subdirectory(tests/passives)
    add_subdirectory(tests/items)
    add_subdirectory(tests/settings)
    add_subdirectory(tests/launcher)
endif()

set(_arpg_checkpoint_forbidden_targets
    arpg_combat
    arpg_dungeon
    arpg_persistence
    arpg_settings)
foreach(_arpg_checkpoint_optional_forbidden IN ITEMS
        raylib arpg_raylib arpg_game)
    if(TARGET "${_arpg_checkpoint_optional_forbidden}")
        list(APPEND _arpg_checkpoint_forbidden_targets
            "${_arpg_checkpoint_optional_forbidden}")
    endif()
endforeach()
arpg_assert_target_dependency_boundary(
    arpg_checkpoint ${_arpg_checkpoint_forbidden_targets})

set(_arpg_persistence_boundary_roots arpg_persistence)
if(TARGET arpg_persistence_link_isolation_tests)
    list(APPEND _arpg_persistence_boundary_roots
        arpg_persistence_link_isolation_tests)
endif()
foreach(_arpg_persistence_boundary_root IN LISTS
        _arpg_persistence_boundary_roots)
    arpg_assert_target_dependency_boundary(
        "${_arpg_persistence_boundary_root}" arpg_combat arpg_dungeon)
endforeach()
