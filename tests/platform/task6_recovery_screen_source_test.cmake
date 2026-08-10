cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(host_source "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_SOURCE_OVERRIDE)
    set(host_source "${HOST_SOURCE_OVERRIDE}")
endif()
if(NOT EXISTS "${host_source}")
    message(FATAL_ERROR "recovery screen host source is missing: ${host_source}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
file(READ "${host_source}" host_text)
extract_cpp_function_body("${host_text}" "void draw_recovery_screen("
    recovery_draw_body recovery_draw_code)

foreach(required IN ITEMS
        "const RecoveryScreenView view = ::arpg::platform::make_recovery_screen_view(status,"
        "DrawRectangleRounded(view.panel"
        "draw_crisp_ui_text(font, text")
    string(FIND "${recovery_draw_body}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Task 6 recovery draw seam is missing: ${required}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "DrawText" "TextFormat" "GetFontDefault" "LoadFont")
    string(FIND "${recovery_draw_code}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Task 6 recovery draw has a forbidden text/font path: ${forbidden}")
    endif()
endforeach()

set(recovery_branch_marker
    "if (runtime.state() == DungeonRuntimeState::recovery_required)")
set(recovery_observe_marker
    "renderer.observe_presented_hud_frame(HudPresentedFrame::recovery,")
set(recovery_draw_marker
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());")
set(recovery_present_marker
    "static_cast<void>(present_frame_and_maybe_capture(")
string(FIND "${host_text}" "${recovery_branch_marker}" recovery_branch_start)
if(recovery_branch_start EQUAL -1)
    message(FATAL_ERROR "Task 6 recovery branch is missing")
endif()
string(SUBSTRING "${host_text}" ${recovery_branch_start} -1 recovery_branch)
foreach(marker_name IN ITEMS recovery_observe_marker recovery_draw_marker recovery_present_marker)
    string(FIND "${recovery_branch}" "${${marker_name}}" ${marker_name}_index)
    if(${marker_name}_index EQUAL -1)
        message(FATAL_ERROR "Task 6 recovery branch is missing: ${${marker_name}}")
    endif()
endforeach()
if(NOT recovery_observe_marker_index LESS recovery_draw_marker_index
        OR NOT recovery_draw_marker_index LESS recovery_present_marker_index)
    message(FATAL_ERROR
        "Task 6 recovery presentation must observe, draw with SharedUiFont, then present/capture")
endif()

message(STATUS "Task 6 recovery source guard accepted")
