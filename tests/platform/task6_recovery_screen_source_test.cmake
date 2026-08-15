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

set(recovery_request_marker
    "recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,")
set(recovery_branch_marker
    "if (runtime.state() == DungeonRuntimeState::recovery_required\n                    || runtime.state() == DungeonRuntimeState::faulted)")
set(recovery_observe_marker
    "renderer.observe_presented_hud_frame(HudPresentedFrame::recovery,")
set(recovery_draw_marker
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());")
set(recovery_present_marker
    "static_cast<void>(present_frame_and_maybe_capture(")
set(gameplay_branch_marker
    "dungeon::DungeonSession* const session = runtime.session();")
string(FIND "${host_text}" "${recovery_branch_marker}" recovery_branch_start)
if(recovery_branch_start EQUAL -1)
    message(FATAL_ERROR "Task 6 recovery/faulted blocking branch is missing")
endif()
set(host_loop_marker "while (!exit_requested) {")
string(FIND "${host_text}" "${host_loop_marker}" host_loop_start)
if(host_loop_start EQUAL -1 OR NOT host_loop_start LESS recovery_branch_start)
    message(FATAL_ERROR "Task 6 Host exit loop is missing")
endif()
math(EXPR pre_blocking_length
    "${recovery_branch_start} - ${host_loop_start}")
string(SUBSTRING "${host_text}" ${host_loop_start}
    ${pre_blocking_length} pre_blocking_loop)
set(ready_auto_exit_marker
    "if (runtime.clean_shutdown_state() == CleanShutdownState::ready) {")
string(FIND "${pre_blocking_loop}" "${ready_auto_exit_marker}"
    ready_auto_exit_start)
string(FIND "${pre_blocking_loop}" "CleanShutdownState::faulted"
    faulted_auto_exit_start)
if(ready_auto_exit_start EQUAL -1 OR NOT faulted_auto_exit_start EQUAL -1)
    message(FATAL_ERROR
        "Task 6 Host may auto-exit only after a ready clean shutdown")
endif()
string(FIND "${host_text}" "${recovery_request_marker}" recovery_request_start)
if(recovery_request_start EQUAL -1
        OR NOT recovery_request_start LESS recovery_branch_start)
    message(FATAL_ERROR
        "Task 6 N recovery must stay exclusive to recovery_required")
endif()
string(FIND "${host_text}" "${gameplay_branch_marker}" gameplay_branch_start)
if(gameplay_branch_start EQUAL -1
        OR NOT recovery_branch_start LESS gameplay_branch_start)
    message(FATAL_ERROR
        "Task 6 recovery/faulted blocking branch must precede gameplay")
endif()
math(EXPR recovery_branch_length
    "${gameplay_branch_start} - ${recovery_branch_start}")
string(SUBSTRING "${host_text}" ${recovery_branch_start}
    ${recovery_branch_length} recovery_branch)
set(recovery_stage12_marker
    "if (recovery_blocked\n                        && (config.stage12_material_background_only\n                            || config.stage12_material_icons_only))")
set(recovery_capture_marker
    "std::optional<std::string> capture_path = recovery_blocked\n                    ? validation_capture_path() : std::nullopt;")
foreach(marker_name IN ITEMS recovery_stage12_marker recovery_capture_marker)
    string(FIND "${recovery_branch}" "${${marker_name}}" ${marker_name}_index)
    if(${marker_name}_index EQUAL -1)
        message(FATAL_ERROR
            "Task 6 faulted blocking branch leaked a recovery-only action: ${${marker_name}}")
    endif()
endforeach()
string(FIND "${recovery_branch}" "recovery_requested(" recovery_inside_branch)
if(NOT recovery_inside_branch EQUAL -1)
    message(FATAL_ERROR
        "Task 6 faulted blocking branch must not accept N recovery")
endif()
foreach(marker_name IN ITEMS recovery_observe_marker recovery_draw_marker recovery_present_marker)
    string(FIND "${recovery_branch}" "${${marker_name}}" ${marker_name}_index)
    if(${marker_name}_index EQUAL -1)
        message(FATAL_ERROR "Task 6 recovery branch is missing: ${${marker_name}}")
    endif()
endforeach()
foreach(required IN ITEMS
        "frame_input.keys.escape"
        "window_close_requested"
        "begin_clean_exit();")
    string(FIND "${recovery_branch}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Task 6 recovery/faulted blocking exit is missing: ${required}")
    endif()
endforeach()
if(NOT recovery_observe_marker_index LESS recovery_draw_marker_index
        OR NOT recovery_draw_marker_index LESS recovery_present_marker_index)
    message(FATAL_ERROR
        "Task 6 recovery presentation must observe, draw with SharedUiFont, then present/capture")
endif()

message(STATUS "Task 6 recovery source guard accepted")
