if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11c_hud_evidence_guard_test.cmake")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_game_validation.cpp")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_bad "${SOURCE_ROOT}/tests/platform/stage11c_hud_bad_formal_input.txt")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")
file(READ "${_formal}" _formal_source)
file(READ "${_host}" _host_source)
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

set(_fake_font "${GUARD_TEST_ROOT}/host-fake-font.cpp")
string(REPLACE
    "stage11c_validation_state.cjk_font_ready = hud_resources_ready;"
    "stage11c_validation_state.cjk_font_ready = true;"
    _fake_font_source "${_host_source}")
file(WRITE "${_fake_font}" "${_fake_font_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_fake_font}" -P "${_guard}"
    RESULT_VARIABLE _font_result OUTPUT_VARIABLE _font_stdout ERROR_VARIABLE _font_stderr)
if(_font_result EQUAL 0)
    message(FATAL_ERROR "Stage11C evidence guard accepted named mutation: fake_font_ready")
endif()
if(NOT "${_font_stdout}${_font_stderr}" MATCHES "fake font-ready")
    message(FATAL_ERROR "named mutation fake_font_ready failed for wrong reason: ${_font_stdout}${_font_stderr}")
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
    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
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

set(_runtime_font "stage11c_validation_state.model = renderer.hud_model();")
stage11c_expect_host_text_rejection(host_runtime_model_and_hash_overwrite
    "${_runtime_font}"
    "${_runtime_font}\n                stage11c_validation_state.model = {};\n                stage11c_validation_state.production_snapshot_hash = 1U;"
    "direct model overwrite")

set(_model_copy "stage11c_validation_state.model = renderer.hud_model();")
stage11c_expect_host_rejection(host_direct_model_overwrite
    "${_model_copy}"
    "${_model_copy}\n                stage11c_validation_state.model = {};"
    "direct model overwrite")
stage11c_expect_host_text_rejection(host_model_comment_decoy
    "stage11c_validation_state.model = renderer.hud_model();"
    "// stage11c_validation_state.model = renderer.hud_model();"
    "direct model overwrite")
stage11c_expect_host_text_rejection(host_fake_notices_capture
    "stage11c_validation_state.notices = renderer.hud_notice_view();"
    "stage11c_validation_state.notices = {};"
    "missing capture surface token")
stage11c_expect_host_text_rejection(host_fake_layout_capture
    "stage11c_validation_state.layout = make_hud_layout("
    "stage11c_validation_state.layout = {};\n                make_hud_layout("
    "missing capture surface token")
stage11c_expect_host_text_rejection(host_duplicate_notices_capture
    "stage11c_validation_state.notices = renderer.hud_notice_view();"
    "stage11c_validation_state.notices = renderer.hud_notice_view();\n                stage11c_validation_state.notices = renderer.hud_notice_view();"
    "notices overwrite")
stage11c_expect_host_text_rejection(host_captured_early
    "const bool capture_succeeded =\n                present_frame_and_maybe_capture("
    "stage11c_validation_state.captured = true;\n            const bool capture_succeeded =\n                present_frame_and_maybe_capture("
    "captured overwrite")
stage11c_expect_host_text_rejection(host_capture_success_removed
    "&& capture_succeeded;"
    ";"
    "missing capture success")

set(_hash_copy "stage11c_validation_state.production_snapshot_hash =\n                    host_validation::stage11c_production_snapshot_hash(current);")
stage11c_expect_host_rejection(host_fake_snapshot_hash
    "${_hash_copy}"
    "${_hash_copy}\n                stage11c_validation_state.production_snapshot_hash = 1U;"
    "fake snapshot hash")

set(_summary_gate "const bool stage11c_validation_result = state.captured")
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

stage11c_expect_host_rejection(host_bypassed_session_progression
    "${_model_copy}"
    "${_model_copy}\n                current.progression.level = 99U;"
    "bypassed Session progression")

set(_state_alias
    "host_validation::Stage11CHudValidationState& stage11c_validation_state =\n            validation_states->stage11c;")
string(REPLACE "${_state_alias}"
    "host_validation::Stage11CHudValidationState stage11c_validation_state{};"
    _independent_state_source "${_host_source}")
if(_independent_state_source STREQUAL _host_source)
    message(FATAL_ERROR "independent Stage11C state mutation did not change production source")
endif()
set(_independent_state "${GUARD_TEST_ROOT}/host-independent-stage11c-state.cpp")
file(WRITE "${_independent_state}" "${_independent_state_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_independent_state}" -P "${_guard}"
    RESULT_VARIABLE _independent_state_result
    OUTPUT_VARIABLE _independent_state_stdout
    ERROR_VARIABLE _independent_state_stderr)
if(_independent_state_result EQUAL 0)
    message(FATAL_ERROR "Stage11C evidence guard accepted named host mutation: independent_stage11c_state")
endif()
if(NOT "${_independent_state_stdout}${_independent_state_stderr}" MATCHES
        "cannot bind actual Stage11C runtime alias")
    message(FATAL_ERROR "named host mutation independent_stage11c_state failed for wrong reason: ${_independent_state_stdout}${_independent_state_stderr}")
endif()

stage11c_expect_host_text_rejection(host_decoy_stage11c_alias
    "${_state_alias}"
    "host_validation::Stage11CHudValidationState& stage11c_validation_state =\n            stage11c_decoy;"
    "cannot bind actual Stage11C runtime alias")
