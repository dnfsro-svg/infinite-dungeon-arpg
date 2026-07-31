if(NOT DEFINED CHECKPOINT_SOURCE_DIR)
    message(FATAL_ERROR "CHECKPOINT_SOURCE_DIR is required")
endif()
if(NOT DEFINED CHECKPOINT_CMAKE_FILE)
    message(FATAL_ERROR "CHECKPOINT_CMAKE_FILE is required")
endif()

function(checkpoint_find_boundary_violation
        SOURCE_ROOT CMAKE_FILE OUT_FOUND OUT_DIAGNOSTIC)
    if(NOT IS_DIRECTORY "${SOURCE_ROOT}")
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_DIAGNOSTIC}"
            "Checkpoint boundary violation: source directory does not exist: ${SOURCE_ROOT}"
            PARENT_SCOPE)
        return()
    endif()
    if(NOT EXISTS "${CMAKE_FILE}")
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_DIAGNOSTIC}"
            "Checkpoint boundary violation: target file does not exist: ${CMAKE_FILE}"
            PARENT_SCOPE)
        return()
    endif()

    file(GLOB_RECURSE _checkpoint_sources
        "${SOURCE_ROOT}/*.h"
        "${SOURCE_ROOT}/*.hh"
        "${SOURCE_ROOT}/*.hpp"
        "${SOURCE_ROOT}/*.hxx"
        "${SOURCE_ROOT}/*.inl"
        "${SOURCE_ROOT}/*.inc"
        "${SOURCE_ROOT}/*.ipp"
        "${SOURCE_ROOT}/*.tpp"
        "${SOURCE_ROOT}/*.tcc"
        "${SOURCE_ROOT}/*.c"
        "${SOURCE_ROOT}/*.cc"
        "${SOURCE_ROOT}/*.cpp"
        "${SOURCE_ROOT}/*.cxx"
        "${SOURCE_ROOT}/*.ixx"
        "${SOURCE_ROOT}/*.cppm"
        "${SOURCE_ROOT}/*.mpp")
    list(SORT _checkpoint_sources)

    foreach(_checkpoint_source IN LISTS _checkpoint_sources)
        file(STRINGS "${_checkpoint_source}" _checkpoint_lines)
        set(_checkpoint_line_number 0)
        foreach(_checkpoint_line IN LISTS _checkpoint_lines)
            math(EXPR _checkpoint_line_number
                "${_checkpoint_line_number} + 1")
            string(TOLOWER
                "${_checkpoint_line}" _checkpoint_line_lower)
            if(_checkpoint_line_lower MATCHES
                    "^[ \t]*#[ \t]*include[ \t]*[<\"]([.][.]?[/\\\\])*(combat|dungeon|persistence|platform|app|raylib)([/\\\\.]|[>\"])")
                string(REGEX MATCH
                    "[<\"]([^>\"]+)[>\"]"
                    _checkpoint_include_match
                    "${_checkpoint_line}")
                set(_checkpoint_include "${CMAKE_MATCH_1}")
                file(RELATIVE_PATH
                    _checkpoint_relative "${SOURCE_ROOT}" "${_checkpoint_source}")
                set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                set("${OUT_DIAGNOSTIC}"
                    "Checkpoint boundary violation: forbidden runtime include '${_checkpoint_include}' in ${_checkpoint_relative}:${_checkpoint_line_number}"
                    PARENT_SCOPE)
                return()
            endif()
        endforeach()

        file(READ "${_checkpoint_source}" _checkpoint_source_content)
        string(REPLACE "\r\n" "\n" _checkpoint_source_content
            "${_checkpoint_source_content}")
        string(REPLACE "\r" "\n" _checkpoint_source_content
            "${_checkpoint_source_content}")
        string(REGEX REPLACE "\\\\[ \t]*\n[ \t]*" ""
            _checkpoint_logical_content "${_checkpoint_source_content}")
        string(TOLOWER "${_checkpoint_logical_content}"
            _checkpoint_logical_content_lower)
        if(_checkpoint_logical_content_lower MATCHES
                "(^|\n)[ \t]*#[ \t]*include[ \t]*[<\"]([.][.]?[/\\\\])*(combat|dungeon|persistence|platform|app|raylib)([/\\\\.][^>\"\n]*)?[>\"]")
            set(_checkpoint_forbidden_directive "${CMAKE_MATCH_0}")
            string(REGEX MATCH
                "[<\"]([^>\"]+)[>\"]"
                _checkpoint_include_match
                "${_checkpoint_forbidden_directive}")
            set(_checkpoint_include "${CMAKE_MATCH_1}")
            file(RELATIVE_PATH
                _checkpoint_relative "${SOURCE_ROOT}" "${_checkpoint_source}")
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_DIAGNOSTIC}"
                "Checkpoint boundary violation: forbidden runtime include '${_checkpoint_include}' in ${_checkpoint_relative} after logical-line normalization"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(_checkpoint_forbidden_targets
        arpg_combat
        arpg_dungeon
        arpg_persistence
        arpg_platform
        arpg_settings
        arpg_raylib
        arpg_app
        arpg_game
        raylib)
    file(STRINGS "${CMAKE_FILE}" _checkpoint_cmake_lines)
    set(_checkpoint_line_number 0)
    foreach(_checkpoint_line IN LISTS _checkpoint_cmake_lines)
        math(EXPR _checkpoint_line_number "${_checkpoint_line_number} + 1")
        string(REGEX REPLACE "[ \t]*#.*$" ""
            _checkpoint_code "${_checkpoint_line}")
        string(TOLOWER "${_checkpoint_code}" _checkpoint_code_lower)
        foreach(_checkpoint_target IN LISTS _checkpoint_forbidden_targets)
            if(_checkpoint_code_lower MATCHES
                    "(^|[^a-z0-9_])${_checkpoint_target}([^a-z0-9_]|$)")
                set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                set("${OUT_DIAGNOSTIC}"
                    "Checkpoint boundary violation: forbidden runtime target token '${_checkpoint_target}' in ${CMAKE_FILE}:${_checkpoint_line_number}"
                    PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_DIAGNOSTIC}" "" PARENT_SCOPE)
endfunction()

checkpoint_find_boundary_violation(
    "${CHECKPOINT_SOURCE_DIR}"
    "${CHECKPOINT_CMAKE_FILE}"
    _checkpoint_found
    _checkpoint_diagnostic)
if(_checkpoint_found)
    message(FATAL_ERROR "${_checkpoint_diagnostic}")
endif()

if(CHECKPOINT_BOUNDARY_SCAN_ONLY)
    message(STATUS "checkpoint-boundary-scan=PASS")
    return()
endif()

if(NOT DEFINED ASSERT_TARGET_BOUNDARY_MODULE
        OR NOT EXISTS "${ASSERT_TARGET_BOUNDARY_MODULE}")
    message(FATAL_ERROR
        "ASSERT_TARGET_BOUNDARY_MODULE must name the dependency assertion module")
endif()
if(NOT DEFINED CHECKPOINT_BOUNDARY_SELF_TEST_ROOT
        OR CHECKPOINT_BOUNDARY_SELF_TEST_ROOT STREQUAL "")
    message(FATAL_ERROR "CHECKPOINT_BOUNDARY_SELF_TEST_ROOT is required")
endif()

cmake_path(ABSOLUTE_PATH CHECKPOINT_SOURCE_DIR
    NORMALIZE OUTPUT_VARIABLE _checkpoint_source_root)
cmake_path(GET _checkpoint_source_root PARENT_PATH _checkpoint_src_root)
cmake_path(GET _checkpoint_src_root PARENT_PATH _checkpoint_project_root)
cmake_path(ABSOLUTE_PATH CHECKPOINT_BOUNDARY_SELF_TEST_ROOT
    NORMALIZE OUTPUT_VARIABLE _checkpoint_self_test_root)
cmake_path(IS_PREFIX _checkpoint_source_root
    "${_checkpoint_self_test_root}" NORMALIZE _checkpoint_self_test_in_checkpoint)
cmake_path(IS_PREFIX _checkpoint_self_test_root
    "${_checkpoint_project_root}" NORMALIZE _checkpoint_self_test_is_ancestor)
cmake_path(GET _checkpoint_self_test_root
    FILENAME _checkpoint_self_test_name)
if(_checkpoint_self_test_in_checkpoint
        OR _checkpoint_self_test_is_ancestor
        OR NOT _checkpoint_self_test_name STREQUAL
            "checkpoint-boundary-self-test")
    message(FATAL_ERROR
        "CHECKPOINT_BOUNDARY_SELF_TEST_ROOT must be a dedicated checkpoint-boundary-self-test directory outside src/checkpoint")
endif()

file(REMOVE_RECURSE "${_checkpoint_self_test_root}")
file(MAKE_DIRECTORY "${_checkpoint_self_test_root}")
set(_checkpoint_self_test_failure "")

set(_checkpoint_include_source
    "${_checkpoint_self_test_root}/forbidden-include-src")
file(MAKE_DIRECTORY "${_checkpoint_include_source}")
file(WRITE "${_checkpoint_include_source}/forbidden.hpp"
    "#include \"combat/combat_types.hpp\"\n")
file(WRITE "${_checkpoint_include_source}/CMakeLists.txt"
    "add_library(checkpoint_fixture INTERFACE)\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DCHECKPOINT_SOURCE_DIR=${_checkpoint_include_source}"
        "-DCHECKPOINT_CMAKE_FILE=${_checkpoint_include_source}/CMakeLists.txt"
        -DCHECKPOINT_BOUNDARY_SCAN_ONLY=ON
        -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE _checkpoint_include_result
    OUTPUT_VARIABLE _checkpoint_include_stdout
    ERROR_VARIABLE _checkpoint_include_stderr)
set(_checkpoint_include_log
    "${_checkpoint_include_stdout}\n${_checkpoint_include_stderr}")
string(REGEX REPLACE "[\r\n\t ]+" " "
    _checkpoint_include_log_normalized "${_checkpoint_include_log}")
set(_checkpoint_include_expected
    "Checkpoint boundary violation: forbidden runtime include 'combat/combat_types.hpp'")
string(FIND "${_checkpoint_include_log_normalized}"
    "${_checkpoint_include_expected}" _checkpoint_include_diagnostic_index)
if(_checkpoint_include_result EQUAL 0
        OR _checkpoint_include_diagnostic_index EQUAL -1)
    string(APPEND _checkpoint_self_test_failure
        "forbidden include mutation did not fail with '${_checkpoint_include_expected}'; result=${_checkpoint_include_result}; log=${_checkpoint_include_log}\n")
endif()

set(_checkpoint_relative_include_source
    "${_checkpoint_self_test_root}/forbidden-relative-include-src")
file(MAKE_DIRECTORY "${_checkpoint_relative_include_source}")
file(WRITE "${_checkpoint_relative_include_source}/forbidden.hpp"
    "#include \"checkpoint/room_checkpoint_schema.hpp\"\n"
    "#include \\\n    \"./../combat/combat_types.hpp\"\n")
file(WRITE "${_checkpoint_relative_include_source}/CMakeLists.txt"
    "add_library(checkpoint_fixture INTERFACE)\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DCHECKPOINT_SOURCE_DIR=${_checkpoint_relative_include_source}"
        "-DCHECKPOINT_CMAKE_FILE=${_checkpoint_relative_include_source}/CMakeLists.txt"
        -DCHECKPOINT_BOUNDARY_SCAN_ONLY=ON
        -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE _checkpoint_relative_include_result
    OUTPUT_VARIABLE _checkpoint_relative_include_stdout
    ERROR_VARIABLE _checkpoint_relative_include_stderr)
set(_checkpoint_relative_include_log
    "${_checkpoint_relative_include_stdout}\n${_checkpoint_relative_include_stderr}")
string(REGEX REPLACE "[\r\n\t ]+" " "
    _checkpoint_relative_include_log_normalized
    "${_checkpoint_relative_include_log}")
set(_checkpoint_relative_include_expected
    "Checkpoint boundary violation: forbidden runtime include './../combat/combat_types.hpp'")
string(FIND "${_checkpoint_relative_include_log_normalized}"
    "${_checkpoint_relative_include_expected}"
    _checkpoint_relative_include_diagnostic_index)
if(_checkpoint_relative_include_result EQUAL 0
        OR _checkpoint_relative_include_diagnostic_index EQUAL -1)
    string(APPEND _checkpoint_self_test_failure
        "relative multiline include mutation did not fail with '${_checkpoint_relative_include_expected}'; result=${_checkpoint_relative_include_result}; log=${_checkpoint_relative_include_log}\n")
endif()

set(_checkpoint_dependency_source
    "${_checkpoint_self_test_root}/forbidden-dependency-src")
set(_checkpoint_dependency_build
    "${_checkpoint_self_test_root}/forbidden-dependency-build")
file(MAKE_DIRECTORY "${_checkpoint_dependency_source}")
file(TO_CMAKE_PATH "${ASSERT_TARGET_BOUNDARY_MODULE}"
    _checkpoint_assert_module_cmake)
file(WRITE "${_checkpoint_dependency_source}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\n"
    "project(checkpoint_boundary_fixture LANGUAGES NONE)\n"
    "include(\"${_checkpoint_assert_module_cmake}\")\n"
    "add_library(checkpoint_fixture_forbidden INTERFACE)\n"
    "add_library(checkpoint_fixture_middle INTERFACE)\n"
    "target_link_libraries(checkpoint_fixture_middle INTERFACE checkpoint_fixture_forbidden)\n"
    "add_library(checkpoint_fixture_root INTERFACE)\n"
    "target_link_libraries(checkpoint_fixture_root INTERFACE checkpoint_fixture_middle)\n"
    "arpg_assert_target_dependency_boundary(checkpoint_fixture_root checkpoint_fixture_forbidden)\n")
set(_checkpoint_dependency_command
    "${CMAKE_COMMAND}"
    -S "${_checkpoint_dependency_source}"
    -B "${_checkpoint_dependency_build}")
if(DEFINED CHECKPOINT_BOUNDARY_SELF_TEST_GENERATOR
        AND NOT CHECKPOINT_BOUNDARY_SELF_TEST_GENERATOR STREQUAL "")
    list(APPEND _checkpoint_dependency_command
        -G "${CHECKPOINT_BOUNDARY_SELF_TEST_GENERATOR}")
endif()
execute_process(
    COMMAND ${_checkpoint_dependency_command}
    RESULT_VARIABLE _checkpoint_dependency_result
    OUTPUT_VARIABLE _checkpoint_dependency_stdout
    ERROR_VARIABLE _checkpoint_dependency_stderr)
set(_checkpoint_dependency_log
    "${_checkpoint_dependency_stdout}\n${_checkpoint_dependency_stderr}")
string(REGEX REPLACE "[\r\n\t ]+" " "
    _checkpoint_dependency_log_normalized "${_checkpoint_dependency_log}")
set(_checkpoint_dependency_expected
    "Target dependency boundary violation: 'checkpoint_fixture_root' reaches forbidden target 'checkpoint_fixture_forbidden'")
string(FIND "${_checkpoint_dependency_log_normalized}"
    "${_checkpoint_dependency_expected}" _checkpoint_dependency_diagnostic_index)
if(_checkpoint_dependency_result EQUAL 0
        OR _checkpoint_dependency_diagnostic_index EQUAL -1)
    string(APPEND _checkpoint_self_test_failure
        "forbidden dependency mutation did not fail with '${_checkpoint_dependency_expected}'; result=${_checkpoint_dependency_result}; log=${_checkpoint_dependency_log}\n")
endif()

file(REMOVE_RECURSE "${_checkpoint_self_test_root}")
if(NOT _checkpoint_self_test_failure STREQUAL "")
    message(FATAL_ERROR "${_checkpoint_self_test_failure}")
endif()

message(STATUS "checkpoint-boundary-scan=PASS")
message(STATUS
    "checkpoint-boundary-include-self-test=PASS diagnostic='${_checkpoint_include_expected}'")
message(STATUS
    "checkpoint-boundary-relative-include-self-test=PASS diagnostic='${_checkpoint_relative_include_expected}'")
message(STATUS
    "checkpoint-boundary-dependency-self-test=PASS diagnostic='${_checkpoint_dependency_expected}'")
