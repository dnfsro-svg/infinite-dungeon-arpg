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

set(initialization_presentable_marker
    "if (!host_runtime_initialization_is_presentable(initialized,\n                runtime.state(), runtime.render_snapshot_storage() != nullptr,\n                runtime.render_status().error))")
set(legacy_initialization_gate
    "if (!initialized && runtime.state() != DungeonRuntimeState::recovery_required)")
string(FIND "${host_text}" "const bool initialized = runtime.initialize();"
    initialization_start)
string(FIND "${host_text}" "${initialization_presentable_marker}"
    initialization_presentable_start)
string(FIND "${host_text}" "${legacy_initialization_gate}"
    legacy_initialization_gate_start)
string(FIND "${host_text}" "if (!window.initialize(config, committed_settings))"
    window_initialization_start)
if(initialization_start EQUAL -1
        OR initialization_presentable_start EQUAL -1
        OR NOT legacy_initialization_gate_start EQUAL -1
        OR window_initialization_start EQUAL -1
        OR NOT initialization_start LESS initialization_presentable_start
        OR NOT initialization_presentable_start LESS window_initialization_start)
    message(FATAL_ERROR
        "Task 6 initialization fault presentation must call the live presentable helper before window initialization")
endif()

extract_cpp_function_body("${host_text}"
    "bool host_directory_busy_retry_requested("
    directory_busy_gate_body directory_busy_gate_code)
set(required_directory_busy_gate
    "return state == DungeonRuntimeState::faulted && error == persistence::SaveError::directory_busy && n_pressed;")
string(FIND "${directory_busy_gate_code}" "${required_directory_busy_gate}"
    directory_busy_gate_index)
if(NOT directory_busy_gate_index EQUAL 0)
    message(FATAL_ERROR
        "Task 6 directory-busy retry gate must require exactly faulted, directory_busy, and the N edge")
endif()

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
set(directory_busy_retry_marker
    "if (host_directory_busy_retry_requested(\n                    runtime.state(), runtime.render_status().error,\n                    frame_input.keys.recovery))")
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
string(FIND "${host_text}" "dungeon::DungeonRenderSnapshot* render_world ="
    mutable_render_world_start)
string(FIND "${host_text}"
    "dungeon::DungeonRenderSnapshot* const render_world ="
    stale_const_render_world_start)
if(mutable_render_world_start EQUAL -1
        OR NOT stale_const_render_world_start EQUAL -1)
    message(FATAL_ERROR
        "Task 6 directory-busy retry must keep the render snapshot pointer rebindable")
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
string(FIND "${host_text}" "${directory_busy_retry_marker}"
    directory_busy_retry_start)
if(directory_busy_retry_start EQUAL -1
        OR recovery_request_start EQUAL -1
        OR NOT host_loop_start LESS directory_busy_retry_start
        OR NOT directory_busy_retry_start LESS recovery_request_start
        OR NOT recovery_request_start LESS recovery_branch_start)
    message(FATAL_ERROR
        "Task 6 must consume directory-busy retry before recovery_required handling and blocking presentation")
endif()
string(REGEX MATCHALL "runtime\\.initialize\\(\\)"
    runtime_initialize_calls "${host_text}")
list(LENGTH runtime_initialize_calls runtime_initialize_call_count)
if(NOT runtime_initialize_call_count EQUAL 2)
    message(FATAL_ERROR
        "Task 6 Host must initialize only at startup and on the explicit directory-busy N edge")
endif()
extract_cpp_function_body("${host_text}" "${directory_busy_retry_marker}"
    directory_busy_retry_body directory_busy_retry_code)
set(required_retry_rebind_sequence
    "const bool retry_initialized = runtime.initialize(); render_world = runtime.render_snapshot_storage();")
string(FIND "${directory_busy_retry_body}" "${required_retry_rebind_sequence}"
    retry_rebind_sequence_index)
if(retry_rebind_sequence_index EQUAL -1)
    message(FATAL_ERROR
        "Task 6 directory-busy retry must rebind render_world immediately after initialize")
endif()
set(directory_busy_retry_last_index -1)
foreach(required IN ITEMS
        "const bool retry_initialized = runtime.initialize();"
        "render_world = runtime.render_snapshot_storage();"
        "if (retry_initialized && runtime.session() != nullptr && render_world != nullptr)"
        "runtime.session()->snapshot(current);"
        "previous = current;"
        "validation_runtime->observe_snapshot(current);"
        "drain_events(*runtime.session(), renderer, feedback, audio, validation_runtime.get());"
        "fixed_step.clear_accumulator();"
        "continue;")
    string(FIND "${directory_busy_retry_body}" "${required}"
        directory_busy_retry_required_index)
    if(directory_busy_retry_required_index EQUAL -1
            OR NOT directory_busy_retry_last_index LESS
                directory_busy_retry_required_index)
        message(FATAL_ERROR
            "Task 6 directory-busy retry is missing or misorders: ${required}")
    endif()
    set(directory_busy_retry_last_index
        ${directory_busy_retry_required_index})
endforeach()
set(required_consumed_retry
    "fixed_step.clear_accumulator(); } continue;")
string(FIND "${directory_busy_retry_body}" "${required_consumed_retry}"
    consumed_retry_index)
if(consumed_retry_index EQUAL -1)
    message(FATAL_ERROR
        "Task 6 directory-busy retry must consume the N edge on both success and failure")
endif()
foreach(forbidden IN ITEMS "recover_with_new_run(" "while (" "for (")
    string(FIND "${directory_busy_retry_code}" "${forbidden}"
        directory_busy_retry_forbidden_index)
    if(NOT directory_busy_retry_forbidden_index EQUAL -1)
        message(FATAL_ERROR
            "Task 6 directory-busy retry contains forbidden action: ${forbidden}")
    endif()
endforeach()
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
