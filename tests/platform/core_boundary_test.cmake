if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR is required")
endif()

set(ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX
    [=[^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]*[/\\])?(raylib\.h|raymath\.h|rlgl\.h|raylib-cpp[^>"]*)[>"]]=])
set(ARPG_FORBIDDEN_SOURCE_MACRO_REGEX
    [=[^[ \t]*#[ \t]*define[ \t]+[a-z_][a-z0-9_]*[ \t]+[<"]([^>"]*[/\\])?(raylib\.h|raymath\.h|rlgl\.h|raylib-cpp[^>"]*)[>"]]=])

function(arpg_line_has_forbidden_include INPUT_LINE OUT_FOUND)
    string(TOLOWER "${INPUT_LINE}" _arpg_line_lower)
    if(_arpg_line_lower MATCHES
            "${ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX}"
            OR _arpg_line_lower MATCHES
            "${ARPG_FORBIDDEN_SOURCE_MACRO_REGEX}")
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    endif()
endfunction()

macro(arpg_finish_scanned_source_line)
    arpg_line_has_forbidden_include("${_arpg_line}" _arpg_line_found)
    if(_arpg_line_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
        return()
    endif()
    set(_arpg_line "")
endmacro()

function(arpg_source_has_forbidden_include SOURCE_TEXT OUT_FOUND OUT_LINE)
    set(_arpg_state CODE)
    set(_arpg_line "")
    set(_arpg_raw_closer "")
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

        math(EXPR _arpg_after_next_index "${_arpg_index} + 2")
        set(_arpg_after_next_char "")
        if(_arpg_after_next_index LESS _arpg_source_length)
            string(SUBSTRING
                "${SOURCE_TEXT}"
                ${_arpg_after_next_index}
                1
                _arpg_after_next_char)
        endif()

        set(_arpg_logical_next_index ${_arpg_next_index})
        set(_arpg_logical_next_char "${_arpg_next_char}")
        if(NOT _arpg_state STREQUAL RAW_STRING)
            set(_arpg_logical_next_char "")
            while(_arpg_logical_next_index LESS _arpg_source_length)
                string(SUBSTRING
                    "${SOURCE_TEXT}"
                    ${_arpg_logical_next_index}
                    1
                    _arpg_logical_candidate)
                if(_arpg_logical_candidate STREQUAL "\\")
                    math(EXPR
                        _arpg_splice_next_index
                        "${_arpg_logical_next_index} + 1")
                    set(_arpg_splice_next_char "")
                    if(_arpg_splice_next_index LESS _arpg_source_length)
                        string(SUBSTRING
                            "${SOURCE_TEXT}"
                            ${_arpg_splice_next_index}
                            1
                            _arpg_splice_next_char)
                    endif()
                    if(_arpg_splice_next_char STREQUAL "\n")
                        math(EXPR
                            _arpg_logical_next_index
                            "${_arpg_logical_next_index} + 2")
                        continue()
                    endif()

                    math(EXPR
                        _arpg_splice_after_index
                        "${_arpg_logical_next_index} + 2")
                    set(_arpg_splice_after_char "")
                    if(_arpg_splice_after_index LESS _arpg_source_length)
                        string(SUBSTRING
                            "${SOURCE_TEXT}"
                            ${_arpg_splice_after_index}
                            1
                            _arpg_splice_after_char)
                    endif()
                    if(_arpg_splice_next_char STREQUAL "\r"
                            AND _arpg_splice_after_char STREQUAL "\n")
                        math(EXPR
                            _arpg_logical_next_index
                            "${_arpg_logical_next_index} + 3")
                        continue()
                    endif()
                endif()

                set(_arpg_logical_next_char "${_arpg_logical_candidate}")
                break()
            endwhile()
        endif()

        if(NOT _arpg_state STREQUAL RAW_STRING
                AND _arpg_char STREQUAL "\\")
            if(_arpg_next_char STREQUAL "\n")
                math(EXPR _arpg_index "${_arpg_index} + 2")
                continue()
            endif()
            if(_arpg_next_char STREQUAL "\r"
                    AND _arpg_after_next_char STREQUAL "\n")
                math(EXPR _arpg_index "${_arpg_index} + 3")
                continue()
            endif()
        endif()

        if(_arpg_state STREQUAL CODE)
            if(_arpg_char STREQUAL "/"
                    AND _arpg_logical_next_char STREQUAL "/")
                string(APPEND _arpg_line " ")
                set(_arpg_state LINE_COMMENT)
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()
            if(_arpg_char STREQUAL "/"
                    AND _arpg_logical_next_char STREQUAL "*")
                string(APPEND _arpg_line " ")
                set(_arpg_state BLOCK_COMMENT)
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()

            if(_arpg_char STREQUAL "R"
                    AND _arpg_logical_next_char STREQUAL "\"")
                math(EXPR
                    _arpg_delimiter_index
                    "${_arpg_logical_next_index} + 1")
                set(_arpg_raw_delimiter "")
                set(_arpg_valid_raw_opener FALSE)
                while(_arpg_delimiter_index LESS _arpg_source_length)
                    string(SUBSTRING
                        "${SOURCE_TEXT}"
                        ${_arpg_delimiter_index}
                        1
                        _arpg_delimiter_char)
                    if(_arpg_delimiter_char STREQUAL "\\")
                        math(EXPR
                            _arpg_delimiter_next_index
                            "${_arpg_delimiter_index} + 1")
                        set(_arpg_delimiter_next_char "")
                        if(_arpg_delimiter_next_index LESS _arpg_source_length)
                            string(SUBSTRING
                                "${SOURCE_TEXT}"
                                ${_arpg_delimiter_next_index}
                                1
                                _arpg_delimiter_next_char)
                        endif()
                        if(_arpg_delimiter_next_char STREQUAL "\n")
                            math(EXPR
                                _arpg_delimiter_index
                                "${_arpg_delimiter_index} + 2")
                            continue()
                        endif()

                        math(EXPR
                            _arpg_delimiter_after_index
                            "${_arpg_delimiter_index} + 2")
                        set(_arpg_delimiter_after_char "")
                        if(_arpg_delimiter_after_index LESS _arpg_source_length)
                            string(SUBSTRING
                                "${SOURCE_TEXT}"
                                ${_arpg_delimiter_after_index}
                                1
                                _arpg_delimiter_after_char)
                        endif()
                        if(_arpg_delimiter_next_char STREQUAL "\r"
                                AND _arpg_delimiter_after_char STREQUAL "\n")
                            math(EXPR
                                _arpg_delimiter_index
                                "${_arpg_delimiter_index} + 3")
                            continue()
                        endif()
                    endif()
                    if(_arpg_delimiter_char STREQUAL "(")
                        string(LENGTH
                            "${_arpg_raw_delimiter}" _arpg_delimiter_length)
                        if(_arpg_delimiter_length LESS_EQUAL 16)
                            set(_arpg_valid_raw_opener TRUE)
                        endif()
                        break()
                    endif()
                    if(_arpg_delimiter_char STREQUAL "\n"
                            OR _arpg_delimiter_char STREQUAL "\r")
                        break()
                    endif()
                    string(APPEND
                        _arpg_raw_delimiter "${_arpg_delimiter_char}")
                    string(LENGTH
                        "${_arpg_raw_delimiter}" _arpg_delimiter_length)
                    if(_arpg_delimiter_length GREATER 16)
                        break()
                    endif()
                    math(EXPR
                        _arpg_delimiter_index
                        "${_arpg_delimiter_index} + 1")
                endwhile()

                if(_arpg_valid_raw_opener)
                    string(APPEND _arpg_line " ")
                    set(_arpg_raw_closer ")${_arpg_raw_delimiter}\"")
                    set(_arpg_state RAW_STRING)
                    math(EXPR _arpg_index "${_arpg_delimiter_index} + 1")
                    continue()
                endif()
            endif()

            if(_arpg_char STREQUAL "\"")
                string(TOLOWER "${_arpg_line}" _arpg_line_lower)
                if(_arpg_line_lower MATCHES
                        "^[ \t]*#[ \t]*(include[ \t]*|define[ \t]+[a-z_][a-z0-9_]*[ \t]+)$")
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
                arpg_finish_scanned_source_line()
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
                arpg_finish_scanned_source_line()
                set(_arpg_state CODE)
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL BLOCK_COMMENT)
            if(_arpg_char STREQUAL "*"
                    AND _arpg_logical_next_char STREQUAL "/")
                set(_arpg_state CODE)
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_finish_scanned_source_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL RAW_STRING)
            string(LENGTH "${_arpg_raw_closer}" _arpg_closer_length)
            math(EXPR
                _arpg_remaining_length
                "${_arpg_source_length} - ${_arpg_index}")
            if(_arpg_remaining_length GREATER_EQUAL _arpg_closer_length)
                string(SUBSTRING
                    "${SOURCE_TEXT}"
                    ${_arpg_index}
                    ${_arpg_closer_length}
                    _arpg_closer_candidate)
                if(_arpg_closer_candidate STREQUAL _arpg_raw_closer)
                    set(_arpg_state CODE)
                    math(EXPR
                        _arpg_index
                        "${_arpg_index} + ${_arpg_closer_length}")
                    continue()
                endif()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_finish_scanned_source_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL STRING OR _arpg_state STREQUAL CHAR)
            if(_arpg_char STREQUAL "\\")
                if(_arpg_next_char STREQUAL "\n")
                    arpg_finish_scanned_source_line()
                endif()
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
                arpg_finish_scanned_source_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL INCLUDE_STRING)
            if(_arpg_char STREQUAL "\n")
                arpg_finish_scanned_source_line()
                set(_arpg_state CODE)
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            string(APPEND _arpg_line "${_arpg_char}")
            if(_arpg_char STREQUAL "\"")
                set(_arpg_state CODE)
            endif()
            set(_arpg_index ${_arpg_next_index})
        endif()
    endwhile()

    arpg_line_has_forbidden_include("${_arpg_line}" _arpg_line_found)
    if(_arpg_line_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
        return()
    endif()
    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_LINE}" "" PARENT_SCOPE)
endfunction()

function(arpg_expect_source_boundary LABEL EXPECTED_FOUND SOURCE_TEXT)
    arpg_source_has_forbidden_include(
        "${SOURCE_TEXT}" _arpg_found _arpg_line)
    if(EXPECTED_FOUND AND NOT _arpg_found)
        set_property(
            GLOBAL APPEND PROPERTY
            ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES "missed:${LABEL}")
    endif()
    if(NOT EXPECTED_FOUND AND _arpg_found)
        set_property(
            GLOBAL APPEND PROPERTY
            ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES "false-positive:${LABEL}")
    endif()
endfunction()

set_property(GLOBAL PROPERTY ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES "")

arpg_expect_source_boundary(
    "real angle include" TRUE [=[#include <raylib.h>]=])
arpg_expect_source_boundary(
    "real quoted include" TRUE [=[  # include "vendor/raymath.h"]=])
arpg_expect_source_boundary(
    "comment-separated directive" TRUE [=[#/**/include <rlgl.h>]=])
arpg_expect_source_boundary(
    "comment before header" TRUE [=[#include/**/<raylib.h>]=])
arpg_expect_source_boundary(
    "block comment opener across splice" TRUE [=[/\
*comment*/ #include <raylib.h>]=])
arpg_expect_source_boundary(
    "block comment closer across splice" TRUE [=[/*comment*\
/ #include <raymath.h>]=])
arpg_expect_source_boundary(
    "continued directive token" TRUE [=[#inc\
lude <raylib.h>]=])
arpg_expect_source_boundary(
    "continued header" TRUE [=[#include \
<raymath.h>]=])
arpg_expect_source_boundary(
    "macro angle header" TRUE [=[#define RL_HEADER <raylib.h>
#include RL_HEADER]=])
arpg_expect_source_boundary(
    "macro quoted header" TRUE [=[#define RL_HEADER "raymath.h"
#include RL_HEADER]=])

arpg_expect_source_boundary(
    "line comment" FALSE [=[// #include <raylib.h>]=])
arpg_expect_source_boundary(
    "block comment" FALSE [=[/*
#include <raymath.h>
*/]=])
arpg_expect_source_boundary(
    "ordinary string" FALSE
    [=[const char* text = "#include <rlgl.h>";]=])
arpg_expect_source_boundary(
    "character literal" FALSE
    [=[const char hash = '#'; // #include <raylib.h>]=])
arpg_expect_source_boundary(
    "continued line comment" FALSE [=[// harmless \
#include <raylib.h>]=])
arpg_expect_source_boundary(
    "custom-delimiter raw string" FALSE
    [=[constexpr auto text = R"arpg_raw(
#include <raylib.h>
)arpg_raw";]=])
arpg_expect_source_boundary(
    "raw string preserves splice-shaped comment tokens" FALSE
    [=[constexpr auto text = R"splice_raw(
/\
*comment*/ #include <raylib.h>
/*comment*\
/ #include <raymath.h>
)splice_raw";]=])
arpg_expect_source_boundary(
    "unrelated include" FALSE [=[#include <array>]=])
arpg_expect_source_boundary(
    "similar header" FALSE [=[#include <my_raylib.h>]=])

get_property(
    ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES
    GLOBAL PROPERTY ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES)
if(ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES)
    string(JOIN
        ", " ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY
        ${ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURES})
    message(FATAL_ERROR
        "Source lexer self-tests failed: ${ARPG_SOURCE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY}")
endif()

file(GLOB_RECURSE CORE_FILES
    LIST_DIRECTORIES FALSE
    "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.hh"
    "${CORE_DIR}/*.hpp"
    "${CORE_DIR}/*.hxx"
    "${CORE_DIR}/*.inl"
    "${CORE_DIR}/*.inc"
    "${CORE_DIR}/*.ipp"
    "${CORE_DIR}/*.tpp"
    "${CORE_DIR}/*.tcc"
    "${CORE_DIR}/*.c"
    "${CORE_DIR}/*.cc"
    "${CORE_DIR}/*.cpp"
    "${CORE_DIR}/*.cxx"
    "${CORE_DIR}/*.ixx"
    "${CORE_DIR}/*.cppm"
    "${CORE_DIR}/*.mpp")

foreach(CORE_FILE IN LISTS CORE_FILES)
    file(READ "${CORE_FILE}" CORE_CONTENT)
    arpg_source_has_forbidden_include(
        "${CORE_CONTENT}" CORE_HAS_FORBIDDEN_INCLUDE CORE_FORBIDDEN_LINE)
    if(CORE_HAS_FORBIDDEN_INCLUDE)
        message(FATAL_ERROR
            "Core file depends on raylib: ${CORE_FILE}: ${CORE_FORBIDDEN_LINE}")
    endif()
endforeach()
