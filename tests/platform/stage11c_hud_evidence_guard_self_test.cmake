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
    "observation scope")
stage11c_expect_host_text_rejection(host_fake_notices_capture
    "stage11c_validation_state.notices = renderer.hud_notice_view();"
    "stage11c_validation_state.notices = {};"
    "observation scope")
stage11c_expect_host_text_rejection(host_fake_layout_capture
    "stage11c_validation_state.layout = make_hud_layout("
    "stage11c_validation_state.layout = {};\n                make_hud_layout("
    "fake layout capture")
stage11c_expect_host_text_rejection(host_duplicate_notices_capture
    "stage11c_validation_state.notices = renderer.hud_notice_view();"
    "stage11c_validation_state.notices = renderer.hud_notice_view();\n                stage11c_validation_state.notices = renderer.hud_notice_view();"
    "observation scope")
stage11c_expect_host_text_rejection(host_captured_early
    "const bool capture_succeeded =\n                present_frame_and_maybe_capture("
    "stage11c_validation_state.captured = true;\n            const bool capture_succeeded =\n                present_frame_and_maybe_capture("
    "capture scope")
stage11c_expect_host_text_rejection(host_capture_success_removed
    "&& capture_succeeded;"
    ";"
    "capture ordering")

set(_run_signature "HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {")
string(REPLACE "${_run_signature}"
    "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN observation\nconstexpr const char* stage11c_marker_decoy = \"// STAGE11C_HUD_VALIDATION_SEAM_BEGIN observation\";\n${_run_signature}"
    _marker_decoy_source "${_host_source}")
stage11c_expect_host_source_rejection(host_marker_comment_string_decoy
    "${_marker_decoy_source}" "cannot bind observation seam marker")

function(stage11c_extract_host_seam SOURCE NAME OUT_SEAM)
    set(_begin "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN ${NAME}")
    set(_end "// STAGE11C_HUD_VALIDATION_SEAM_END ${NAME}")
    string(FIND "${SOURCE}" "${_begin}" _begin_position)
    string(FIND "${SOURCE}" "${_end}" _end_position)
    if(_begin_position EQUAL -1 OR _end_position EQUAL -1
            OR _end_position LESS _begin_position)
        message(FATAL_ERROR "cannot extract Stage11C ${NAME} seam mutation")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _seam_length "${_end_position} - ${_begin_position} + ${_end_length}")
    string(SUBSTRING "${SOURCE}" ${_begin_position} ${_seam_length} _seam)
    set(${OUT_SEAM} "${_seam}" PARENT_SCOPE)
endfunction()

stage11c_extract_host_seam("${_host_source}" observation _observation_seam)
string(REPLACE "${_observation_seam}"
    "const auto stage11c_observation_decoy = [&] {\n${_observation_seam}\n            };\n            const bool stage11c_target_visible = false;"
    _observation_lambda_source "${_host_source}")
stage11c_expect_host_source_rejection(host_observation_seam_in_lambda
    "${_observation_lambda_source}" "observation seam scope")

stage11c_extract_host_seam("${_host_source}" presented_capture _presented_capture_seam)
string(REPLACE "${_presented_capture_seam}"
    "const auto stage11c_presented_capture_decoy = [&] {\n${_presented_capture_seam}\n            };\n            static_cast<void>(present_frame_and_maybe_capture(nullptr));\n            ++presented_frame_count;\n            const bool capture_succeeded = false;\n            const bool captured_stage10_frame = false;"
    _presented_capture_lambda_source "${_host_source}")
stage11c_expect_host_source_rejection(host_presented_capture_seam_in_lambda
    "${_presented_capture_lambda_source}" "presented capture seam scope")

stage11c_extract_host_seam("${_host_source}" reached _reached_seam)
string(REPLACE "${_reached_seam}"
    "if (false) {\n${_reached_seam}\n            }\n            const bool stage11c_reached = false;"
    _reached_if_false_source "${_host_source}")
stage11c_expect_host_source_rejection(host_reached_seam_in_if_false
    "${_reached_if_false_source}" "reached seam scope")

set(_captured_block "if (captured_stage10_frame && stage11c_reached) {\n                stage11c_validation_state.captured = true;\n            }")
string(REPLACE "${_captured_block}" "if (captured_stage10_frame && stage11c_reached) {\n            }"
    _captured_early_source "${_host_source}")
string(REPLACE "const bool capture_succeeded =\n                present_frame_and_maybe_capture("
    "if (stage11c_reached) {\n                stage11c_validation_state.captured = true;\n            }\n            const bool capture_succeeded =\n                present_frame_and_maybe_capture("
    _captured_early_source "${_captured_early_source}")
stage11c_expect_host_source_rejection(host_captured_moved_before_present
    "${_captured_early_source}" "rejected capture ordering")

string(REPLACE "${_captured_block}"
    "const auto stage11c_capture_decoy = [&] {\n                if (captured_stage10_frame && stage11c_reached) {\n                    stage11c_validation_state.captured = true;\n                }\n            };"
    _captured_lambda_source "${_host_source}")
stage11c_expect_host_source_rejection(host_captured_in_lambda
    "${_captured_lambda_source}" "rejected capture scope")

stage11c_expect_host_text_rejection(host_layout_wrong_dimensions
    "GetScreenWidth(), GetScreenHeight(), true"
    "1, 1, false"
    "rejected fake layout capture")

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
stage11c_expect_stage_rejection(stage_driver_early_return
    "Stage11CHudValidationState& state) noexcept {"
    "Stage11CHudValidationState& state) noexcept {\n    return snapshot;"
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
    "Stage11CHudValidationState& state) noexcept {"
    "Stage11CHudValidationState& state) noexcept {\n    if (true) {\n        return snapshot;\n    }"
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
