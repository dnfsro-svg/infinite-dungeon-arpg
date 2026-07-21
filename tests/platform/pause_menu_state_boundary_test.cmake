cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PAUSE_STATE_HEADER OR NOT DEFINED PAUSE_STATE_SOURCE)
    message(FATAL_ERROR "Pause state source paths are required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

function(arpg_pause_preprocessor_is_allowed SANITIZED_SOURCE OUT_ALLOWED OUT_BAD_LINE)
    string(REPLACE "\n" ";" _arpg_lines "${SANITIZED_SOURCE}")
    foreach(_arpg_line IN LISTS _arpg_lines)
        if(_arpg_line MATCHES "^[ \t]*#")
            if(_arpg_line MATCHES "^[ \t]*#[ \t]*pragma[ \t]+once[ \t]*$")
                continue()
            endif()
            if(_arpg_line MATCHES
                    "^[ \t]*#[ \t]*include[ \t]*(\"pause_menu_state\\.hpp\"|\"platform/settings/settings_types\\.hpp\"|<cstddef>|<cstdint>|<optional>|<limits>)[ \t]*$")
                continue()
            endif()
            set("${OUT_ALLOWED}" FALSE PARENT_SCOPE)
            set("${OUT_BAD_LINE}" "${_arpg_line}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set("${OUT_ALLOWED}" TRUE PARENT_SCOPE)
    set("${OUT_BAD_LINE}" "" PARENT_SCOPE)
endfunction()

function(arpg_pause_has_forbidden_api SANITIZED_SOURCE OUT_FOUND OUT_LABEL)
    set(_arpg_api_checks
        "raylib device API|(^|[^A-Za-z0-9_])(IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV)[ \t\r\n]*\\("
        "raylib window API|(^|[^A-Za-z0-9_])(IsWindowFocused|IsWindowState|SetWindowState|ClearWindowState|ToggleFullscreen|ToggleBorderlessWindowed|SetWindowSize|SetWindowPosition|SetWindowMinSize|SetWindowMaxSize|SetWindowTitle|SetWindowMonitor|SetTargetFPS|SetMasterVolume|CloseWindow|WindowShouldClose)[ \t\r\n]*\\("
        "SettingsStore type|(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)"
        "settings_store token|(^|[^A-Za-z0-9_])settings_store([^A-Za-z0-9_]|$)"
        "combat namespace|(^|[^A-Za-z0-9_])combat[ \t\r\n]*::"
        "dungeon namespace|(^|[^A-Za-z0-9_])dungeon[ \t\r\n]*::"
        "filesystem namespace|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?filesystem[ \t\r\n]*::"
        "C++ stream type|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?(fstream|ifstream|ofstream)([^A-Za-z0-9_]|$)"
        "C FILE type|(^|[^A-Za-z0-9_])FILE([^A-Za-z0-9_]|$)"
        "C file API|(^|[^A-Za-z0-9_])(fopen|fread|fwrite|fclose)[ \t\r\n]*\\("
        "Windows file API|(^|[^A-Za-z0-9_])(CreateFile|ReadFile|WriteFile|MoveFile|MoveFileEx|ReplaceFile|DeleteFile|FlushFileBuffers)(A|W)?[ \t\r\n]*\\(")
    foreach(_arpg_check IN LISTS _arpg_api_checks)
        string(FIND "${_arpg_check}" "|" _arpg_separator)
        string(SUBSTRING "${_arpg_check}" 0 ${_arpg_separator} _arpg_label)
        math(EXPR _arpg_pattern_start "${_arpg_separator} + 1")
        string(SUBSTRING "${_arpg_check}" ${_arpg_pattern_start} -1 _arpg_pattern)
        if(SANITIZED_SOURCE MATCHES "${_arpg_pattern}")
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_LABEL}" "${_arpg_label}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_LABEL}" "" PARENT_SCOPE)
endfunction()

function(arpg_pause_source_has_violation SOURCE_TEXT OUT_FOUND OUT_REASON)
    arpg_sanitize_cpp_source("${SOURCE_TEXT}" _arpg_sanitized)
    arpg_pause_preprocessor_is_allowed(
        "${_arpg_sanitized}" _arpg_preprocessor_allowed _arpg_bad_line)
    if(NOT _arpg_preprocessor_allowed)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_REASON}" "preprocessor:${_arpg_bad_line}" PARENT_SCOPE)
        return()
    endif()
    arpg_pause_has_forbidden_api(
        "${_arpg_sanitized}" _arpg_api_found _arpg_api_label)
    if(_arpg_api_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_REASON}" "api:${_arpg_api_label}" PARENT_SCOPE)
        return()
    endif()
    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_REASON}" "" PARENT_SCOPE)
endfunction()

function(arpg_expect_pause_boundary LABEL EXPECTED_VIOLATION SOURCE_TEXT)
    arpg_pause_source_has_violation(
        "${SOURCE_TEXT}" _arpg_found _arpg_reason)
    if(EXPECTED_VIOLATION AND NOT _arpg_found)
        set_property(
            GLOBAL APPEND PROPERTY
            ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES "missed:${LABEL}")
    endif()
    if(NOT EXPECTED_VIOLATION AND _arpg_found)
        set_property(
            GLOBAL APPEND PROPERTY
            ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES
            "false-positive:${LABEL}:${_arpg_reason}")
    endif()
endfunction()

set_property(GLOBAL PROPERTY ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES "")

arpg_expect_pause_boundary(
    "pragma once" FALSE [=[#pragma once]=])
arpg_expect_pause_boundary(
    "own include" FALSE [=[#include "pause_menu_state.hpp"]=])
arpg_expect_pause_boundary(
    "settings include" FALSE
    [=[ # include "platform/settings/settings_types.hpp"]=])
arpg_expect_pause_boundary(
    "standard include" FALSE [=[#include <cstddef>]=])
arpg_expect_pause_boundary(
    "comment-separated allowed include" FALSE
    [=[#/**/include/**/<cstdint>]=])

arpg_expect_pause_boundary(
    "raylib include" TRUE [=[#include <raylib.h>]=])
arpg_expect_pause_boundary(
    "combat include" TRUE [=[#include "combat/combat.hpp"]=])
arpg_expect_pause_boundary(
    "dungeon include" TRUE [=[#include "dungeon/dungeon.hpp"]=])
arpg_expect_pause_boundary(
    "filesystem include" TRUE [=[#include <filesystem>]=])
arpg_expect_pause_boundary(
    "fstream include" TRUE [=[#include <fstream>]=])
arpg_expect_pause_boundary(
    "cstdio include" TRUE [=[#include <cstdio>]=])
arpg_expect_pause_boundary(
    "windows include" TRUE [=[#include <windows.h>]=])
arpg_expect_pause_boundary(
    "comment-separated forbidden include" TRUE
    [=[#/**/include/**/<raylib.h>]=])
arpg_expect_pause_boundary(
    "continued directive" TRUE [=[#inc\
lude <raylib.h>]=])
arpg_expect_pause_boundary(
    "continued header" TRUE [=[#include \
<raylib.h>]=])
string(ASCII 13 ARPG_PAUSE_BOUNDARY_CR)
set(ARPG_PAUSE_BOUNDARY_CRLF_INCLUDE
    "#include \\${ARPG_PAUSE_BOUNDARY_CR}\n<raylib.h>")
arpg_expect_pause_boundary(
    "CRLF continued header" TRUE
    "${ARPG_PAUSE_BOUNDARY_CRLF_INCLUDE}")
arpg_expect_pause_boundary(
    "macro angle header" TRUE [=[#define RL_HEADER <raylib.h>
#include RL_HEADER]=])
arpg_expect_pause_boundary(
    "macro quoted header" TRUE [=[#define RL_HEADER "raylib.h"
#include RL_HEADER]=])
arpg_expect_pause_boundary(
    "macro include" TRUE [=[#include RL_HEADER]=])
arpg_expect_pause_boundary(
    "unapproved define" TRUE [=[#define PAUSE_STATE_INTERNAL 1]=])
arpg_expect_pause_boundary(
    "unapproved pragma" TRUE [=[#pragma message("pause")]=])

arpg_expect_pause_boundary(
    "raylib device API" TRUE [=[IsKeyPressed(1);]=])
arpg_expect_pause_boundary(
    "raylib window API" TRUE [=[SetWindowState(1);]=])
arpg_expect_pause_boundary(
    "SettingsStore type" TRUE [=[SettingsStore* store;]=])
arpg_expect_pause_boundary(
    "settings_store token" TRUE [=[auto settings_store = 0;]=])
arpg_expect_pause_boundary(
    "combat namespace" TRUE [=[combat::CombatState state;]=])
arpg_expect_pause_boundary(
    "dungeon namespace" TRUE [=[dungeon::DungeonSession session;]=])
arpg_expect_pause_boundary(
    "filesystem namespace" TRUE [=[std::filesystem::path path;]=])
arpg_expect_pause_boundary(
    "C++ stream" TRUE [=[std::ifstream input;]=])
arpg_expect_pause_boundary(
    "C FILE" TRUE [=[FILE* handle;]=])
arpg_expect_pause_boundary(
    "C file API" TRUE [=[fopen(path, mode);]=])
arpg_expect_pause_boundary(
    "Windows file API" TRUE [=[CreateFileW(path, 0, 0, 0, 0, 0, 0);]=])
arpg_expect_pause_boundary(
    "spliced API token" TRUE [=[IsKey\
Pressed(1);]=])
set(ARPG_PAUSE_BOUNDARY_CRLF_API
    "IsKey\\${ARPG_PAUSE_BOUNDARY_CR}\nPressed(1);")
arpg_expect_pause_boundary(
    "CRLF spliced API token" TRUE "${ARPG_PAUSE_BOUNDARY_CRLF_API}")

arpg_expect_pause_boundary(
    "line-comment API" FALSE [=[// IsKeyPressed(1)]=])
arpg_expect_pause_boundary(
    "ordinary-string SettingsStore" FALSE
    [=[constexpr char text[] = "SettingsStore";]=])
arpg_expect_pause_boundary(
    "line-comment FILE" FALSE [=[// FILE is forbidden]=])
arpg_expect_pause_boundary(
    "ordinary-string fopen" FALSE
    [=[constexpr char text[] = "fopen(path, mode)";]=])
arpg_expect_pause_boundary(
    "block-comment APIs" FALSE
    [=[/* IsKeyPressed(1); SettingsStore; FILE* f; fopen(path, mode); */]=])
arpg_expect_pause_boundary(
    "raw-string APIs" FALSE
    [=[constexpr auto text = R"apis(
IsKeyPressed(1); SettingsStore; FILE* f; fopen(path, mode);
)apis";]=])
arpg_expect_pause_boundary(
    "line-comment include" FALSE [=[// #include <raylib.h>]=])
arpg_expect_pause_boundary(
    "block-comment include" FALSE [=[/* #include <raylib.h> */]=])
arpg_expect_pause_boundary(
    "raw-string include" FALSE
    [=[constexpr auto text = R"include(#include <raylib.h>)include";]=])
arpg_expect_pause_boundary(
    "ordinary architecture words" FALSE
    [=[constexpr char text[] = "raylib combat dungeon file";]=])
arpg_expect_pause_boundary(
    "similar identifiers" FALSE
    [=[int raylibish_combatant_dungeoned_file_id;
SettingsStorefront settings_storefront;
auto IsKeyPressedMessage_SetWindowStateCache = 0;]=])

get_property(
    ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES
    GLOBAL PROPERTY ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES)
if(ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES)
    string(JOIN
        ", " ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY
        ${ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES})
    message(FATAL_ERROR
        "Pause boundary self-tests failed: ${ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY}")
endif()

foreach(_arpg_source IN ITEMS "${PAUSE_STATE_HEADER}" "${PAUSE_STATE_SOURCE}")
    if(NOT EXISTS "${_arpg_source}")
        message(FATAL_ERROR "Pause state source missing: ${_arpg_source}")
    endif()
    file(READ "${_arpg_source}" _arpg_source_text)
    arpg_pause_source_has_violation(
        "${_arpg_source_text}" _arpg_found _arpg_reason)
    if(_arpg_found)
        message(FATAL_ERROR
            "Pause state source boundary violation '${_arpg_reason}': ${_arpg_source}")
    endif()
endforeach()
