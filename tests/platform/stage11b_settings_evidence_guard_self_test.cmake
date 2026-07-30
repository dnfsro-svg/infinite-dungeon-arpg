if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11b_settings_evidence_guard_test.cmake")
set(_stage_source "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.cpp")
set(_runtime_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp")
set(_settings_runtime_source
    "${SOURCE_ROOT}/src/platform/raylib/host_settings_runtime.cpp")
set(_fixture "${SOURCE_ROOT}/tests/platform/stage11b_settings_bad_host_input.txt")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")
file(READ "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp"
    _formal_source)
foreach(_required IN ITEMS "${_stage_source}" "${_runtime_source}"
        "${_settings_runtime_source}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "Stage11B evidence self-test target is missing: ${_required}")
    endif()
endforeach()

file(READ "${_settings_runtime_source}" _settings_runtime_source_text)
set(_settings_save_statement [=[const settings::SettingsSaveResult saved = store->save(
            pause_menu->committed, save_draft);]=])
string(REPLACE "${_settings_save_statement}" ""
    _settings_without_save "${_settings_runtime_source_text}")
if(_settings_without_save STREQUAL _settings_runtime_source_text)
    message(FATAL_ERROR
        "Stage11B Task8B settings-save mutation anchor disappeared")
endif()
set(_settings_switch_anchor "    switch (command) {")
set(_settings_decoy_block [=[    const auto task8b_save_decoy = [&]() constexpr {
        const settings::SettingsSaveResult saved = store->save(
            pause_menu->committed, save_draft);
    };
    struct Task8BSaveDecoy {
        static void save(const settings::SettingsStore* store,
            const PauseMenuState* pause_menu,
            const settings::SettingsData& save_draft) {
            const settings::SettingsSaveResult saved = store->save(
                pause_menu->committed, save_draft);
        }
    };
    switch (command) {]=])
string(REPLACE "${_settings_switch_anchor}" "${_settings_decoy_block}"
    _settings_decoy_mutation "${_settings_without_save}")
if(_settings_decoy_mutation STREQUAL _settings_without_save)
    message(FATAL_ERROR
        "Stage11B Task8B settings-decoy mutation anchor disappeared")
endif()
string(APPEND _settings_decoy_mutation [=[
namespace arpg::platform {
#if 0
const settings::SettingsSaveResult inactive_saved = store->save(
    pause_menu->committed, save_draft);
#endif
// store->save(pause_menu->committed, save_draft);
constexpr const char* task8b_save_text = "store->save(pause_menu->committed, save_draft);";
void task8b_cross_function_save(const settings::SettingsStore* store,
    const PauseMenuState* pause_menu,
    const settings::SettingsData& save_draft) {
    const settings::SettingsSaveResult saved = store->save(
        pause_menu->committed, save_draft);
}
}  // namespace arpg::platform
]=])
set(_settings_decoy_path
    "${GUARD_TEST_ROOT}/settings-runtime-save-decoys.cpp")
file(WRITE "${_settings_decoy_path}" "${_settings_decoy_mutation}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    "-DSETTINGS_RUNTIME_OVERRIDE=${_settings_decoy_path}" -P "${_guard}"
    RESULT_VARIABLE _settings_decoy_result
    OUTPUT_VARIABLE _settings_decoy_stdout ERROR_VARIABLE _settings_decoy_stderr)
if(_settings_decoy_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11B evidence guard accepted Task8B settings save decoys")
endif()
if(NOT "${_settings_decoy_stdout}${_settings_decoy_stderr}" MATCHES
        "requires exactly one settings-store save")
    message(FATAL_ERROR
        "Task8B settings save decoys failed for wrong reason: ${_settings_decoy_stdout}${_settings_decoy_stderr}")
endif()

set(_settings_external_save_mutation
    "${_settings_runtime_source_text}\nnamespace arpg::platform {\n[[maybe_unused]] settings::SettingsSaveResult task8b_external_save(\n    const settings::SettingsStore* target,\n    const settings::SettingsData& committed,\n    const settings::SettingsData& save_draft) {\n    return target->save(committed, save_draft);\n}\n}\n")
set(_settings_external_save_path
    "${GUARD_TEST_ROOT}/settings-runtime-external-save.cpp")
file(WRITE "${_settings_external_save_path}"
    "${_settings_external_save_mutation}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    "-DSETTINGS_RUNTIME_OVERRIDE=${_settings_external_save_path}" -P "${_guard}"
    RESULT_VARIABLE _settings_external_save_result
    OUTPUT_VARIABLE _settings_external_save_stdout
    ERROR_VARIABLE _settings_external_save_stderr)
if(_settings_external_save_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11B evidence guard accepted an external settings save helper")
endif()
if(NOT "${_settings_external_save_stdout}${_settings_external_save_stderr}"
        MATCHES "requires exactly one settings-store save")
    message(FATAL_ERROR
        "Task8B external settings save failed for wrong reason: ${_settings_external_save_stdout}${_settings_external_save_stderr}")
endif()

set(_settings_renamed_save_mutation
    "${_settings_runtime_source_text}\nnamespace arpg::platform {\n[[maybe_unused]] settings::SettingsSaveResult task8b_renamed_save(\n    const settings::SettingsStore* target,\n    const settings::SettingsData& committed,\n    const settings::SettingsData& save_draft) {\n    const auto& renamed = *target;\n    return renamed.save(committed, save_draft);\n}\n}\n")
set(_settings_renamed_save_path
    "${GUARD_TEST_ROOT}/settings-runtime-renamed-save.cpp")
file(WRITE "${_settings_renamed_save_path}"
    "${_settings_renamed_save_mutation}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    "-DSETTINGS_RUNTIME_OVERRIDE=${_settings_renamed_save_path}" -P "${_guard}"
    RESULT_VARIABLE _settings_renamed_save_result
    OUTPUT_VARIABLE _settings_renamed_save_stdout
    ERROR_VARIABLE _settings_renamed_save_stderr)
if(_settings_renamed_save_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11B evidence guard accepted a renamed settings save helper")
endif()
if(NOT "${_settings_renamed_save_stdout}${_settings_renamed_save_stderr}"
        MATCHES "requires exactly one settings-store save")
    message(FATAL_ERROR
        "Task8B renamed settings save failed for wrong reason: ${_settings_renamed_save_stdout}${_settings_renamed_save_stderr}")
endif()

set(_settings_member_pointer_save_mutation
    "${_settings_runtime_source_text}\nnamespace arpg::platform {\n[[maybe_unused]] settings::SettingsSaveResult task8b_member_pointer_save(\n    const settings::SettingsStore* target,\n    const settings::SettingsData& committed,\n    const settings::SettingsData& save_draft) {\n    return (target->*(&settings::SettingsStore::save))(\n        committed, save_draft);\n}\n}\n")
set(_settings_member_pointer_save_path
    "${GUARD_TEST_ROOT}/settings-runtime-member-pointer-save.cpp")
file(WRITE "${_settings_member_pointer_save_path}"
    "${_settings_member_pointer_save_mutation}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    "-DSETTINGS_RUNTIME_OVERRIDE=${_settings_member_pointer_save_path}"
    -P "${_guard}"
    RESULT_VARIABLE _settings_member_pointer_save_result
    OUTPUT_VARIABLE _settings_member_pointer_save_stdout
    ERROR_VARIABLE _settings_member_pointer_save_stderr)
if(_settings_member_pointer_save_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11B evidence guard accepted a member-pointer settings save helper")
endif()
if(NOT "${_settings_member_pointer_save_stdout}${_settings_member_pointer_save_stderr}"
        MATCHES "requires exactly one settings-store save")
    message(FATAL_ERROR
        "Task8B member-pointer settings save failed for wrong reason: ${_settings_member_pointer_save_stdout}${_settings_member_pointer_save_stderr}")
endif()

set(_settings_harmless_mutation
    "${_settings_runtime_source_text}\n// store->save(pause_menu->committed, save_draft);\nconstexpr const char* task8b_harmless_save_text = \"store->save(pause_menu->committed, save_draft);\";\n")
set(_settings_harmless_path
    "${GUARD_TEST_ROOT}/settings-runtime-harmless-decoys.cpp")
file(WRITE "${_settings_harmless_path}" "${_settings_harmless_mutation}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    "-DSETTINGS_RUNTIME_OVERRIDE=${_settings_harmless_path}" -P "${_guard}"
    RESULT_VARIABLE _settings_harmless_result
    OUTPUT_VARIABLE _settings_harmless_stdout
    ERROR_VARIABLE _settings_harmless_stderr)
if(NOT _settings_harmless_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11B evidence guard rejected harmless settings decoys: ${_settings_harmless_stdout}${_settings_harmless_stderr}")
endif()

string(ASCII 92 _settings_macro_backslash)
string(ASCII 10 _settings_macro_line_feed)
set(_settings_owner_anchor "bool HostSettingsRuntime::settle(")
set(_settings_macro_names plain spliced digraph)
set(_settings_macro_directives
    "#define live input"
    "#defi${_settings_macro_backslash}${_settings_macro_line_feed}ne live input"
    "%:define live input")
foreach(_settings_macro_index RANGE 0 2)
    list(GET _settings_macro_names ${_settings_macro_index}
        _settings_macro_name)
    list(GET _settings_macro_directives ${_settings_macro_index}
        _settings_macro_directive)
    string(REPLACE "${_settings_owner_anchor}"
        "${_settings_macro_directive}\n${_settings_owner_anchor}"
        _settings_macro_mutation "${_settings_runtime_source_text}")
    if(_settings_macro_mutation STREQUAL _settings_runtime_source_text)
        message(FATAL_ERROR
            "Stage11B ${_settings_macro_name} macro mutation anchor disappeared")
    endif()
    set(_settings_macro_path
        "${GUARD_TEST_ROOT}/settings-runtime-macro-${_settings_macro_name}.cpp")
    file(WRITE "${_settings_macro_path}" "${_settings_macro_mutation}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DSETTINGS_RUNTIME_OVERRIDE=${_settings_macro_path}" -P "${_guard}"
        RESULT_VARIABLE _settings_macro_result
        OUTPUT_VARIABLE _settings_macro_stdout
        ERROR_VARIABLE _settings_macro_stderr)
    if(_settings_macro_result EQUAL 0)
        message(FATAL_ERROR
            "Stage11B evidence guard accepted ${_settings_macro_name} settings macro")
    endif()
    if(NOT "${_settings_macro_stdout}${_settings_macro_stderr}" MATCHES
            "settings settlement owner forbids preprocessor macros")
        message(FATAL_ERROR
            "Task8B ${_settings_macro_name} settings macro failed for wrong reason: ${_settings_macro_stdout}${_settings_macro_stderr}")
    endif()
endforeach()
if(DEFINED TASK8B_TARGETED_ONLY AND TASK8B_TARGETED_ONLY)
    message(STATUS
        "Stage11B Task8B targeted settings-owner self-test passed: bad_mutations=7; harmless_decoys=1")
    return()
endif()
file(READ "${_fixture}" _fixture_text)
foreach(_fixture_forbidden IN ITEMS
        "TestAccess" "validation_input_setter" "queue_action"
        "pause_menu.committed =" "LoadImageFromScreen()")
    string(FIND "${_fixture_text}" "${_fixture_forbidden}" _fixture_token)
    if(_fixture_token EQUAL -1)
        message(FATAL_ERROR "bad host input fixture is missing: ${_fixture_forbidden}")
    endif()
    set(_formal_mutation
        "${GUARD_TEST_ROOT}/formal-${_fixture_token}.cpp")
    file(WRITE "${_formal_mutation}"
        "${_formal_source}\n// copied fixture mutation\n${_fixture_forbidden}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DFORMAL_OVERRIDE=${_formal_mutation}" -P "${_guard}"
        RESULT_VARIABLE _fixture_result OUTPUT_VARIABLE _fixture_stdout ERROR_VARIABLE _fixture_stderr)
    if(_fixture_result EQUAL 0)
        message(FATAL_ERROR "evidence guard accepted fixture mutation: ${_fixture_forbidden}")
    endif()
    if(NOT "${_fixture_stdout}${_fixture_stderr}" MATCHES
            "rejected forbidden token: ${_fixture_forbidden}")
        message(FATAL_ERROR "fixture mutation failed for the wrong reason: ${_fixture_forbidden}: ${_fixture_stdout}${_fixture_stderr}")
    endif()
endforeach()

set(_stage_mutation_index 0)
foreach(_stage_mutation_pair IN ITEMS
        "settings::StableKey::j|settings::StableKey::q|StableKey::j"
        "state.player_monster_hash_before <<|state.player_monster_hash_before_removed <<|state.player_monster_hash_before <<")
    string(REPLACE "|" ";" _stage_mutation_parts "${_stage_mutation_pair}")
    list(GET _stage_mutation_parts 0 _stage_before)
    list(GET _stage_mutation_parts 1 _stage_after)
    list(GET _stage_mutation_parts 2 _stage_expected)
    math(EXPR _stage_mutation_index "${_stage_mutation_index} + 1")
    set(_stage_mutation "${GUARD_TEST_ROOT}/stage-${_stage_mutation_index}.cpp")
    file(COPY_FILE "${_stage_source}" "${_stage_mutation}")
    file(READ "${_stage_mutation}" _stage_mutation_text)
    string(REPLACE "${_stage_before}" "${_stage_after}"
        _stage_mutation_text "${_stage_mutation_text}")
    string(APPEND _stage_mutation_text
        "\n// decoy ${_stage_before}\\\n")
    file(WRITE "${_stage_mutation}" "${_stage_mutation_text}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DSTAGE_OVERRIDE=${_stage_mutation}" -P "${_guard}"
        RESULT_VARIABLE _stage_mutation_result
        OUTPUT_VARIABLE _stage_mutation_stdout ERROR_VARIABLE _stage_mutation_stderr)
    if(_stage_mutation_result EQUAL 0)
        message(FATAL_ERROR "evidence guard self-test accepted Stage mutation: ${_stage_before}")
    endif()
    string(FIND "${_stage_mutation_stdout}${_stage_mutation_stderr}"
        "Stage11B evidence guard missing Stage" _stage_reason)
    string(FIND "${_stage_mutation_stdout}${_stage_mutation_stderr}"
        "${_stage_expected}" _stage_expected_failure)
    if(_stage_reason EQUAL -1 OR _stage_expected_failure EQUAL -1)
        message(FATAL_ERROR "Stage mutation failed for wrong reason: ${_stage_before}: ${_stage_mutation_stdout}${_stage_mutation_stderr}")
    endif()
endforeach()
# T7C-M07: the pause transition must stay before GetFrameTime, frame gating,
# and every fixed tick. Move the real call to each later executable anchor and
# require the dedicated ordering diagnostic.
set_property(GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT 0)
function(stage11b_expect_late_pause_transition NAME LATE_ANCHOR)
    file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" _source)
    set(_call
        "            validation_runtime->observe_pause_transition(\n                pause_was_open, pause_open);\n")
    set(_early_replacement "")
    if(ARGC GREATER 2)
        set(_decoy_kind "${ARGV2}")
        set(_call_text
            "validation_runtime->observe_pause_transition(pause_was_open, pause_open);")
        if(_decoy_kind STREQUAL "comment")
            set(_early_replacement "            // ${_call_text}\n")
        elseif(_decoy_kind STREQUAL "string")
            set(_early_replacement
                "            const char* m07_decoy = \"${_call_text}\";\n")
        elseif(_decoy_kind STREQUAL "raw")
            set(_early_replacement
                "            const char* m07_decoy = R\"guard(${_call_text})guard\";\n")
        elseif(_decoy_kind STREQUAL "inactive")
            set(_early_replacement
                "#if 0\n${_call}#endif\n")
        elseif(_decoy_kind STREQUAL "lambda")
            set(_early_replacement
                "            const auto m07_decoy = [&] { ${_call_text} };\n")
        elseif(_decoy_kind STREQUAL "dead")
            set(_early_replacement
                "            if (false) { ${_call_text} }\n")
        elseif(_decoy_kind STREQUAL "cross_scope")
            set(_early_replacement "")
    else()
        message(FATAL_ERROR "${NAME}: unknown M07 decoy ${_decoy_kind}")
    endif()
    endif()
    set(_before_call_replacement "${_source}")
    string(REPLACE "${_call}" "${_early_replacement}" _source "${_source}")
    if(_source STREQUAL _before_call_replacement)
        message(FATAL_ERROR "${NAME}: real pause-transition call anchor is missing")
    endif()
    string(FIND "${_source}" "${LATE_ANCHOR}" _anchor_index)
    if(_anchor_index EQUAL -1)
        message(FATAL_ERROR "${NAME}: late pause-transition anchor is missing")
    endif()
    string(REPLACE "${LATE_ANCHOR}" "${_call}${LATE_ANCHOR}"
        _source "${_source}")
    if(ARGC GREATER 2 AND ARGV2 STREQUAL "cross_scope")
        string(APPEND _source
            "\nvoid m07_cross_scope_decoy() {\n${_call}}\n")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/host-${NAME}.cpp")
    file(WRITE "${_mutation}" "${_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "${NAME}: evidence guard accepted late pause transition")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "T7C-M07")
        message(FATAL_ERROR
            "${NAME}: late pause transition failed for wrong reason: ${_stdout}${_stderr}")
    endif()
    get_property(_case_count GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT)
    math(EXPR _case_count "${_case_count} + 1")
    set_property(GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT ${_case_count})
endfunction()

stage11b_expect_late_pause_transition(m07_after_get_frame_time
    "            HostFrameGateResult host_gate{};")
stage11b_expect_late_pause_transition(m07_after_frame_gate
    "            const PassiveOverlayInputGate passive_input_gate =")
stage11b_expect_late_pause_transition(m07_after_fixed_tick
    "                validation_runtime->observe_fixed_tick();")
foreach(_m07_decoy_kind IN ITEMS
        comment string raw inactive lambda dead cross_scope)
    stage11b_expect_late_pause_transition(
        "m07_after_get_frame_time_${_m07_decoy_kind}_decoy"
        "            HostFrameGateResult host_gate{};"
        "${_m07_decoy_kind}")
endforeach()

# T7C-M22: this is an assignment of the pending visible value, never an OR
# latch. The runtime mutation must be rejected for that named reason.
function(stage11b_expect_m22_rejection NAME REPLACEMENT)
    file(READ "${_runtime_source}" _m22_runtime_source)
    set(_m22_anchor
        "impl_->states.stage11b.pause_capture_while_paused =\n        impl_->pending_stage11b_paused_visible_capture;")
    string(REPLACE "${_m22_anchor}" "${REPLACEMENT}"
        _m22_runtime_source "${_m22_runtime_source}")
    file(READ "${_runtime_source}" _m22_runtime_original)
    if(_m22_runtime_source STREQUAL _m22_runtime_original)
        message(FATAL_ERROR "${NAME}: T7C-M22 runtime assignment anchor is missing")
    endif()
    set(_m22_runtime_mutation
        "${GUARD_TEST_ROOT}/runtime-${NAME}.cpp")
    file(WRITE "${_m22_runtime_mutation}" "${_m22_runtime_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DRUNTIME_OVERRIDE=${_m22_runtime_mutation}" -P "${_guard}"
        RESULT_VARIABLE _m22_result OUTPUT_VARIABLE _m22_stdout
        ERROR_VARIABLE _m22_stderr)
    if(_m22_result EQUAL 0)
        message(FATAL_ERROR "${NAME}: T7C-M22 mutation was accepted")
    endif()
    if(NOT "${_m22_stdout}${_m22_stderr}" MATCHES "T7C-M22")
        message(FATAL_ERROR
            "${NAME}: T7C-M22 failed for wrong reason: ${_m22_stdout}${_m22_stderr}")
    endif()
    get_property(_case_count GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT)
    math(EXPR _case_count "${_case_count} + 1")
    set_property(GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT ${_case_count})
endfunction()

stage11b_expect_m22_rejection(m22_plain_or_latch
    "impl_->states.stage11b.pause_capture_while_paused =\n        impl_->states.stage11b.pause_capture_while_paused\n        || impl_->pending_stage11b_paused_visible_capture;")
stage11b_expect_m22_rejection(m22_parenthesized_or_latch
    "impl_->states.stage11b.pause_capture_while_paused =\n        (impl_->states.stage11b.pause_capture_while_paused)\n        || impl_->pending_stage11b_paused_visible_capture;")
stage11b_expect_m22_rejection(m22_split_expression
    "impl_->states.stage11b.pause_capture_while_paused = false;\n    impl_->pending_stage11b_paused_visible_capture;")

function(stage11b_expect_pause_cjk_rejection NAME SEARCH REPLACEMENT)
    file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" _source)
    string(FIND "${_source}" "${SEARCH}" _search_position)
    if(_search_position EQUAL -1)
        message(FATAL_ERROR "${NAME}: pause CJK mutation anchor is missing")
    endif()
    string(REPLACE "${SEARCH}" "${REPLACEMENT}" _mutated "${_source}")
    if(_mutated STREQUAL _source)
        message(FATAL_ERROR "${NAME}: pause CJK mutation made no change")
    endif()
    set(_mutation "${GUARD_TEST_ROOT}/host-${NAME}.cpp")
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "${NAME}: pause CJK mutation was accepted")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES "T7C pause CJK readiness")
        message(FATAL_ERROR
            "${NAME}: pause CJK mutation failed for wrong reason: ${_stdout}${_stderr}")
    endif()
    get_property(_case_count GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT)
    math(EXPR _case_count "${_case_count} + 1")
    set_property(GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT ${_case_count})
endfunction()

stage11b_expect_pause_cjk_rejection(pause_cjk_starts_true
    "bool pause_cjk_ready = false;" "bool pause_cjk_ready = true;")
stage11b_expect_pause_cjk_rejection(pause_cjk_before_draw
    "pause_menu_renderer.draw(pause_menu, renderer.material_pack());\n                pause_cjk_ready = pause_menu_renderer.has_cjk_font();"
    "pause_cjk_ready = pause_menu_renderer.has_cjk_font();\n                pause_menu_renderer.draw(pause_menu, renderer.material_pack());")
stage11b_expect_pause_cjk_rejection(pause_cjk_outside_pause_branch
    "pause_cjk_ready = pause_menu_renderer.has_cjk_font();\n            }\n            const PresentationDecision decision ="
    "}\n            pause_cjk_ready = pause_menu_renderer.has_cjk_font();\n            const PresentationDecision decision =")
set(_call_index 0)
foreach(_replacement_pair IN ITEMS
        "platform::run_raylib_host(config)|platform::run_raylib_host_removed(config)"
        "std::system(command.c_str())|std::system_removed(command.c_str())")
    string(REPLACE "|" ";" _replacement "${_replacement_pair}")
    list(GET _replacement 0 _before)
    list(GET _replacement 1 _after)
    string(REPLACE "${_before}" "${_after}" _replaced_formal "${_formal_source}")
    math(EXPR _call_index "${_call_index} + 1")
    set(_formal_mutation "${GUARD_TEST_ROOT}/formal-call-${_call_index}.cpp")
    file(WRITE "${_formal_mutation}" "${_replaced_formal}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DFORMAL_OVERRIDE=${_formal_mutation}" -P "${_guard}"
        RESULT_VARIABLE _call_result OUTPUT_VARIABLE _call_stdout ERROR_VARIABLE _call_stderr)
    if(_call_result EQUAL 0)
        message(FATAL_ERROR "evidence guard accepted removed formal call: ${_before}")
    endif()
    if(NOT "${_call_stdout}${_call_stderr}" MATCHES "requires")
        message(FATAL_ERROR "formal call mutation failed for the wrong reason: ${_before}: ${_call_stdout}${_call_stderr}")
    endif()
endforeach()
set(_host_mutation "${GUARD_TEST_ROOT}/raylib_host.cpp")
file(COPY_FILE "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" "${_host_mutation}")
file(APPEND "${_host_mutation}" "\n// mutation: forbidden validation focus/snapshot bypass\nsnapshot.down.fill(false);\nsnapshot.pressed.fill(false);\nsnapshot.escape = false;\nsnapshot.enter = false;\npause_input.focus_lost = false;\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_host_mutation}" -P "${_guard}"
    RESULT_VARIABLE _host_result OUTPUT_VARIABLE _host_stdout ERROR_VARIABLE _host_stderr)
if(_host_result EQUAL 0)
    message(FATAL_ERROR "evidence guard self-test accepted the host bypass mutation")
endif()
if(NOT "${_host_stdout}${_host_stderr}" MATCHES "rejected host bypass")
    message(FATAL_ERROR "host bypass mutation failed for the wrong reason: ${_host_stdout}${_host_stderr}")
endif()

function(stage11b_expect_host_count_mutation NAME ANCHOR REPLACEMENT EXPECTED)
    file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp"
        _mutated_source)
    string(REPLACE "${ANCHOR}" "${REPLACEMENT}"
        _mutated_source "${_mutated_source}")
    file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp"
        _original_source)
    if(_mutated_source STREQUAL _original_source)
        message(FATAL_ERROR "${NAME}: real Host count-mutation anchor is missing")
    endif()
    set(_mutated_host "${GUARD_TEST_ROOT}/host-count-${NAME}.cpp")
    file(WRITE "${_mutated_host}" "${_mutated_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutated_host}" -P "${_guard}"
        RESULT_VARIABLE _mutation_result OUTPUT_VARIABLE _mutation_stdout ERROR_VARIABLE _mutation_stderr)
    if(_mutation_result EQUAL 0)
        message(FATAL_ERROR "evidence guard self-test accepted count mutation: ${NAME}")
    endif()
    if(NOT "${_mutation_stdout}${_mutation_stderr}" MATCHES "${EXPECTED}")
        message(FATAL_ERROR "count mutation failed for wrong reason: ${NAME}: ${_mutation_stdout}${_mutation_stderr}")
    endif()
endfunction()

stage11b_expect_host_count_mutation(duplicate_screen_capture
    "Image image = LoadImageFromScreen();"
    "Image image = LoadImageFromScreen();\n    Image duplicate_image = LoadImageFromScreen();"
    "Stage11B-count-capture")
set(_fixed_tick_anchor
    "runtime.fixed_tick(step_movement,\n                    loot_pickup_policy(live_settings.loot_filter_mode));")
stage11b_expect_host_count_mutation(duplicate_fixed_tick
    "${_fixed_tick_anchor}"
    "runtime.fixed_tick({});\n                ${_fixed_tick_anchor}"
    "Stage11B-count-fixed")

set(_submitted_runtime_mutation
    "${GUARD_TEST_ROOT}/runtime-submitted-count.cpp")
file(READ "${_runtime_source}" _submitted_runtime_source)
string(REPLACE
    "submitted_actions.combat[0] ? 1U : 0U"
    "submitted_actions.combat[1] ? 1U : 0U"
    _submitted_runtime_source "${_submitted_runtime_source}")
string(APPEND _submitted_runtime_source
    "\n// decoy submitted_actions.combat[0] ? 1U : 0U\n")
file(WRITE "${_submitted_runtime_mutation}" "${_submitted_runtime_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DRUNTIME_OVERRIDE=${_submitted_runtime_mutation}" -P "${_guard}"
    RESULT_VARIABLE _submitted_runtime_result
    OUTPUT_VARIABLE _submitted_runtime_stdout
    ERROR_VARIABLE _submitted_runtime_stderr)
if(_submitted_runtime_result EQUAL 0)
    message(FATAL_ERROR
        "evidence guard self-test accepted submitted-action runtime mutation")
endif()
if(NOT "${_submitted_runtime_stdout}${_submitted_runtime_stderr}" MATCHES
        "Stage11B evidence guard requires accepted queue_action evidence")
    message(FATAL_ERROR
        "submitted-action runtime mutation failed for wrong reason: ${_submitted_runtime_stdout}${_submitted_runtime_stderr}")
endif()

set(_submitted_host_mutation
    "${GUARD_TEST_ROOT}/host-submitted-observer.cpp")
file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp"
    _submitted_host_source)
string(REPLACE
    "validation_runtime->observe_submitted_actions(submitted_actions);"
    "validation_runtime->observe_submitted_actions_removed(submitted_actions);"
    _submitted_host_source "${_submitted_host_source}")
string(APPEND _submitted_host_source
    "\n// decoy validation_runtime->observe_submitted_actions(submitted_actions);\n")
file(WRITE "${_submitted_host_mutation}" "${_submitted_host_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_submitted_host_mutation}" -P "${_guard}"
    RESULT_VARIABLE _submitted_host_result
    OUTPUT_VARIABLE _submitted_host_stdout
    ERROR_VARIABLE _submitted_host_stderr)
if(_submitted_host_result EQUAL 0)
    message(FATAL_ERROR
        "evidence guard self-test accepted removed submitted-action facade call")
endif()
if(NOT "${_submitted_host_stdout}${_submitted_host_stderr}" MATCHES
        "Stage11B evidence guard requires accepted queue_action evidence")
    message(FATAL_ERROR
        "submitted-action Host mutation failed for wrong reason: ${_submitted_host_stdout}${_submitted_host_stderr}")
endif()

set(_decoy_policy_host "${GUARD_TEST_ROOT}/host-decoy-loot-policy.cpp")
file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp"
    _decoy_policy_source)
string(REPLACE
    "runtime.fixed_tick(step_movement,\n                    loot_pickup_policy(live_settings.loot_filter_mode));"
    "runtime.fixed_tick(step_movement, {});\n                static_cast<void>(loot_pickup_policy(live_settings.loot_filter_mode));"
    _decoy_policy_source "${_decoy_policy_source}")
file(WRITE "${_decoy_policy_host}" "${_decoy_policy_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_decoy_policy_host}" -P "${_guard}"
    RESULT_VARIABLE _decoy_policy_result
    OUTPUT_VARIABLE _decoy_policy_stdout
    ERROR_VARIABLE _decoy_policy_stderr)
if(_decoy_policy_result EQUAL 0)
    message(FATAL_ERROR
        "evidence guard self-test accepted default policy with live-policy decoy")
endif()
if(NOT "${_decoy_policy_stdout}${_decoy_policy_stderr}" MATCHES
        "requires live loot policy behind host gate")
    message(FATAL_ERROR
        "decoy loot policy mutation failed for wrong reason: ${_decoy_policy_stdout}${_decoy_policy_stderr}")
endif()

set(_draft_policy_host "${GUARD_TEST_ROOT}/host-draft-loot-policy.cpp")
file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp"
    _draft_policy_source)
string(REPLACE
    "loot_pickup_policy(live_settings.loot_filter_mode)"
    "loot_pickup_policy(pause_menu.draft.loot_filter_mode)"
    _draft_policy_source "${_draft_policy_source}")
file(WRITE "${_draft_policy_host}" "${_draft_policy_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_draft_policy_host}" -P "${_guard}"
    RESULT_VARIABLE _draft_policy_result
    OUTPUT_VARIABLE _draft_policy_stdout
    ERROR_VARIABLE _draft_policy_stderr)
if(_draft_policy_result EQUAL 0)
    message(FATAL_ERROR
        "evidence guard self-test accepted draft loot pickup policy")
endif()
if(NOT "${_draft_policy_stdout}${_draft_policy_stderr}" MATCHES
        "rejects draft loot policy in fixed_tick")
    message(FATAL_ERROR
        "draft loot policy mutation failed for wrong reason: ${_draft_policy_stdout}${_draft_policy_stderr}")
endif()

get_property(_task7c_case_count GLOBAL PROPERTY STAGE11B_TASK7C_CASE_COUNT)
if(NOT _task7c_case_count EQUAL 16)
    message(FATAL_ERROR
        "Stage11B Task7C mutation inventory drifted: expected 16, got ${_task7c_case_count}")
endif()
message(STATUS
    "Stage11B Task7C cases=16: M07=10, M22=3, pause-CJK=3")
