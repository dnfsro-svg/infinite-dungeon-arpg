if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED SOURCE_LABEL)
    message(FATAL_ERROR "SOURCE_LABEL is required")
endif()

set(ARPG_DUNGEON_INCLUDE_REGEX
    [=[^[ \t]*#[ \t]*include[ \t]*[<"]dungeon[/\\][^>"]+[>"]]=])

function(arpg_line_has_dungeon_include INPUT_LINE OUT_FOUND)
    string(TOLOWER "${INPUT_LINE}" _arpg_line_lower)
    if(_arpg_line_lower MATCHES "${ARPG_DUNGEON_INCLUDE_REGEX}")
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    endif()
endfunction()

macro(arpg_finish_module_source_line)
    arpg_line_has_dungeon_include("${_arpg_line}" _arpg_line_found)
    if(_arpg_line_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
        return()
    endif()
    set(_arpg_line "")
endmacro()

function(arpg_source_has_dungeon_include SOURCE_TEXT OUT_FOUND OUT_LINE)
    set(_arpg_state CODE)
    set(_arpg_line "")
    string(LENGTH "${SOURCE_TEXT}" _arpg_source_length)
    set(_arpg_index 0)

    while(_arpg_index LESS _arpg_source_length)
        string(SUBSTRING "${SOURCE_TEXT}" ${_arpg_index} 1 _arpg_char)
        math(EXPR _arpg_next_index "${_arpg_index} + 1")
        set(_arpg_next_char "")
        if(_arpg_next_index LESS _arpg_source_length)
            string(SUBSTRING
                "${SOURCE_TEXT}" ${_arpg_next_index} 1 _arpg_next_char)
        endif()

        if(_arpg_state STREQUAL CODE)
            if(_arpg_char STREQUAL "/" AND _arpg_next_char STREQUAL "/")
                string(APPEND _arpg_line " ")
                set(_arpg_state LINE_COMMENT)
                math(EXPR _arpg_index "${_arpg_index} + 2")
                continue()
            endif()
            if(_arpg_char STREQUAL "/" AND _arpg_next_char STREQUAL "*")
                string(APPEND _arpg_line " ")
                set(_arpg_state BLOCK_COMMENT)
                math(EXPR _arpg_index "${_arpg_index} + 2")
                continue()
            endif()
            if(_arpg_char STREQUAL "\"")
                string(TOLOWER "${_arpg_line}" _arpg_line_lower)
                if(_arpg_line_lower MATCHES
                        "^[ \t]*#[ \t]*include[ \t]*$")
                    string(APPEND _arpg_line "\"")
                    set(_arpg_state INCLUDE_STRING)
                else()
                    string(APPEND _arpg_line " ")
                    set(_arpg_state STRING)
                endif()
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(_arpg_char STREQUAL "'")
                string(APPEND _arpg_line " ")
                set(_arpg_state CHAR)
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_finish_module_source_line()
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(NOT _arpg_char STREQUAL "\r")
                string(APPEND _arpg_line "${_arpg_char}")
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL LINE_COMMENT)
            if(_arpg_char STREQUAL "\n")
                arpg_finish_module_source_line()
                set(_arpg_state CODE)
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL BLOCK_COMMENT)
            if(_arpg_char STREQUAL "*" AND _arpg_next_char STREQUAL "/")
                set(_arpg_state CODE)
                math(EXPR _arpg_index "${_arpg_index} + 2")
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_finish_module_source_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL STRING OR _arpg_state STREQUAL CHAR)
            if(_arpg_char STREQUAL "\\")
                math(EXPR _arpg_index "${_arpg_index} + 2")
                continue()
            endif()
            if((_arpg_state STREQUAL STRING AND _arpg_char STREQUAL "\"")
                    OR (_arpg_state STREQUAL CHAR AND _arpg_char STREQUAL "'"))
                set(_arpg_state CODE)
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_finish_module_source_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL INCLUDE_STRING)
            string(APPEND _arpg_line "${_arpg_char}")
            if(_arpg_char STREQUAL "\"")
                set(_arpg_state CODE)
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_finish_module_source_line()
                set(_arpg_state CODE)
            endif()
            set(_arpg_index ${_arpg_next_index})
        endif()
    endwhile()

    arpg_line_has_dungeon_include("${_arpg_line}" _arpg_line_found)
    if(_arpg_line_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
        return()
    endif()
    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_LINE}" "" PARENT_SCOPE)
endfunction()

function(arpg_expect_module_boundary LABEL EXPECTED_FOUND SOURCE_TEXT)
    arpg_source_has_dungeon_include(
        "${SOURCE_TEXT}" _arpg_found _arpg_line)
    if(EXPECTED_FOUND AND NOT _arpg_found)
        set_property(GLOBAL APPEND PROPERTY
            ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES "missed:${LABEL}")
    endif()
    if(NOT EXPECTED_FOUND AND _arpg_found)
        set_property(GLOBAL APPEND PROPERTY
            ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES "false-positive:${LABEL}")
    endif()
endfunction()

set_property(GLOBAL PROPERTY ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES "")
arpg_expect_module_boundary(
    "real angle include" TRUE [=[#include <dungeon/dungeon_types.hpp>]=])
arpg_expect_module_boundary(
    "real quoted include" TRUE [=[#include "dungeon/dungeon_session.hpp"]=])
arpg_expect_module_boundary(
    "line comment" FALSE [=[// #include <dungeon/dungeon_types.hpp>]=])
arpg_expect_module_boundary(
    "block comment" FALSE [=[/*
#include "dungeon/dungeon_session.hpp"
*/]=])
arpg_expect_module_boundary(
    "ordinary string" FALSE
    [=[const char* text = "#include <dungeon/dungeon_types.hpp>";]=])
arpg_expect_module_boundary(
    "unrelated include" FALSE [=[#include <combat/combat_types.hpp>]=])

get_property(ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES
    GLOBAL PROPERTY ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES)
if(ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES)
    string(JOIN ", " ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY
        ${ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURES})
    message(FATAL_ERROR
        "Module source scanner self-tests failed: ${ARPG_MODULE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY}")
endif()

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/*.h"
    "${SOURCE_DIR}/*.hh"
    "${SOURCE_DIR}/*.hpp"
    "${SOURCE_DIR}/*.hxx"
    "${SOURCE_DIR}/*.inl"
    "${SOURCE_DIR}/*.inc"
    "${SOURCE_DIR}/*.ipp"
    "${SOURCE_DIR}/*.tpp"
    "${SOURCE_DIR}/*.tcc"
    "${SOURCE_DIR}/*.c"
    "${SOURCE_DIR}/*.cc"
    "${SOURCE_DIR}/*.cpp"
    "${SOURCE_DIR}/*.cxx"
    "${SOURCE_DIR}/*.ixx"
    "${SOURCE_DIR}/*.cppm"
    "${SOURCE_DIR}/*.mpp")

foreach(SOURCE_FILE IN LISTS SOURCE_FILES)
    file(READ "${SOURCE_FILE}" SOURCE_CONTENT)
    arpg_source_has_dungeon_include(
        "${SOURCE_CONTENT}" SOURCE_HAS_DUNGEON_INCLUDE SOURCE_DUNGEON_LINE)
    if(SOURCE_HAS_DUNGEON_INCLUDE)
        message(FATAL_ERROR
            "${SOURCE_LABEL} file depends on Dungeon: ${SOURCE_FILE}: ${SOURCE_DUNGEON_LINE}")
    endif()
endforeach()
