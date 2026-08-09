cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR SOURCE_ROOT STREQUAL "")
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()
if(NOT DEFINED TEST_ROOT OR TEST_ROOT STREQUAL "")
    set(TEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/runtime-asset-sync-self-test")
endif()

function(run_checked label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed (${result})\nstdout:\n${output}\nstderr:\n${error}")
    endif()
    set(RUN_OUTPUT "${output}${error}" PARENT_SCOPE)
endfunction()

function(require_file_content path expected)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected copied asset is missing: ${path}")
    endif()
    file(READ "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Unexpected asset content at ${path}: '${actual}', expected '${expected}'")
    endif()
endfunction()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY
    "${TEST_ROOT}/fixture/assets/fonts"
    "${TEST_ROOT}/fixture/assets/stage12")
file(WRITE "${TEST_ROOT}/fixture/assets/fonts/font.txt" "font-v1")
file(WRITE "${TEST_ROOT}/fixture/assets/stage12/changed.txt" "asset-v1")

file(TO_CMAKE_PATH "${SOURCE_ROOT}/cmake/ArpgRuntimeAssets.cmake" module_path)
file(WRITE "${TEST_ROOT}/fixture/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25)
project(runtime_asset_sync_fixture NONE)
include("@MODULE_PATH@")
add_custom_target(fake_game ALL)
arpg_add_runtime_asset_sync(
    TARGET fake_game
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    DESTINATION_ROOT "${CMAKE_CURRENT_BINARY_DIR}/runtime/assets"
    DIRECTORIES fonts stage12)
]=])
file(READ "${TEST_ROOT}/fixture/CMakeLists.txt" fixture_cmake)
string(REPLACE "@MODULE_PATH@" "${module_path}" fixture_cmake "${fixture_cmake}")
file(WRITE "${TEST_ROOT}/fixture/CMakeLists.txt" "${fixture_cmake}")

set(fixture_root "${TEST_ROOT}/fixture")
set(build_root "${TEST_ROOT}/build")
set(destination_root "${build_root}/runtime/assets")
set(stamp "${build_root}/fake_game-runtime-assets.stamp")

run_checked("configure fixture" "${CMAKE_COMMAND}"
    -S "${fixture_root}" -B "${build_root}" -G Ninja)
run_checked("initial asset build" "${CMAKE_COMMAND}" --build "${build_root}")
require_file_content("${destination_root}/fonts/font.txt" "font-v1")
require_file_content("${destination_root}/stage12/changed.txt" "asset-v1")
if(NOT EXISTS "${stamp}")
    message(FATAL_ERROR "Runtime asset stamp is missing after initial build")
endif()

file(TIMESTAMP "${stamp}" unchanged_stamp_before "%s" UTC)
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
run_checked("no-change reconfigure" "${CMAKE_COMMAND}"
    -S "${fixture_root}" -B "${build_root}" -G Ninja)
run_checked("no-change asset build" "${CMAKE_COMMAND}" --build "${build_root}")
file(TIMESTAMP "${stamp}" unchanged_stamp_after "%s" UTC)
if(NOT unchanged_stamp_before STREQUAL unchanged_stamp_after)
    message(FATAL_ERROR "No-change build reran runtime asset synchronization")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(WRITE "${fixture_root}/assets/stage12/changed.txt" "asset-v2")
run_checked("modified asset build" "${CMAKE_COMMAND}" --build "${build_root}")
require_file_content("${destination_root}/stage12/changed.txt" "asset-v2")

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(WRITE "${fixture_root}/assets/stage12/added.txt" "asset-added")
run_checked("added asset build" "${CMAKE_COMMAND}" --build "${build_root}")
require_file_content("${destination_root}/stage12/added.txt" "asset-added")

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(REMOVE "${fixture_root}/assets/stage12/changed.txt")
run_checked("deleted asset build" "${CMAKE_COMMAND}" --build "${build_root}")
if(EXISTS "${destination_root}/stage12/changed.txt")
    message(FATAL_ERROR "Deleted source asset remained in runtime output")
endif()
require_file_content("${destination_root}/stage12/added.txt" "asset-added")

message(STATUS "Runtime asset sync self-test passed")
