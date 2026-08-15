cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(guard "${SOURCE_ROOT}/tests/platform/task6_recovery_screen_source_test.cmake")
set(host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    -P "${guard}" RESULT_VARIABLE pristine_result)
if(NOT pristine_result EQUAL 0)
    message(FATAL_ERROR "Task 6 recovery source guard rejected pristine source")
endif()

file(READ "${host}" source)
function(expect_rejected label mutated)
    set(path "${MUTATION_ROOT}/${label}.cpp")
    file(WRITE "${path}" "${mutated}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_SOURCE_OVERRIDE=${path}" -P "${guard}"
        RESULT_VARIABLE result)
    if(result EQUAL 0)
        message(FATAL_ERROR "Task 6 recovery source guard accepted mutation: ${label}")
    endif()
endfunction()

string(REPLACE
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    "draw_recovery_screen(Font{}, runtime.render_status());"
    unshared_font "${source}")
if(unshared_font STREQUAL source)
    message(FATAL_ERROR "Task 6 shared-font mutation anchor is missing")
endif()
expect_rejected("unshared-font" "${unshared_font}")

string(REPLACE
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    "static_cast<void>(present_frame_and_maybe_capture(nullptr));\n                draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    present_before_draw "${source}")
if(present_before_draw STREQUAL source)
    message(FATAL_ERROR "Task 6 present-order mutation anchor is missing")
endif()
expect_rejected("present-before-draw" "${present_before_draw}")

string(REPLACE "draw_crisp_ui_text(font, text"
    "DrawTextEx(font, text" default_text "${source}")
if(default_text STREQUAL source)
    message(FATAL_ERROR "Task 6 CJK draw mutation anchor is missing")
endif()
expect_rejected("default-text" "${default_text}")

string(REPLACE
    "if (runtime.clean_shutdown_state() == CleanShutdownState::ready) {\n                exit_requested = true;\n                continue;"
    "if (runtime.clean_shutdown_state() == CleanShutdownState::ready\n                    || runtime.clean_shutdown_state()\n                        == CleanShutdownState::faulted) {\n                exit_requested = true;\n                continue;"
    faulted_auto_exit "${source}")
if(faulted_auto_exit STREQUAL source)
    message(FATAL_ERROR "Task 6 faulted auto-exit mutation anchor is missing")
endif()
expect_rejected("faulted-auto-exit" "${faulted_auto_exit}")

string(REPLACE
    "if (runtime.state() == DungeonRuntimeState::recovery_required\n                    || runtime.state() == DungeonRuntimeState::faulted)"
    "if (runtime.state() == DungeonRuntimeState::recovery_required)"
    faulted_not_blocked "${source}")
if(faulted_not_blocked STREQUAL source)
    message(FATAL_ERROR "Task 6 faulted blocking mutation anchor is missing")
endif()
expect_rejected("faulted-not-blocked" "${faulted_not_blocked}")

string(REPLACE
    "if (recovery_blocked\n                        && (config.stage12_material_background_only\n                            || config.stage12_material_icons_only))"
    "if (config.stage12_material_background_only\n                        || config.stage12_material_icons_only)"
    faulted_stage12_exit "${source}")
if(faulted_stage12_exit STREQUAL source)
    message(FATAL_ERROR "Task 6 faulted Stage12 mutation anchor is missing")
endif()
expect_rejected("faulted-stage12-exit" "${faulted_stage12_exit}")

string(REPLACE
    "std::optional<std::string> capture_path = recovery_blocked\n                    ? validation_capture_path() : std::nullopt;"
    "std::optional<std::string> capture_path =\n                    validation_capture_path();"
    faulted_periodic_capture "${source}")
if(faulted_periodic_capture STREQUAL source)
    message(FATAL_ERROR "Task 6 faulted capture mutation anchor is missing")
endif()
expect_rejected("faulted-periodic-capture" "${faulted_periodic_capture}")

message(STATUS "Task 6 recovery source guard self-test rejected all mutations")
