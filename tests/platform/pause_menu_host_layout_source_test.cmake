if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" HOST_SOURCE)
file(READ "${SOURCE_ROOT}/src/platform/raylib/pause_menu_renderer.cpp"
    RENDERER_SOURCE)

string(REGEX REPLACE "[ \t\r\n]+" "" HOST_COMPACT "${HOST_SOURCE}")
string(REGEX REPLACE "[ \t\r\n]+" "" RENDERER_COMPACT
    "${RENDERER_SOURCE}")

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
