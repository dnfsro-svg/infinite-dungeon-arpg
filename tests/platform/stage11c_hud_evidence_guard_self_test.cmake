if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11c_hud_evidence_guard_test.cmake")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_game_validation.cpp")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_runtime "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp")
set(_bad "${SOURCE_ROOT}/tests/platform/stage11c_hud_bad_formal_input.txt")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")
file(READ "${_formal}" _formal_source)
file(READ "${_host}" _host_source)
file(READ "${_runtime}" _runtime_source)
set(_input "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.cpp")
if(NOT EXISTS "${_input}")
    message(FATAL_ERROR "Stage11C input source is missing: ${_input}")
endif()
file(READ "${_input}" _input_source)
set(_stage_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.cpp")
if(NOT EXISTS "${_stage_source}")
    message(FATAL_ERROR "Stage11C validation source is missing: ${_stage_source}")
endif()
file(READ "${_stage_source}" _stage_source_text)
file(READ "${_bad}" _bad_source)

function(stage11c_expect_task7c_host_rejection LABEL MUTATED EXPECTED)
    set(_mutation "${GUARD_TEST_ROOT}/host-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${MUTATED}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR
            "Stage11C evidence guard accepted Task7C Host mutation: ${LABEL}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR
            "Task7C Host mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

set(_task7c_mask_hud_observer
    "            validation_runtime->observe_hud(\n                current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug, GetScreenWidth(), GetScreenHeight());")
set(_task7c_mask_hud_observer_get
    "            validation_runtime.get()->observe_hud(\n                current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug, GetScreenWidth(), GetScreenHeight());")
set(_task7c_mask_renderer_hud_anchor
    "            renderer.observe_presented_hud_frame(hud_presented_frame,")

string(REPLACE "${_task7c_mask_hud_observer}"
    "            if (false) {\n${_task7c_mask_hud_observer}\n            }"
    _task7c_m08_mask_host "${_host_source}")
string(REPLACE "${_task7c_mask_renderer_hud_anchor}"
    "${_task7c_mask_hud_observer_get}\n${_task7c_mask_renderer_hud_anchor}"
    _task7c_m08_mask_host "${_task7c_m08_mask_host}")
stage11c_expect_task7c_host_rejection(task7c_m08_dead_decoy_get_call
    "${_task7c_m08_mask_host}" "T7C-M08")

string(REPLACE "${_task7c_mask_hud_observer}"
    "            const auto task7c_hud_observer_decoy = [&] {\n${_task7c_mask_hud_observer}\n            };"
    _task7c_m09_mask_host "${_host_source}")
string(REPLACE "            BeginDrawing();"
    "            BeginDrawing();\n${_task7c_mask_hud_observer_get}"
    _task7c_m09_mask_host "${_task7c_m09_mask_host}")
stage11c_expect_task7c_host_rejection(task7c_m09_lambda_decoy_get_call
    "${_task7c_m09_mask_host}" "T7C-M09")

string(REPLACE "${_task7c_mask_hud_observer}"
    "            validation_runtime.get()->observe_hud(\n                current, HudViewModel{}, renderer.hud_notice_view(),\n                draw_debug, GetScreenWidth(), GetScreenHeight());\n            if (false) {\n${_task7c_mask_hud_observer}\n            }"
    _task7c_m10_model_mask_host "${_host_source}")
stage11c_expect_task7c_host_rejection(task7c_m10_model_dead_decoy_get_call
    "${_task7c_m10_model_mask_host}" "T7C-M10")

string(REPLACE "${_task7c_mask_hud_observer}"
    "            validation_runtime.get()->observe_hud(\n                current, renderer.hud_model(), HudNoticeView{},\n                draw_debug, GetScreenWidth(), GetScreenHeight());\n            const auto task7c_hud_observer_decoy = [&] {\n${_task7c_mask_hud_observer}\n            };"
    _task7c_m10_notices_mask_host "${_host_source}")
stage11c_expect_task7c_host_rejection(task7c_m10_notices_lambda_decoy_get_call
    "${_task7c_m10_notices_mask_host}" "T7C-M10")

set(_task7c_m25_shutdown_chain [=[        validation_runtime->write_summaries(
            runtime.clean_shutdown_state(), pause_menu);
        audio.shutdown();
        renderer.shutdown_resources();
        pause_menu_renderer.shutdown();
        window.close();]=])
set(_task7c_m25_dead_shutdown_chain [=[        validation_runtime->write_summaries(
            runtime.clean_shutdown_state(), pause_menu);
        if (false) {
            audio.shutdown();
            renderer.shutdown_resources();
            pause_menu_renderer.shutdown();
            window.close();
        }]=])
string(REPLACE "${_task7c_m25_shutdown_chain}"
    "${_task7c_m25_dead_shutdown_chain}"
    _task7c_m25_dead_shutdown_host "${_host_source}")
stage11c_expect_task7c_host_rejection(task7c_m25_dead_shutdown_decoy
    "${_task7c_m25_dead_shutdown_host}" "facade summary binding/order")

set(_task7c_m25_conditional_shutdown_chain [=[        validation_runtime->write_summaries(
            runtime.clean_shutdown_state(), pause_menu);
        if (config.fullscreen) {
            audio.shutdown();
            renderer.shutdown_resources();
            pause_menu_renderer.shutdown();
            window.close();
        }]=])
string(REPLACE "${_task7c_m25_shutdown_chain}"
    "${_task7c_m25_conditional_shutdown_chain}"
    _task7c_m25_conditional_shutdown_host "${_host_source}")
stage11c_expect_task7c_host_rejection(task7c_m25_conditional_shutdown_decoy
    "${_task7c_m25_conditional_shutdown_host}"
    "facade summary binding/order")

if(DEFINED STAGE11C_TASK7C_HOST_MASK_ONLY)
    message(STATUS "Stage11C HUD evidence Task7C Host-mask cases passed")
    return()
endif()

function(stage11c_expect_formal_rejection LABEL TOKEN EXPECTED)
    string(FIND "${_bad_source}" "${LABEL}:" _named)
    if(_named EQUAL -1)
        message(FATAL_ERROR "bad formal input is missing named mutation: ${LABEL}")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/formal-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_formal_source}\n// named mutation: ${LABEL}\n${TOKEN}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DFORMAL_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named mutation: ${LABEL}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR "named mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

stage11c_expect_formal_rejection(
    direct_viewmodel "HudViewModel direct_model{}" "direct ViewModel")
stage11c_expect_formal_rejection(
    test_access "TestAccess" "TestAccess")
stage11c_expect_formal_rejection(
    logical_action_queue "session.queue_action(combat::Action::light)" "logical action queue")
stage11c_expect_formal_rejection(
    fake_result_pass "result=pass" "fake result pass")
stage11c_expect_formal_rejection(
    bypassed_session_progression "state.progression = {}" "bypassed Session progression")

set(_pre_present "${GUARD_TEST_ROOT}/host-pre-present.cpp")
string(REPLACE
    "EndDrawing();\n    if (path == nullptr) return true;\n    Image image = LoadImageFromScreen();"
    "Image image = LoadImageFromScreen();\n    EndDrawing();\n    if (path == nullptr) return true;"
    _pre_present_source "${_host_source}")
if(_pre_present_source STREQUAL _host_source)
    message(FATAL_ERROR "pre-present capture mutation did not change production source")
endif()
file(WRITE "${_pre_present}" "${_pre_present_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_pre_present}" -P "${_guard}"
    RESULT_VARIABLE _pre_result OUTPUT_VARIABLE _pre_stdout ERROR_VARIABLE _pre_stderr)
if(_pre_result EQUAL 0)
    message(FATAL_ERROR "Stage11C evidence guard accepted named mutation: pre_present_capture")
endif()
if(NOT "${_pre_stdout}${_pre_stderr}" MATCHES "pre-Present capture")
    message(FATAL_ERROR "named mutation pre_present_capture failed for wrong reason: ${_pre_stdout}${_pre_stderr}")
endif()

function(stage11c_expect_host_rejection LABEL NEEDLE REPLACEMENT EXPECTED)
    string(FIND "${_bad_source}" "${LABEL}:" _named)
    if(_named EQUAL -1)
        message(FATAL_ERROR "bad formal input is missing named mutation: ${LABEL}")
    endif()
    string(FIND "${_host_source}" "${NEEDLE}" _needle_found)
    if(_needle_found EQUAL -1)
        message(FATAL_ERROR "host mutation ${LABEL} cannot find production replacement site")
    endif()
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_host_source}")
    if(_mutated STREQUAL _host_source)
        message(FATAL_ERROR "host mutation ${LABEL} did not change production source")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/host-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named host mutation: ${LABEL}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR "named host mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_host_text_rejection LABEL NEEDLE REPLACEMENT EXPECTED)
    string(FIND "${_host_source}" "${NEEDLE}" _needle_found)
    if(_needle_found EQUAL -1)
        message(FATAL_ERROR "host mutation ${LABEL} cannot find production replacement site")
    endif()
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_host_source}")
    if(_mutated STREQUAL _host_source)
        message(FATAL_ERROR "host mutation ${LABEL} did not change production source")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/host-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named host mutation: ${LABEL}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR "named host mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_host_source_rejection LABEL MUTATED EXPECTED)
    if(MUTATED STREQUAL _host_source)
        message(FATAL_ERROR "host mutation ${LABEL} did not change production source")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/host-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${MUTATED}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named host mutation: ${LABEL}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR "named host mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_runtime_rejection LABEL NEEDLE REPLACEMENT EXPECTED)
    string(FIND "${_runtime_source}" "${NEEDLE}" _needle_found)
    if(_needle_found EQUAL -1)
        message(FATAL_ERROR
            "runtime mutation ${LABEL} cannot find production replacement site")
    endif()
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_runtime_source}")
    if(_mutated STREQUAL _runtime_source)
        message(FATAL_ERROR "runtime mutation ${LABEL} made no change")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/runtime-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_VALIDATION_RUNTIME_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR
            "Stage11C evidence guard accepted runtime mutation: ${LABEL}")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" " " _diagnostic
        "${_stdout}${_stderr}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _expected "${EXPECTED}")
    string(FIND "${_diagnostic}" "${_expected}" _expected_position)
    if(_expected_position EQUAL -1)
        message(FATAL_ERROR
            "runtime mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_runtime_acceptance LABEL NEEDLE REPLACEMENT)
    string(FIND "${_runtime_source}" "${NEEDLE}" _needle_found)
    if(_needle_found EQUAL -1)
        message(FATAL_ERROR
            "runtime variant ${LABEL} cannot find production replacement site")
    endif()
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_runtime_source}")
    if(_mutated STREQUAL _runtime_source)
        message(FATAL_ERROR "runtime variant ${LABEL} made no change")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/runtime-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_VALIDATION_RUNTIME_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected harmless runtime variant ${LABEL}: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_input_rejection LABEL NEEDLE REPLACEMENT EXPECTED)
    string(FIND "${_input_source}" "${NEEDLE}" _needle_found)
    if(_needle_found EQUAL -1)
        message(FATAL_ERROR "input mutation ${LABEL} cannot find production replacement site")
    endif()
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_input_source}")
    if(_mutated STREQUAL _input_source)
        message(FATAL_ERROR "input mutation ${LABEL} did not change production source")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/input-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DINPUT_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named input mutation: ${LABEL}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR "named input mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_stage_rejection LABEL NEEDLE REPLACEMENT EXPECTED)
    string(FIND "${_stage_source_text}" "${NEEDLE}" _needle_found)
    if(_needle_found EQUAL -1)
        message(FATAL_ERROR "Stage source mutation ${LABEL} cannot find production replacement site")
    endif()
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_stage_source_text}")
    if(_mutated STREQUAL _stage_source_text)
        message(FATAL_ERROR "Stage source mutation ${LABEL} did not change production source")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/stage-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DSTAGE11C_SOURCE_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named Stage mutation: ${LABEL}")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" " " _diagnostic
        "${_stdout}${_stderr}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _expected "${EXPECTED}")
    string(FIND "${_diagnostic}" "${_expected}" _expected_position)
    if(_expected_position EQUAL -1)
        message(FATAL_ERROR "named Stage mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

function(stage11c_expect_stage_source_rejection LABEL MUTATED EXPECTED)
    if(MUTATED STREQUAL _stage_source_text)
        message(FATAL_ERROR "Stage source mutation ${LABEL} did not change production source")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/stage-${LABEL}.cpp")
    file(WRITE "${_mutation}" "${MUTATED}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DSTAGE11C_SOURCE_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11C evidence guard accepted named Stage mutation: ${LABEL}")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" " " _diagnostic
        "${_stdout}${_stderr}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _expected "${EXPECTED}")
    string(FIND "${_diagnostic}" "${_expected}" _expected_position)
    if(_expected_position EQUAL -1)
        message(FATAL_ERROR "named Stage mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

stage11c_expect_input_rejection(input_test_access
    "snapshot.down[index] = true;"
    "snapshot.down[index] = true;\n    TestAccess stage11c_test_access{};"
    "non-physical shared input")
stage11c_expect_input_rejection(input_direct_viewmodel
    "snapshot.down[index] = true;"
    "snapshot.down[index] = true;\n    HudViewModel direct_model{};"
    "non-physical shared input")
stage11c_expect_input_rejection(input_direct_logical_action
    "snapshot.down[index] = true;"
    "snapshot.down[index] = true;\n    session.queue_action(combat::Action::light);"
    "non-physical shared input")
stage11c_expect_input_rejection(input_skip_stable_binding
    "settings::binding_for(settings_data, action)"
    "static_cast<settings::StableKey>(action)"
    "skipped stable binding")

stage11c_expect_runtime_rejection(fake_font_ready
    "impl_->states.stage11c.cjk_font_ready = cjk_font_ready;"
    "impl_->states.stage11c.cjk_font_ready = true;"
    "font-ready ownership")

stage11c_expect_host_text_rejection(task7c_m10_fabricated_hud_model
    "current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug,"
    "current, HudViewModel{}, renderer.hud_notice_view(),\n                draw_debug,"
    "T7C-M10: Stage11C evidence guard rejected fabricated HUD model argument")
stage11c_expect_host_text_rejection(task7c_m10_fabricated_hud_notices
    "current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug,"
    "current, renderer.hud_model(), HudNoticeView{},\n                draw_debug,"
    "T7C-M10: Stage11C evidence guard rejected fabricated HUD notices argument")
stage11c_expect_runtime_rejection(task7c_m10_corrupt_layout
    "make_hud_layout(\n            screen_width, screen_height, true)"
    "make_hud_layout(\n            screen_height, screen_width, true)"
    "T7C-M10 layout")
stage11c_expect_runtime_rejection(task7c_m10_corrupt_hash
    "stage11c_production_snapshot_hash(snapshot)"
    "stage11c_production_snapshot_hash(dungeon::DungeonSnapshot{})"
    "T7C-M10 hash")
stage11c_expect_host_text_rejection(task7c_m20_duplicate_capture_callback
    "validation_runtime->observe_capture_result("
    "validation_runtime->observe_capture_result(selected_capture_owner, selected_validation_capture_succeeded);\n            validation_runtime->observe_capture_result("
    "T7C-M20")
stage11c_expect_runtime_rejection(task7c_m25_duplicate_stage11c_summary
    "host_validation::write_stage11c_hud_validation_summary("
    "host_validation::write_stage11c_hud_validation_summary(*impl_->config, impl_->states.stage11c);\n    host_validation::write_stage11c_hud_validation_summary("
    "T7C-M25")
string(REGEX MATCH
    "impl_->([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*=[ \t\r\n]*stage11c_reached;"
    _stage11c_pending_assignment "${_runtime_source}")
if("${_stage11c_pending_assignment}" STREQUAL "")
    message(FATAL_ERROR "Stage11C pending capture mutation anchor is missing")
endif()
set(_stage11c_pending_field "${CMAKE_MATCH_1}")
set(_stage11c_capture_gate
    "if (impl_->${_stage11c_pending_field}) {\n        impl_->states.stage11c.captured = true;\n    }")
stage11c_expect_runtime_rejection(task7c_pending_capture_publication
    "${_stage11c_pending_assignment}"
    "impl_->${_stage11c_pending_field} = false;"
    "T7C-Stage11C-pending")
stage11c_expect_runtime_rejection(task7c_capture_result_pending_gate
    "${_stage11c_capture_gate}"
    "impl_->states.stage11c.captured = true;"
    "T7C-Stage11C-capture")
stage11c_expect_runtime_rejection(task7c_pending_capture_in_uncalled_lambda
    "${_stage11c_pending_assignment}"
    "const auto pending_capture_decoy = [&] { ${_stage11c_pending_assignment} };"
    "T7C-Stage11C-pending")
stage11c_expect_runtime_rejection(task7c_capture_gate_in_dead_branch
    "${_stage11c_capture_gate}"
    "if (false) { ${_stage11c_capture_gate} }"
    "T7C-Stage11C-capture")
stage11c_expect_runtime_acceptance(runtime_stage11c_pending_field_rename
    "${_stage11c_pending_field}" "pending_hud_capture_ready")
stage11c_expect_runtime_rejection(runtime_hud_owner_comment_string_decoy
    "void HostValidationRuntime::observe_hud("
    "// void HostValidationRuntime::observe_hud(\nconstexpr const char* task7c_hud_owner_decoy = \"void HostValidationRuntime::observe_hud(\";\nvoid HostValidationRuntime::observe_hud_removed("
    "T7C-HUD-observation")
stage11c_expect_runtime_acceptance(runtime_hud_owner_inactive_decoy
    "void HostValidationRuntime::observe_hud("
    "#if 0\nvoid HostValidationRuntime::observe_hud() {}\n#endif\nvoid HostValidationRuntime::observe_hud(")

set(_summary_gate
    "const bool stage11c_validation_result = state.captured\n            && state.cjk_font_ready && state.production_snapshot_hash != 0U;")
set(_driver_signature_tail
    "const dungeon::DungeonSnapshot& current,\n    Stage11CHudValidationState& state) noexcept {")
stage11c_expect_stage_rejection(stage_fake_summary_state
    "${_summary_gate}"
    "const bool stage11c_validation_result = true || state.captured"
    "fake summary state")

stage11c_expect_stage_rejection(stage_nonphysical_driver
    "++state.injected_frames;"
    "++state.injected_frames;\n    TestAccess stage11c_test_access{};"
    "non-physical scenario driver")
stage11c_expect_stage_rejection(stage_hash_constant
    "mix(snapshot.root_seed);"
    "return 1U;"
    "production hash token")
stage11c_expect_stage_rejection(stage_reached_constant
    "case Scenario::cleared_exit:"
    "return true;"
    "reached predicate token")
stage11c_expect_stage_rejection(stage_summary_notice_removed
    "state.notices.primary.kind"
    "state.notices_primary_removed"
    "summary token")
stage11c_expect_stage_rejection(stage_driver_early_return
    "${_driver_signature_tail}"
    "${_driver_signature_tail}\n    return snapshot;"
    "physical driver return inventory")
stage11c_expect_stage_rejection(stage_hash_early_return
    "const dungeon::DungeonSnapshot& snapshot) noexcept {"
    "const dungeon::DungeonSnapshot& snapshot) noexcept {\n    return 1U;"
    "production hash return inventory")
stage11c_expect_stage_rejection(stage_reached_early_return
    "const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
    "const Stage11CHudValidationState& state, bool draw_debug) noexcept {\n    return true;"
    "reached predicate return inventory")
stage11c_expect_stage_rejection(stage_summary_early_return
    "const Stage11CHudValidationState& state) noexcept {"
    "const Stage11CHudValidationState& state) noexcept {\n    return;"
    "summary return inventory")
stage11c_expect_stage_rejection(stage_driver_unreachable_return
    "${_driver_signature_tail}"
    "${_driver_signature_tail}\n    if (true) {\n        return snapshot;\n    }"
    "physical driver return inventory")
stage11c_expect_stage_rejection(stage_hash_unreachable_return
    "const dungeon::DungeonSnapshot& snapshot) noexcept {"
    "const dungeon::DungeonSnapshot& snapshot) noexcept {\n    if (true) {\n        return 1U;\n    }"
    "production hash return inventory")
stage11c_expect_stage_rejection(stage_reached_unreachable_return
    "const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
    "const Stage11CHudValidationState& state, bool draw_debug) noexcept {\n    if (true) {\n        return true;\n    }"
    "reached predicate return inventory")
stage11c_expect_stage_rejection(stage_summary_unreachable_return
    "const Stage11CHudValidationState& state) noexcept {"
    "const Stage11CHudValidationState& state) noexcept {\n    if (true) {\n        return;\n    }"
    "summary return inventory")
stage11c_expect_stage_rejection(stage_hash_direct_overwrite
    "mix(snapshot.root_seed);"
    "mix(snapshot.root_seed);\n    hash = 1U;"
    "production hash direct overwrite")
stage11c_expect_stage_rejection(stage_hash_offset_basis_changed
    "1469598103934665603ULL"
    "1469598103934665604ULL"
    "production hash core")
stage11c_expect_stage_rejection(stage_hash_xor_changed
    "hash ^= value;"
    "hash ^= value + 1U;"
    "production hash core")
stage11c_expect_stage_rejection(stage_hash_prime_changed
    "hash *= 1099511628211ULL;"
    "hash *= 1099511628213ULL;"
    "production hash core")

set(_hash_core_begin "    std::uint64_t hash = 1469598103934665603ULL;")
set(_hash_core_return "    return hash;")
string(FIND "${_stage_source_text}" "${_hash_core_begin}" _hash_core_begin_at)
string(FIND "${_stage_source_text}" "${_hash_core_return}" _hash_core_return_at)
if(_hash_core_begin_at EQUAL -1 OR _hash_core_return_at EQUAL -1
        OR NOT _hash_core_begin_at LESS _hash_core_return_at)
    message(FATAL_ERROR "hash relocation mutation anchor is missing")
endif()
string(SUBSTRING "${_stage_source_text}" 0 ${_hash_core_begin_at}
    _hash_relocation_prefix)
math(EXPR _hash_core_length "${_hash_core_return_at} - ${_hash_core_begin_at}")
string(SUBSTRING "${_stage_source_text}" ${_hash_core_begin_at}
    ${_hash_core_length} _hash_relocation_body)
string(LENGTH "${_hash_core_return}" _hash_core_return_length)
math(EXPR _hash_relocation_suffix_at
    "${_hash_core_return_at} + ${_hash_core_return_length}")
string(SUBSTRING "${_stage_source_text}" ${_hash_relocation_suffix_at} -1
    _hash_relocation_suffix)
set(_hash_relocation_source "${_hash_relocation_prefix}    [[maybe_unused]] const auto stage11c_hash_core_decoy = [&] {\n${_hash_relocation_body}    };\n    std::uint64_t hash{1U};\n    return hash;${_hash_relocation_suffix}")
stage11c_expect_stage_source_rejection(stage_hash_core_in_uncalled_lambda
    "${_hash_relocation_source}"
    "production hash core")

message(STATUS "Stage11C HUD evidence guard self-test passed")
