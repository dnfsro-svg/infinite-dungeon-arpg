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
set(executed_mutant_count 0)
function(expect_rejected label mutated)
    if(DEFINED TASK6_MUTANT_FILTER
            AND NOT "${label}" MATCHES "${TASK6_MUTANT_FILTER}")
        return()
    endif()
    math(EXPR next_executed_mutant_count "${executed_mutant_count} + 1")
    set(executed_mutant_count ${next_executed_mutant_count} PARENT_SCOPE)
    set(path "${MUTATION_ROOT}/${label}.cpp")
    file(WRITE "${path}" "${mutated}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_SOURCE_OVERRIDE=${path}" -P "${guard}"
        RESULT_VARIABLE result)
    if(result EQUAL 0)
        message(FATAL_ERROR "Task 6 recovery source guard accepted mutation: ${label}")
    endif()
endfunction()

function(replace_unique input match replacement output label)
    set(remainder "${input}")
    set(match_count 0)
    string(LENGTH "${match}" match_length)
    while(TRUE)
        string(FIND "${remainder}" "${match}" match_index)
        if(match_index EQUAL -1)
            break()
        endif()
        math(EXPR match_count "${match_count} + 1")
        math(EXPR after_match "${match_index} + ${match_length}")
        string(SUBSTRING "${remainder}" ${after_match} -1 remainder)
    endwhile()
    if(NOT match_count EQUAL 1)
        message(FATAL_ERROR
            "Task 6 ${label} mutation requires one anchor, found ${match_count}")
    endif()
    string(REPLACE "${match}" "${replacement}" mutated "${input}")
    set(${output} "${mutated}" PARENT_SCOPE)
endfunction()

string(REPLACE
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status(),\n                    recovery_new_run_confirmation);"
    "draw_recovery_screen(Font{}, runtime.render_status(),\n                    recovery_new_run_confirmation);"
    unshared_font "${source}")
if(unshared_font STREQUAL source)
    message(FATAL_ERROR "Task 6 shared-font mutation anchor is missing")
endif()
expect_rejected("unshared-font" "${unshared_font}")

string(REPLACE
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status(),\n                    recovery_new_run_confirmation);"
    "static_cast<void>(present_frame_and_maybe_capture(nullptr));\n                draw_recovery_screen(shared_ui_font.font(), runtime.render_status(),\n                    recovery_new_run_confirmation);"
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

set(directory_busy_gate_predicate
    "    return state == DungeonRuntimeState::faulted\n        && error == persistence::SaveError::directory_busy && n_pressed\n        && !focus_lost && !escape_pressed && !window_close_requested;")
replace_unique("${source}" "${directory_busy_gate_predicate}"
    "    return error == persistence::SaveError::directory_busy && n_pressed\n        && !focus_lost && !escape_pressed && !window_close_requested;"
    directory_busy_broad_predicate "directory-busy-broad-predicate")
expect_rejected("directory-busy-broad-predicate"
    "${directory_busy_broad_predicate}")

replace_unique("${source}" "${directory_busy_gate_predicate}"
    "    return state == DungeonRuntimeState::faulted\n        && error == persistence::SaveError::directory_busy\n        && !focus_lost && !escape_pressed && !window_close_requested;"
    directory_busy_without_n "directory-busy-without-n")
expect_rejected("directory-busy-without-n" "${directory_busy_without_n}")

replace_unique("${source}" "${directory_busy_gate_predicate}"
    "    return state == DungeonRuntimeState::faulted\n        && error == persistence::SaveError::directory_busy && n_pressed;"
    directory_busy_without_exit_priority
    "directory-busy-without-exit-priority")
expect_rejected("directory-busy-without-exit-priority"
    "${directory_busy_without_exit_priority}")

set(directory_busy_branch_anchor
    "            if (host_directory_busy_retry_requested(\n                    runtime.state(), runtime.render_status().error,\n                    frame_input.keys.recovery, frame_input.keys.focus_lost,\n                    frame_input.keys.escape, window_close_requested)) {")
replace_unique("${source}" "${directory_busy_branch_anchor}"
    "            static_cast<void>(runtime.initialize());\n${directory_busy_branch_anchor}"
    directory_busy_automatic_poll "directory-busy-automatic-poll")
expect_rejected("directory-busy-automatic-poll"
    "${directory_busy_automatic_poll}")

replace_unique("${source}"
    "                render_world = runtime.render_snapshot_storage();\n"
    "" directory_busy_without_rebind "directory-busy-without-rebind")
expect_rejected("directory-busy-without-rebind"
    "${directory_busy_without_rebind}")

set(directory_busy_snapshot_anchor
    "                if (retry_initialized && runtime.session() != nullptr\n                        && render_world != nullptr) {\n                    runtime.session()->snapshot(current);\n                    previous = current;")
replace_unique("${source}" "${directory_busy_snapshot_anchor}"
    "                if (retry_initialized && runtime.session() != nullptr\n                        && render_world != nullptr) {\n                    previous = current;"
    directory_busy_without_snapshot "directory-busy-without-snapshot")
expect_rejected("directory-busy-without-snapshot"
    "${directory_busy_without_snapshot}")

set(directory_busy_drain_anchor
    "                    validation_runtime->observe_snapshot(current);\n                    drain_events(*runtime.session(), renderer, feedback, audio,\n                        validation_runtime.get());\n                    fixed_step.clear_accumulator();")
replace_unique("${source}" "${directory_busy_drain_anchor}"
    "                    validation_runtime->observe_snapshot(current);\n                    fixed_step.clear_accumulator();"
    directory_busy_without_events "directory-busy-without-events")
expect_rejected("directory-busy-without-events"
    "${directory_busy_without_events}")

set(directory_busy_accumulator_anchor
    "                    drain_events(*runtime.session(), renderer, feedback, audio,\n                        validation_runtime.get());\n                    fixed_step.clear_accumulator();\n                }\n                continue;")
replace_unique("${source}" "${directory_busy_accumulator_anchor}"
    "                    drain_events(*runtime.session(), renderer, feedback, audio,\n                        validation_runtime.get());\n                }\n                continue;"
    directory_busy_without_accumulator
    "directory-busy-without-accumulator")
expect_rejected("directory-busy-without-accumulator"
    "${directory_busy_without_accumulator}")

set(directory_busy_continue_anchor
    "                }\n                continue;\n            }\n            if (recovery_confirmation.recover_with_new_run)")
replace_unique("${source}" "${directory_busy_continue_anchor}"
    "                }\n            }\n            if (recovery_confirmation.recover_with_new_run)"
    directory_busy_without_continue "directory-busy-without-continue")
expect_rejected("directory-busy-without-continue"
    "${directory_busy_without_continue}")

replace_unique("${source}"
    "                const bool retry_initialized = runtime.initialize();"
    "                static_cast<void>(runtime.recover_with_new_run());\n                const bool retry_initialized = runtime.initialize();"
    directory_busy_reuses_recovery "directory-busy-reuses-recovery")
expect_rejected("directory-busy-reuses-recovery"
    "${directory_busy_reuses_recovery}")

set(recovery_confirmation_cancel_anchor
    "    if (focus_lost || escape_pressed || window_close_requested\n            || runtime_state != DungeonRuntimeState::recovery_required) {")
replace_unique("${source}" "${recovery_confirmation_cancel_anchor}"
    "    if (escape_pressed || window_close_requested\n            || runtime_state != DungeonRuntimeState::recovery_required) {"
    recovery_confirmation_without_focus_cancel
    "recovery-confirmation-without-focus-cancel")
expect_rejected("recovery-confirmation-without-focus-cancel"
    "${recovery_confirmation_without_focus_cancel}")

set(recovery_confirmation_idle_anchor
    "    case RecoveryNewRunConfirmationPhase::idle:\n        return n_pressed\n            ? RecoveryNewRunConfirmationDecision{\n                RecoveryNewRunConfirmationPhase::armed, false}\n            : RecoveryNewRunConfirmationDecision{\n                RecoveryNewRunConfirmationPhase::idle, false};")
replace_unique("${source}" "${recovery_confirmation_idle_anchor}"
    "    case RecoveryNewRunConfirmationPhase::idle:\n        return n_pressed\n            ? RecoveryNewRunConfirmationDecision{\n                RecoveryNewRunConfirmationPhase::submitted, true}\n            : RecoveryNewRunConfirmationDecision{\n                RecoveryNewRunConfirmationPhase::idle, false};"
    recovery_confirmation_single_press
    "recovery-confirmation-single-press")
expect_rejected("recovery-confirmation-single-press"
    "${recovery_confirmation_single_press}")

replace_unique("${source}"
    "        return {RecoveryNewRunConfirmationPhase::submitted, false};"
    "        return {RecoveryNewRunConfirmationPhase::submitted, true};"
    recovery_confirmation_submitted_reentry
    "recovery-confirmation-submitted-reentry")
expect_rejected("recovery-confirmation-submitted-reentry"
    "${recovery_confirmation_submitted_reentry}")

replace_unique("${source}"
    "            if (recovery_confirmation.recover_with_new_run) {"
    "            if (frame_input.keys.recovery) {"
    recovery_confirmation_bypassed_action
    "recovery-confirmation-bypassed-action")
expect_rejected("recovery-confirmation-bypassed-action"
    "${recovery_confirmation_bypassed_action}")

set(recovery_confirmation_update_anchor
    "            const RecoveryNewRunConfirmationDecision recovery_confirmation =\n                update_recovery_new_run_confirmation(")
replace_unique("${source}" "${recovery_confirmation_update_anchor}"
    "            static_cast<void>(update_recovery_new_run_confirmation(\n                recovery_new_run_confirmation, runtime.state(),\n                frame_input.keys.recovery, frame_input.keys.focus_lost,\n                frame_input.keys.escape, window_close_requested));\n${recovery_confirmation_update_anchor}"
    recovery_confirmation_double_update
    "recovery-confirmation-double-update")
expect_rejected("recovery-confirmation-double-update"
    "${recovery_confirmation_double_update}")

replace_unique("${source}"
    "            recovery_new_run_confirmation = recovery_confirmation.phase;\n"
    ""
    recovery_confirmation_not_cleared_before_busy
    "recovery-confirmation-not-cleared-before-busy")
expect_rejected("recovery-confirmation-not-cleared-before-busy"
    "${recovery_confirmation_not_cleared_before_busy}")

replace_unique("${source}"
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status(),\n                    recovery_new_run_confirmation);"
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    recovery_confirmation_view_drops_phase
    "recovery-confirmation-view-drops-phase")
expect_rejected("recovery-confirmation-view-drops-phase"
    "${recovery_confirmation_view_drops_phase}")

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

set(presentable_gate
    "if (!host_runtime_initialization_is_presentable(initialized,\n                runtime.state(), runtime.render_snapshot_storage() != nullptr,\n                runtime.render_status().error))")
set(legacy_gate
    "if (!initialized && runtime.state() != DungeonRuntimeState::recovery_required)")
string(REPLACE "${presentable_gate}" "${legacy_gate}"
    legacy_initialization_gate "${source}")
if(legacy_initialization_gate STREQUAL source)
    message(FATAL_ERROR
        "Task 6 legacy initialization-gate mutation anchor is missing")
endif()
expect_rejected("legacy-initialization-gate" "${legacy_initialization_gate}")

string(REPLACE
    "runtime.state(), runtime.render_snapshot_storage() != nullptr,"
    "runtime.state(), false,"
    unavailable_snapshot "${source}")
if(unavailable_snapshot STREQUAL source)
    message(FATAL_ERROR
        "Task 6 unavailable-snapshot mutation anchor is missing")
endif()
expect_rejected("unavailable-snapshot" "${unavailable_snapshot}")

if(DEFINED TASK6_MUTANT_FILTER AND executed_mutant_count EQUAL 0)
    message(FATAL_ERROR
        "Task 6 mutant filter executed no mutations: ${TASK6_MUTANT_FILTER}")
endif()
message(STATUS
    "Task 6 recovery source guard self-test rejected ${executed_mutant_count} mutations")
