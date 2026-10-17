if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED SOURCE_LABEL)
    message(FATAL_ERROR "SOURCE_LABEL is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

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

function(arpg_source_has_forbidden_include SOURCE_TEXT OUT_FOUND OUT_LINE)
    arpg_sanitize_cpp_source("${SOURCE_TEXT}" _arpg_sanitized)
    string(REPLACE "\n" ";" _arpg_lines "${_arpg_sanitized}")
    foreach(_arpg_line IN LISTS _arpg_lines)
        arpg_line_has_forbidden_include("${_arpg_line}" _arpg_line_found)
        if(_arpg_line_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
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
string(ASCII 13 ARPG_SOURCE_BOUNDARY_CR)
set(ARPG_SOURCE_BOUNDARY_CRLF_INCLUDE
    "#inc\\${ARPG_SOURCE_BOUNDARY_CR}\nlude <raylib.h>")
arpg_expect_source_boundary(
    "CRLF continued directive token" TRUE
    "${ARPG_SOURCE_BOUNDARY_CRLF_INCLUDE}")
arpg_expect_source_boundary(
    "macro angle header" TRUE [=[#define RL_HEADER <raylib.h>
#include RL_HEADER]=])
arpg_expect_source_boundary(
    "macro quoted header" TRUE [=[#define RL_HEADER "raymath.h"
#include RL_HEADER]=])
arpg_expect_source_boundary(
    "string escape consumes logical quote across splice" TRUE
    [=[const char* text = "a\\
"b";
#include <raylib.h>]=])
arpg_expect_source_boundary(
    "character escape consumes logical quote across splice" TRUE
    [=[const char value = '\\
'';
#include <raymath.h>]=])

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
set(ARPG_SOURCE_BOUNDARY_CRLF_COMMENT
    "// harmless \\${ARPG_SOURCE_BOUNDARY_CR}\n#include <raylib.h>")
arpg_expect_source_boundary(
    "CRLF continued line comment" FALSE
    "${ARPG_SOURCE_BOUNDARY_CRLF_COMMENT}")
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
    arpg_source_has_forbidden_include(
        "${SOURCE_CONTENT}"
        SOURCE_HAS_FORBIDDEN_INCLUDE
        SOURCE_FORBIDDEN_LINE)
    if(SOURCE_HAS_FORBIDDEN_INCLUDE)
        message(FATAL_ERROR
            "${SOURCE_LABEL} file depends on raylib: ${SOURCE_FILE}: ${SOURCE_FORBIDDEN_LINE}")
    endif()
endforeach()
