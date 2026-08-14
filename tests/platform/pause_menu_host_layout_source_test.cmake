if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" HOST_SOURCE)
file(READ "${SOURCE_ROOT}/src/platform/raylib/pause_menu_renderer.cpp"
    RENDERER_SOURCE)
arpg_sanitize_cpp_source("${HOST_SOURCE}" HOST_CODE)
arpg_sanitize_cpp_source("${RENDERER_SOURCE}" RENDERER_CODE)

string(REGEX REPLACE "[ \t\r\n]+" "" HOST_COMPACT "${HOST_CODE}")
string(REGEX REPLACE "[ \t\r\n]+" "" RENDERER_COMPACT
    "${RENDERER_CODE}")

set(HOST_VIEW_SOURCE
    "constPauseMenuViewpause_view=make_pause_menu_view(pause_menu);")
string(FIND "${HOST_COMPACT}" "${HOST_VIEW_SOURCE}" HOST_VIEW_INDEX)
if(HOST_VIEW_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host mouse input must derive its row count from the current view")
endif()

set(HOST_LAYOUT_SOURCE
    "pause_menu_layout(GetScreenWidth(),GetScreenHeight(),pause_view.row_count)")
string(FIND "${HOST_COMPACT}" "${HOST_LAYOUT_SOURCE}" HOST_LAYOUT_INDEX)
if(HOST_LAYOUT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host mouse hit testing must use the three-argument row-count layout")
endif()

set(HOST_HIT_TEST_SOURCE
    "hit_test_pause_row(layout,physical_keys.mouse_position)")
string(FIND "${HOST_COMPACT}" "${HOST_HIT_TEST_SOURCE}" HOST_HIT_TEST_INDEX)
if(HOST_HIT_TEST_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host mouse hit testing must consume that shared layout")
endif()

set(HOST_HIT_TEST_CHAIN
    "${HOST_VIEW_SOURCE}constPauseMenuLayoutlayout=${HOST_LAYOUT_SOURCE};constautoselected=${HOST_HIT_TEST_SOURCE};")
string(FIND "${HOST_COMPACT}" "${HOST_HIT_TEST_CHAIN}"
    HOST_HIT_TEST_CHAIN_INDEX)
if(HOST_HIT_TEST_CHAIN_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host must build the current view, derive its visible-row layout, and hit-test that same layout in one chain")
endif()

set(HOST_POINTER_HIT_DEFAULT_SOURCE
    "boolpause_pointer_row_hit=false;")
string(FIND "${HOST_COMPACT}" "${HOST_POINTER_HIT_DEFAULT_SOURCE}"
    HOST_POINTER_HIT_DEFAULT_INDEX)
if(HOST_POINTER_HIT_DEFAULT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host must default blank-area clicks to no visible-row hit")
endif()

set(HOST_POINTER_HIT_SOURCE
    "pause_pointer_row_hit=selected.has_value();")
string(FIND "${HOST_COMPACT}" "${HOST_POINTER_HIT_SOURCE}"
    HOST_POINTER_HIT_INDEX)
if(HOST_POINTER_HIT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host must preserve whether the pointer hit a visible row")
endif()

set(HOST_ACTIVATE_GATE_SOURCE
    "pause_input.activate=pause_input.activate&&pause_pointer_row_hit;")
string(FIND "${HOST_COMPACT}" "${HOST_ACTIVATE_GATE_SOURCE}"
    HOST_ACTIVATE_GATE_INDEX)
if(HOST_ACTIVATE_GATE_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host must not activate the selected command from a blank-area click")
endif()

set(HOST_UPDATE_SOURCE
    "constPauseCommandpause_command=update_pause_menu(pause_menu,pause_context,pause_input);")
string(FIND "${HOST_COMPACT}" "${HOST_UPDATE_SOURCE}" HOST_UPDATE_INDEX)
if(HOST_UPDATE_INDEX EQUAL -1
        OR NOT HOST_POINTER_HIT_DEFAULT_INDEX LESS HOST_HIT_TEST_INDEX
        OR NOT HOST_HIT_TEST_INDEX LESS HOST_POINTER_HIT_INDEX
        OR NOT HOST_POINTER_HIT_INDEX LESS HOST_ACTIVATE_GATE_INDEX
        OR NOT HOST_ACTIVATE_GATE_INDEX LESS HOST_UPDATE_INDEX)
    message(FATAL_ERROR
        "pause row hit must gate mouse activation before the pause state update")
endif()

string(LENGTH "${HOST_LAYOUT_SOURCE}" HOST_LAYOUT_SOURCE_LENGTH)
math(EXPR HOST_LAYOUT_TAIL_BEGIN
    "${HOST_LAYOUT_INDEX} + ${HOST_LAYOUT_SOURCE_LENGTH}")
string(SUBSTRING "${HOST_COMPACT}" ${HOST_LAYOUT_TAIL_BEGIN} -1
    HOST_LAYOUT_TAIL)
string(FIND "${HOST_LAYOUT_TAIL}" "${HOST_LAYOUT_SOURCE}"
    DUPLICATE_HOST_LAYOUT_INDEX)
if(NOT DUPLICATE_HOST_LAYOUT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host must have exactly one visible-row mouse hit-test layout")
endif()

string(REPLACE "${HOST_LAYOUT_SOURCE}"
    "pause_menu_layout(GetScreenWidth(),GetScreenHeight())"
    MUTATED_HOST_COMPACT "${HOST_COMPACT}")
string(FIND "${MUTATED_HOST_COMPACT}" "${HOST_HIT_TEST_CHAIN}"
    MUTATED_HOST_CHAIN_INDEX)
if(NOT MUTATED_HOST_CHAIN_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause host layout guard accepted a two-argument full-layout mutation")
endif()

set(RENDERER_VIEW_SOURCE
    "constPauseMenuViewview=make_pause_menu_view(state);")
string(FIND "${RENDERER_COMPACT}" "${RENDERER_VIEW_SOURCE}"
    RENDERER_VIEW_INDEX)
if(RENDERER_VIEW_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must derive its row count from the current view")
endif()

set(RENDERER_LAYOUT_SOURCE
    "pause_menu_layout(screen_width,screen_height,view.row_count)")
string(FIND "${RENDERER_COMPACT}" "${RENDERER_LAYOUT_SOURCE}"
    RENDERER_LAYOUT_INDEX)
if(RENDERER_LAYOUT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must use the three-argument row-count layout")
endif()

string(FIND "${RENDERER_COMPACT}"
    "DrawRectangleRounded(bounds,0.18F,5,style.backing);"
    FULL_ROW_BACKING_INDEX)
if(FULL_ROW_BACKING_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must draw the dark backing across the full row bounds")
endif()

string(FIND "${RENDERER_COMPACT}" "bounds.x+14.0F*viewport_scale"
    LEGACY_LEFT_TEXT_INDEX)
if(NOT LEGACY_LEFT_TEXT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must not restore the legacy left-aligned row text")
endif()

string(FIND "${RENDERER_COMPACT}" "row_text_width+"
    LEGACY_TEXT_BACKING_INDEX)
if(NOT LEGACY_TEXT_BACKING_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must not size row backing from measured text width")
endif()

string(FIND "${RENDERER_COMPACT}"
    "fit_centered_pause_text_layout(font,row.primary.data(),"
    FITTED_PRIMARY_INDEX)
if(FITTED_PRIMARY_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must fit real primary text measurements to the row")
endif()

string(FIND "${RENDERER_COMPACT}"
    "draw_crisp_ui_text(font,fitted_primary,row_position,centered.font_size,centered.spacing,"
    FITTED_DRAW_INDEX)
if(FITTED_DRAW_INDEX EQUAL -1)
    message(FATAL_ERROR
        "pause renderer must draw the final fitted text with its final metrics")
endif()

message(STATUS
    "[CPLAY-031] pause host and renderer share row-count layout geometry")
