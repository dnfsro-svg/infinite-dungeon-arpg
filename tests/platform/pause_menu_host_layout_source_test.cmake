if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

set(_host_source "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(NOT EXISTS "${_host_source}")
    message(FATAL_ERROR "Raylib host source is missing: ${_host_source}")
endif()

file(READ "${_host_source}" _host_text)
arpg_sanitize_cpp_source("${_host_text}" _host_surface)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_surface "${_host_surface}")

set(_visible_layout_call
    "pause_menu_layout(GetScreenWidth(),GetScreenHeight(),pause_view.row_count)")
set(_hit_test_chain
    "constPauseMenuViewpause_view=make_pause_menu_view(pause_menu);constPauseMenuLayoutlayout=${_visible_layout_call};constautoselected=hit_test_pause_row(layout,physical_keys.mouse_position);")

function(pause_menu_host_layout_valid SOURCE OUT_VALID)
    string(FIND "${SOURCE}" "${_visible_layout_call}" _layout_position)
    set(_duplicate_layout_position -1)
    if(NOT _layout_position EQUAL -1)
        string(LENGTH "${_visible_layout_call}" _layout_length)
        math(EXPR _tail_begin
            "${_layout_position} + ${_layout_length}")
        string(SUBSTRING "${SOURCE}" ${_tail_begin} -1 _layout_tail)
        string(FIND "${_layout_tail}"
            "${_visible_layout_call}" _duplicate_layout_position)
    endif()
    string(FIND "${SOURCE}" "${_hit_test_chain}" _chain_position)
    if(NOT _layout_position EQUAL -1
            AND _duplicate_layout_position EQUAL -1
            AND NOT _chain_position EQUAL -1)
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    endif()
endfunction()

pause_menu_host_layout_valid("${_host_surface}" _live_valid)
if(NOT _live_valid)
    message(FATAL_ERROR
        "Pause-menu mouse hit-test must derive its layout from the current view.row_count")
endif()

string(REPLACE
    "${_visible_layout_call}"
    "pause_menu_layout(GetScreenWidth(),GetScreenHeight())"
    _full_layout_mutation "${_host_surface}")
if("${_full_layout_mutation}" STREQUAL "${_host_surface}")
    message(FATAL_ERROR "Pause-menu layout mutation anchor is missing")
endif()
pause_menu_host_layout_valid("${_full_layout_mutation}" _mutation_valid)
if(_mutation_valid)
    message(FATAL_ERROR
        "Pause-menu layout guard accepted a two-argument full-layout mutation")
endif()

message(STATUS "Pause-menu mouse hit-test uses the rendered visible-row layout")
