if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11d_loot_evidence_guard_test.cmake")
set(_sequence_guard
    "${SOURCE_ROOT}/tests/platform/host_validation_sequence_guard_test.cmake")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
get_filename_component(_host_source_dir "${_host}" DIRECTORY)
set(_host_validation_runtime
    "${_host_source_dir}/host_validation_runtime.cpp")
set(_stage_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp")
set(_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_runtime.cpp")
set(_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_game_validation.cpp")
set(_validator "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_validator.ps1")
foreach(_file IN ITEMS "${_guard}" "${_sequence_guard}" "${_host}"
        "${_host_validation_runtime}" "${_stage_header}" "${_runtime}"
        "${_report}" "${_renderer}" "${_formal}" "${_validator}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11D guard self-test input is missing: ${_file}")
    endif()
endforeach()
file(REMOVE_RECURSE "${GUARD_TEST_ROOT}")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    -P "${_guard}" RESULT_VARIABLE _baseline OUTPUT_QUIET ERROR_QUIET)
if(NOT _baseline EQUAL 0)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected its baseline")
endif()

function(expect_rejected NAME OVERRIDE PATH EXPECTED_REASON)
    if(DEFINED TASK5A_TARGETED_ONLY AND TASK5A_TARGETED_ONLY)
        set(_targeted_names
            "stage11d header state payload cross scope"
            "runtime selector payload cross scope"
            "host pause call in uncalled lambda"
            "host fixed-step activation payload cross scope"
            "host real moved definition")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    elseif(DEFINED TASK5B_TARGETED_ONLY AND TASK5B_TARGETED_ONLY)
        set(_targeted_names
            "report semantic recorder comment decoy"
            "report target evaluator string decoy"
            "report summary forward declaration decoy"
            "report semantic payload cross function decoy")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    endif()
    set(_guard_options "")
    if(OVERRIDE STREQUAL "HOST_VALIDATION_RUNTIME")
        list(APPEND _guard_options "-DSTAGE11D_INPUT_OWNER_ONLY=ON")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-D${OVERRIDE}_OVERRIDE=${PATH}" ${_guard_options} -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11D loot evidence guard accepted mutation: ${NAME}")
    endif()
    set(_log "${_output}\n${_error}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_log "${_log}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_reason
        "${EXPECTED_REASON}")
    string(FIND "${_normalized_log}" "${_normalized_reason}" _reason)
    if(_reason EQUAL -1)
        message(FATAL_ERROR
            "Stage11D guard rejected ${NAME} for the wrong reason: ${_log}")
    endif()
endfunction()

file(READ "${_report}" _task5b_report_text)
string(REPLACE "\r\n" "\n" _task5b_report_text
    "${_task5b_report_text}")

string(REPLACE "void stage11d_record_semantics("
    "// void stage11d_record_semantics(\nvoid task5b_record_removed("
    _task5b_report_record_comment "${_task5b_report_text}")
if(_task5b_report_record_comment STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report recorder comment mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/report-record-comment-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_record_comment}")
expect_rejected("report semantic recorder comment decoy" REPORT "${_path}"
    "report semantic recorder definition token inventory")

string(REPLACE "bool stage11d_target_visible("
    "const char* task5b_target_string = \"bool stage11d_target_visible(\";\nbool task5b_target_removed("
    _task5b_report_target_string "${_task5b_report_text}")
if(_task5b_report_target_string STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report target string mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/report-target-string-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_target_string}")
expect_rejected("report target evaluator string decoy" REPORT "${_path}"
    "report target-visible evaluator definition token inventory")

string(REPLACE "void write_stage11d_loot_validation_summary("
    "void write_stage11d_loot_validation_summary();\nvoid task5b_summary_removed("
    _task5b_report_summary_forward "${_task5b_report_text}")
if(_task5b_report_summary_forward STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report summary forward mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/report-summary-forward-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_summary_forward}")
expect_rejected("report summary forward declaration decoy" REPORT "${_path}"
    "missing report summary writer definition")

set(_task5b_affix_payload
    "            state.monster_affix_danger[ordinal] =\n                combat::monster_affix_danger_score(monster.affixes);")
string(REPLACE "${_task5b_affix_payload}" "            static_cast<void>(ordinal);"
    _task5b_report_payload_removed "${_task5b_report_text}")
if(_task5b_report_payload_removed STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report payload mutation made no change")
endif()
set(_task5b_report_close "}  // namespace arpg::platform::host_validation")
set(_task5b_cross_function
    "void task5b_report_payload_decoy() {\n    ${_task5b_affix_payload}\n}\n\n${_task5b_report_close}")
string(REPLACE "${_task5b_report_close}" "${_task5b_cross_function}"
    _task5b_report_payload_cross_function "${_task5b_report_payload_removed}")
set(_path "${GUARD_TEST_ROOT}/report-payload-cross-function-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_payload_cross_function}")
expect_rejected("report semantic payload cross function decoy" REPORT "${_path}"
    "missing report evaluator semantic: state.monster_affix_danger")

if(DEFINED TASK5B_TARGETED_ONLY AND TASK5B_TARGETED_ONLY)
    message(STATUS
        "Stage11D Task5B targeted evidence guard passed: bad_mutations=4")
    return()
endif()

function(expect_guard_accepted NAME GUARD OVERRIDE PATH)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-D${OVERRIDE}_OVERRIDE=${PATH}" -P "${GUARD}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "${NAME} was rejected: ${_output}\n${_error}")
    endif()
endfunction()

function(expect_sequence_rejected NAME PATH EXPECTED_REASON)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${PATH}" -P "${_sequence_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(_result EQUAL 0)
        message(FATAL_ERROR "host sequence guard accepted mutation: ${NAME}")
    endif()
    set(_log "${_output}\n${_error}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_log "${_log}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_reason
        "${EXPECTED_REASON}")
    string(FIND "${_normalized_log}" "${_normalized_reason}" _reason)
    if(_reason EQUAL -1)
        message(FATAL_ERROR
            "host sequence guard rejected ${NAME} for the wrong reason: ${_log}")
    endif()
endfunction()

# Task 5A relocation RED: before the runtime exists, mutate the exact source
# region that will move. The legacy guard ignores RUNTIME_OVERRIDE, which is
# itself part of the RED proof. After extraction these same named mutations
# exercise the real runtime source.
if(EXISTS "${_runtime}")
    file(READ "${_runtime}" _task5a_runtime_text)
else()
    file(READ "${_host}" _task5a_runtime_text)
endif()
set(_task5a_driver_open
    "Stage11DLootValidationState& state) noexcept {\n    using Scenario = Stage11DLootValidationScenario;")
set(_task5a_driver_close
    "    return snapshot;\n}\n// STAGE11D_LOOT_VALIDATION_SEAM_END physical_driver")
string(REPLACE "${_task5a_driver_open}"
    "Stage11DLootValidationState& state) noexcept {\n    const auto stage11d_driver_decoy = [&]() noexcept {\n    using Scenario = Stage11DLootValidationScenario;"
    _task5a_driver_lambda "${_task5a_runtime_text}")
string(REPLACE "${_task5a_driver_close}"
    "    return snapshot;\n    };\n    return snapshot;\n}\n// STAGE11D_LOOT_VALIDATION_SEAM_END physical_driver"
    _task5a_driver_lambda "${_task5a_driver_lambda}")
if(_task5a_driver_lambda STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-driver-lambda mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-driver-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task5a_driver_lambda}")
expect_rejected("runtime driver in uncalled lambda" RUNTIME "${_path}"
    "runtime physical driver scope")

set(_task5a_driver_signature
    "PhysicalKeySnapshot inject_stage11d_physical_edges(")
string(REPLACE "${_task5a_driver_signature}"
    "// ${_task5a_driver_signature}\nPhysicalKeySnapshot task5a_disabled_stage11d_physical_edges("
    _task5a_driver_comment "${_task5a_runtime_text}")
if(_task5a_driver_comment STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-driver-comment mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-driver-comment-decoy.cpp")
file(WRITE "${_path}" "${_task5a_driver_comment}")
expect_rejected("runtime driver comment decoy" RUNTIME "${_path}"
    "runtime physical driver definition")

string(REPLACE "${_task5a_driver_signature}"
    "constexpr const char* task5a_driver_signature = \"${_task5a_driver_signature}\";\nPhysicalKeySnapshot task5a_disabled_stage11d_physical_edges("
    _task5a_driver_string "${_task5a_runtime_text}")
if(_task5a_driver_string STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-driver-string mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-driver-string-decoy.cpp")
file(WRITE "${_path}" "${_task5a_driver_string}")
expect_rejected("runtime driver string decoy" RUNTIME "${_path}"
    "runtime physical driver definition")

function(task5a_extract_seam SOURCE LABEL OUT_SEAM)
    set(_begin "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN ${LABEL}")
    set(_end "// STAGE11D_LOOT_VALIDATION_SEAM_END ${LABEL}")
    string(FIND "${SOURCE}" "${_begin}" _begin_at)
    string(FIND "${SOURCE}" "${_end}" _end_at)
    if(_begin_at EQUAL -1 OR _end_at EQUAL -1 OR NOT _begin_at LESS _end_at)
        message(FATAL_ERROR "Task5A ${LABEL} mutation seam disappeared")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${SOURCE}" ${_begin_at} ${_length} _seam)
    set(${OUT_SEAM} "${_seam}" PARENT_SCOPE)
endfunction()

file(READ "${_stage_header}" _task5a_header_text)
task5a_extract_seam("${_task5a_header_text}" state
    _task5a_header_state_seam)
string(REPLACE "${_task5a_header_state_seam}"
    "namespace task5a_state_decoy {\n${_task5a_header_state_seam}\n}  // namespace task5a_state_decoy"
    _task5a_header_cross_scope "${_task5a_header_text}")
set(_path "${GUARD_TEST_ROOT}/stage11d-header-state-cross-scope.hpp")
file(WRITE "${_path}" "${_task5a_header_cross_scope}")
expect_rejected("stage11d header state cross scope" STAGE11D_HEADER "${_path}"
    "stage_header state seam scope")

set(_task5a_state_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN state")
set(_task5a_state_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END state")
string(REPLACE "${_task5a_state_begin}" "" _task5a_header_state_body
    "${_task5a_header_state_seam}")
string(REPLACE "${_task5a_state_end}" "" _task5a_header_state_body
    "${_task5a_header_state_body}")
set(_task5a_header_nested_state
    "${_task5a_state_begin}\nnamespace task5a_nested_state {${_task5a_header_state_body}\n}  // namespace task5a_nested_state\n${_task5a_state_end}")
string(REPLACE "${_task5a_header_state_seam}"
    "${_task5a_header_nested_state}" _task5a_header_payload_cross_scope
    "${_task5a_header_text}")
if(_task5a_header_payload_cross_scope STREQUAL _task5a_header_text)
    message(FATAL_ERROR "stage11d-header-state-payload mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-header-state-payload-cross-scope.hpp")
file(WRITE "${_path}" "${_task5a_header_payload_cross_scope}")
expect_rejected("stage11d header state payload cross scope" STAGE11D_HEADER
    "${_path}" "stage_header state definition scope")

task5a_extract_seam("${_task5a_runtime_text}" selectors
    _task5a_runtime_selectors_seam)
string(REPLACE "${_task5a_runtime_selectors_seam}"
    "namespace task5a_selector_decoy {\n${_task5a_runtime_selectors_seam}\n}  // namespace task5a_selector_decoy"
    _task5a_selector_cross_scope "${_task5a_runtime_text}")
set(_path "${GUARD_TEST_ROOT}/runtime-selector-cross-scope.cpp")
file(WRITE "${_path}" "${_task5a_selector_cross_scope}")
expect_rejected("runtime selector cross scope" RUNTIME "${_path}"
    "runtime selectors seam scope")

set(_task5a_selectors_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN selectors")
set(_task5a_selectors_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END selectors")
string(REPLACE "${_task5a_selectors_begin}" "" _task5a_selectors_body
    "${_task5a_runtime_selectors_seam}")
string(REPLACE "${_task5a_selectors_end}" "" _task5a_selectors_body
    "${_task5a_selectors_body}")
set(_task5a_nested_selectors
    "${_task5a_selectors_begin}\nnamespace task5a_nested_selectors {${_task5a_selectors_body}\n}  // namespace task5a_nested_selectors\n${_task5a_selectors_end}")
string(REPLACE "${_task5a_runtime_selectors_seam}"
    "${_task5a_nested_selectors}" _task5a_selector_payload_cross_scope
    "${_task5a_runtime_text}")
if(_task5a_selector_payload_cross_scope STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-selector-payload mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-selector-payload-cross-scope.cpp")
file(WRITE "${_path}" "${_task5a_selector_payload_cross_scope}")
expect_rejected("runtime selector payload cross scope" RUNTIME "${_path}"
    "runtime ordinary rarity selector definition scope")

# Task 5A relocation RED: the actual host-loop call may not be replaced with
# an uncalled nested decoy while a pass-through snapshot feeds input mapping.
file(READ "${_host}" _task5a_red_host_text)
string(REPLACE "\r\n" "\n" _task5a_red_host_text
    "${_task5a_red_host_text}")
set(_task5a_host_include "#include \"host_validation_stage11d.hpp\"")
set(_task5a_host_definition_decoys
    "${_task5a_host_include}\n// bool stage11d_validation_active(\nconstexpr const char* task5a_stage11d_definition_decoy = \"bool stage11d_validation_active(\";")
string(REPLACE "${_task5a_host_include}" "${_task5a_host_definition_decoys}"
    _task5a_host_harmless_definition_decoys "${_task5a_red_host_text}")
if(_task5a_host_harmless_definition_decoys STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host harmless definition decoys made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-harmless-definition-decoys.cpp")
file(WRITE "${_path}" "${_task5a_host_harmless_definition_decoys}")
expect_guard_accepted("Stage11D evidence guard harmless comment/string decoys"
    "${_guard}" HOST "${_path}")
expect_guard_accepted("host sequence guard harmless comment/string decoys"
    "${_sequence_guard}" HOST "${_path}")
set(_task5a_host_real_definition
    "${_task5a_host_include}\nbool stage11d_validation_active(\n    const RaylibHostConfig&) noexcept { return false; }")
string(REPLACE "${_task5a_host_include}" "${_task5a_host_real_definition}"
    _task5a_host_with_real_definition "${_task5a_red_host_text}")
if(_task5a_host_with_real_definition STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host real moved definition mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-real-moved-definition.cpp")
file(WRITE "${_path}" "${_task5a_host_with_real_definition}")
expect_rejected("host real moved definition" HOST "${_path}"
    "runtime definition in host")
expect_sequence_rejected("host real moved definition" "${_path}"
    "runtime definition remains in raylib_host.cpp")
file(READ "${_host_validation_runtime}" _task7b_host_validation_runtime_text)
string(REPLACE "\r\n" "\n" _task7b_host_validation_runtime_text
    "${_task7b_host_validation_runtime_text}")
set(_task7b_stage11d_input_leg [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11d);]=])
set(_task7b_stage11d_passthrough [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        stage11c_physical_keys;]=])
string(FIND "${_task7b_host_validation_runtime_text}"
    "${_task7b_stage11d_input_leg}" _task7b_stage11d_input_leg_position)
if(_task7b_stage11d_input_leg_position EQUAL -1)
    message(FATAL_ERROR "facade Stage11D input mutation site disappeared")
endif()

function(task7b_expect_stage11d_input_rejected NAME SLUG REPLACEMENT)
    string(REPLACE "${_task7b_stage11d_input_leg}" "${REPLACEMENT}"
        _mutated "${_task7b_host_validation_runtime_text}")
    if(_mutated STREQUAL _task7b_host_validation_runtime_text)
        message(FATAL_ERROR "${NAME} mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/${SLUG}.cpp")
    file(WRITE "${_path}" "${_mutated}")
    expect_rejected("${NAME}" HOST_VALIDATION_RUNTIME "${_path}"
        "rejected host input call binding")
endfunction()

set(_task7b_input_lambda [=[    const auto stage11d_input_decoy = [&]() noexcept {
]=])
string(APPEND _task7b_input_lambda
    "${_task7b_stage11d_input_leg}\n        return stage11d_physical_keys;\n    };\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input in uncalled lambda"
    "facade-stage11d-input-uncalled-lambda"
    "${_task7b_input_lambda}")

set(_task7b_input_comment
    "/*\n${_task7b_stage11d_input_leg}\n*/\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input comment decoy"
    "facade-stage11d-input-comment-decoy"
    "${_task7b_input_comment}")

set(_task7b_input_string [=[    constexpr const char* stage11d_input_decoy =
        "host_validation::inject_stage11d_physical_edges("
        "stage11c_physical_keys, *impl_->config, input_settings, "
        "dungeon_snapshot, impl_->states.stage11d);";
]=])
string(APPEND _task7b_input_string "${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input string decoy"
    "facade-stage11d-input-string-decoy"
    "${_task7b_input_string}")

set(_task7b_input_raw
    "    constexpr const char* stage11d_input_decoy = R\"TASK7B(\n${_task7b_stage11d_input_leg}\n)TASK7B\";\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input raw-string decoy"
    "facade-stage11d-input-raw-string-decoy"
    "${_task7b_input_raw}")

set(_task7b_input_inactive
    "#if 0\n${_task7b_stage11d_input_leg}\n#endif\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input inactive decoy"
    "facade-stage11d-input-inactive-decoy"
    "${_task7b_input_inactive}")

set(_task7b_input_cross_scope "${_task7b_stage11d_passthrough}")
string(REPLACE "${_task7b_stage11d_input_leg}"
    "${_task7b_input_cross_scope}" _task7b_cross_scope_mutation
    "${_task7b_host_validation_runtime_text}")
string(APPEND _task7b_cross_scope_mutation
    "\nnamespace task7b_input_decoy {\n${_task7b_stage11d_input_leg}\n}\n")
set(_path "${GUARD_TEST_ROOT}/facade-stage11d-input-cross-scope.cpp")
file(WRITE "${_path}" "${_task7b_cross_scope_mutation}")
expect_rejected("facade Stage11D input cross-scope decoy"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected host input call binding")

set(_task7b_input_wrong_argument [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            impl_->death_input_snapshot, impl_->states.stage11d);]=])
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input wrong argument"
    "facade-stage11d-input-wrong-argument"
    "${_task7b_input_wrong_argument}")

set(_task7b_host_crop_end "core::FixedStepFrame frame = host_gate.fixed_step;")
string(FIND "${_task5a_red_host_text}" "${_task7b_host_crop_end}"
    _task7b_host_crop_end_position)
if(_task7b_host_crop_end_position EQUAL -1)
    message(FATAL_ERROR "host facade input duplicate mutation site disappeared")
endif()
set(_task7b_host_spliced_facade_input [=[            static_cast<void>(validation_runtime->inject_phy\
sical_edges(
                sampled_physical_keys, input_settings, current,
                gameplay_rearm_was_required));]=])
string(REPLACE "${_task7b_host_crop_end}"
    "${_task7b_host_crop_end}\n${_task7b_host_spliced_facade_input}"
    _task7b_host_duplicate_facade_input "${_task5a_red_host_text}")
if(_task7b_host_duplicate_facade_input STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host facade input duplicate mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-facade-input-duplicate-outside-crop.cpp")
file(WRITE "${_path}" "${_task7b_host_duplicate_facade_input}")
expect_rejected("host facade input duplicate outside crop" HOST "${_path}"
    "rejected host input call binding")

set(_task5a_pause_call
    "const PauseCommand pause_command = update_pause_menu(\n                pause_menu, pause_context, pause_input);")
string(REPLACE "${_task5a_pause_call}"
    "const auto task5a_pause_decoy = [&]() noexcept {\n                ${_task5a_pause_call}\n                return pause_command;\n            };\n            const PauseCommand pause_command{};"
    _task5a_pause_lambda "${_task5a_red_host_text}")
if(_task5a_pause_lambda STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host-pause-lambda mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-pause-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task5a_pause_lambda}")
expect_rejected("host pause call in uncalled lambda" HOST "${_path}"
    "host pause call scope")

task5a_extract_seam("${_task5a_red_host_text}" fixed_step
    _task5a_fixed_step_seam)
string(REPLACE "${_task5a_fixed_step_seam}"
    "/*\n${_task5a_fixed_step_seam}\n*/\n                        || false"
    _task5a_fixed_step_comment "${_task5a_red_host_text}")
set(_path "${GUARD_TEST_ROOT}/host-fixed-step-comment-decoy.cpp")
file(WRITE "${_path}" "${_task5a_fixed_step_comment}")
expect_rejected("host fixed-step activation comment decoy" HOST "${_path}"
    "cannot bind host fixed_step seam marker")

set(_task5a_fixed_step_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN fixed_step")
set(_task5a_fixed_step_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END fixed_step")
set(_task5a_nested_fixed_step
    "${_task5a_fixed_step_begin}\n                        || ([&]() noexcept {\n                            if (false) {\n                                return host_validation::stage11d_validation_active(config);\n                            }\n                            return false;\n                        }())\n${_task5a_fixed_step_end}")
string(REPLACE "${_task5a_fixed_step_seam}"
    "${_task5a_nested_fixed_step}" _task5a_fixed_step_payload_cross_scope
    "${_task5a_red_host_text}")
if(_task5a_fixed_step_payload_cross_scope STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host-fixed-step-payload mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-fixed-step-payload-cross-scope.cpp")
file(WRITE "${_path}" "${_task5a_fixed_step_payload_cross_scope}")
expect_rejected("host fixed-step activation payload cross scope" HOST "${_path}"
    "host fixed-step activation call scope")

task5a_extract_seam("${_task5a_red_host_text}" abyss_claim
    _task5a_abyss_seam)
string(REPLACE "${_task5a_abyss_seam}"
    "const auto task5a_abyss_decoy = [&]() noexcept {\n${_task5a_abyss_seam}\n                };"
    _task5a_abyss_lambda "${_task5a_red_host_text}")
set(_path "${GUARD_TEST_ROOT}/host-abyss-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task5a_abyss_lambda}")
expect_rejected("host abyss observer in uncalled lambda" HOST "${_path}"
    "host abyss_claim seam scope")

string(REPLACE "${_task5a_abyss_seam}" "" _task5a_abyss_early
    "${_task5a_red_host_text}")
string(REPLACE "                runtime.fixed_tick(step_movement,"
    "${_task5a_abyss_seam}\n                runtime.fixed_tick(step_movement,"
    _task5a_abyss_early "${_task5a_abyss_early}")
set(_path "${GUARD_TEST_ROOT}/host-abyss-before-fixed-tick.cpp")
file(WRITE "${_path}" "${_task5a_abyss_early}")
expect_rejected("host abyss observer before fixed tick" HOST "${_path}"
    "abyss claim observation order")

file(READ "${_formal}" _formal_text)
set(_site "platform::RaylibHostConfig config{};")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "private-injection mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "TestAccess::inject(config);\n    ${_site}" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "private-injection mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/private-injection.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("private injection" FORMAL "${_path}" "formal bypass: TestAccess")

string(REPLACE "${_site}"
    "FakeRenderer fake_renderer{};\n    ${_site}" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "fake-renderer mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/fake-renderer.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("non-production renderer" FORMAL "${_path}"
    "formal bypass: FakeRenderer")

set(_site "return platform::run_raylib_host(config)")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "direct-pickup mutation site disappeared")
endif()
string(REPLACE "${_site}" "request_pickup(0);\n    ${_site}"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "direct-pickup mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/direct-pickup.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("direct pickup completion" FORMAL "${_path}"
    "formal bypass: request_pickup(")

set(_site "draft.loot_filter_mode = spec.mode;")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "fake-settings mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "pause_menu.committed.loot_filter_mode = spec.mode;"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "fake-settings mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/fake-settings.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("fake settings publication" FORMAL "${_path}"
    "pause_menu.committed")

set(_site "|| absolute.filename() != \"stage11d loot evidence\"")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "unsafe-root mutation site disappeared")
endif()
string(REPLACE "${_site}" "|| false" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "unsafe-root mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/unsafe-root.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("malicious evidence root acceptance" FORMAL "${_path}"
    "formal cleanup safety is incomplete: absolute.filename()")

set(_site "FILE_ATTRIBUTE_REPARSE_POINT")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "reparse-safety mutation site disappeared")
endif()
string(REPLACE "${_site}" "FILE_ATTRIBUTE_DIRECTORY"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "reparse-safety mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/reparse-safety.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("reparse-point evidence escape" FORMAL "${_path}"
    "formal cleanup safety is incomplete: FILE_ATTRIBUTE_REPARSE_POINT")

set(_site "std::filesystem::create_directories(root, error);")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "recursive-cleanup mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "std::filesystem::remove_all(root, error);\n    ${_site}"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "recursive-cleanup mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/recursive-cleanup.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("recursive evidence cleanup" FORMAL "${_path}"
    "formal bypass: remove_all(")

file(READ "${_host}" _host_text)
string(REPLACE "\r\n" "\n" _host_text "${_host_text}")
function(expect_host_insert NAME SLUG SITE INSERT EXPECTED_REASON)
    string(FIND "${_host_text}" "${SITE}" _site_index)
    if(_site_index EQUAL -1)
        message(FATAL_ERROR "${NAME} mutation site disappeared")
    endif()
    string(LENGTH "${SITE}" _site_length)
    math(EXPR _after_site "${_site_index} + ${_site_length}")
    string(SUBSTRING "${_host_text}" ${_after_site} -1 _after_text)
    string(FIND "${_after_text}" "${SITE}" _second_site)
    if(NOT _second_site EQUAL -1)
        message(FATAL_ERROR "${NAME} mutation site is not unique")
    endif()
    string(REPLACE "${SITE}" "${SITE}\n                    ${INSERT}"
        _mutated "${_host_text}")
    if(_mutated STREQUAL _host_text)
        message(FATAL_ERROR "${NAME} mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/${SLUG}.cpp")
    file(WRITE "${_path}" "${_mutated}")
    expect_rejected("${NAME}" HOST "${_path}" "${EXPECTED_REASON}")
endfunction()

set(_host_scope_site
    "if (stage11d_validation_state.target_visible")
expect_host_insert("host private access" "host-private-access"
    "${_host_scope_site}" "const auto* TestAccess = session;"
    "host-bypass-TestAccess")
expect_host_insert("host snapshot override" "host-snapshot-override"
    "${_host_scope_site}" "const auto snapshot_override = current;"
    "host-bypass-snapshot_override")
expect_host_insert("host direct pickup request" "host-direct-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->request_pickup(0U));"
    "host-bypass-request_pickup(")
expect_host_insert("host direct pickup completion" "host-complete-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->complete_pickup(0U));"
    "host-bypass-complete_pickup(")
expect_host_insert("host direct pickup publication" "host-publish-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->publish_pickup(0U));"
    "host-bypass-publish_pickup(")
expect_host_insert("host committed settings write" "host-committed-settings"
    "${_host_scope_site}"
    "pause_menu.committed.loot_filter_mode = settings::LootFilterMode::rare_only;"
    "host validation seam direct committed settings write")
expect_host_insert("host live settings write" "host-live-settings"
    "${_host_scope_site}"
    "live_settings.loot_filter_mode = settings::LootFilterMode::rare_only;"
    "host validation seam direct live settings write")
expect_host_insert("host direct result pass" "host-result-pass"
    "${_host_scope_site}" "const char* result = \"result=pass\";"
    "host validation seam direct result pass")

set(_site "const auto& item = current.ground_items[index];")
string(FIND "${_task5a_runtime_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "snapshot-mutation site disappeared")
endif()
string(REPLACE "${_site}" "current.ground_items[0] = fabricated;"
    _mutated "${_task5a_runtime_text}")
if(_mutated STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "snapshot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/snapshot-mutation.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("snapshot mutation" RUNTIME "${_path}"
    "rejected snapshot mutation")

set(_site "EndDrawing();\n    if (path == nullptr) return true;")
string(FIND "${_host_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "pre-present-capture mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "Image pre_present = LoadImageFromScreen();\n    ${_site}"
    _mutated "${_host_text}")
if(_mutated STREQUAL _host_text)
    message(FATAL_ERROR "pre-present mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/pre-present-capture.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("pre-EndDrawing capture" HOST "${_path}"
    "rejected pre-EndDrawing capture")

file(READ "${_renderer}" _renderer_text)
set(_site "const CombatRenderPlan render_plan = make_combat_render_plan(")
string(FIND "${_renderer_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "second-render-plan mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "const CombatRenderPlan duplicate_plan = make_combat_render_plan(\n        previous, current, clamped_interpolation_alpha, camera_offset,\n        loot_filter_mode_, static_cast<float>(GetScreenWidth()),\n        static_cast<float>(GetScreenHeight()));\n\n    ${_site}"
    _mutated "${_renderer_text}")
if(_mutated STREQUAL _renderer_text)
    message(FATAL_ERROR "second-render-plan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/second-render-plan.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("second render plan" RENDERER "${_path}"
    "requires the one production renderer plan")

set(_draw_site "GroundLootView CombatRenderer::draw(")
string(FIND "${_renderer_text}" "${_draw_site}" _draw_site_index)
if(_draw_site_index EQUAL -1)
    message(FATAL_ERROR "draw-external-plan mutation site disappeared")
endif()
set(_draw_external_plan
    "void stage11d_draw_external_plan(const dungeon::DungeonSnapshot& snapshot) noexcept {\n    static_cast<void>(make_combat_render_plan(\n        snapshot, settings::LootFilterMode::show_all, 1.0F, 1.0F));\n}\n\n${_draw_site}")
string(REPLACE "${_draw_site}" "${_draw_external_plan}"
    _mutated "${_renderer_text}")
if(_mutated STREQUAL _renderer_text)
    message(FATAL_ERROR "draw-external-plan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/draw-external-plan.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("draw-external render plan" RENDERER "${_path}"
    "rejected a draw-external renderer plan")

file(READ "${_validator}" _validator_text)
set(_mutated "${_validator_text}")
string(REPLACE "LastWriteTimeUtc" "CreationTimeUtc" _mutated "${_mutated}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "stale-validator mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stale-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("stale PNG acceptance" VALIDATOR "${_path}"
    "missing semantic check: LastWriteTimeUtc")

set(_mutated "${_validator_text}")
set(_site "Require ($hashes.Add([string]$hash)) \"duplicate screenshot hash: $name\"")
string(FIND "${_validator_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "duplicate-validator mutation site disappeared")
endif()
string(REPLACE "${_site}" "[void]$hashes.Add([string]$hash)"
    _mutated "${_validator_text}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "duplicate-validator mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/duplicate-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("duplicate PNG acceptance" VALIDATOR "${_path}"
    "missing semantic check: duplicate")

set(_mutated "${_validator_text}")
set(_item_feature_site
    "$feature = Measure-Region $bitmap $rect.X $rect.Y $rect.W $rect.H")
set(_notice_feature_site
    "$feature = Measure-Region $bitmap $notice[0] $notice[1] $notice[2] $notice[3]")
string(FIND "${_validator_text}" "${_item_feature_site}" _item_site_index)
string(FIND "${_validator_text}" "${_notice_feature_site}" _notice_site_index)
if(_item_site_index EQUAL -1 OR _notice_site_index EQUAL -1)
    message(FATAL_ERROR "existence-only mutation site disappeared")
endif()
string(REPLACE "${_item_feature_site}"
    "$feature = [pscustomobject]@{ Colors = 99; Bright = 99; Dark = 99 } #"
    _mutated "${_validator_text}")
string(REPLACE "${_notice_feature_site}"
    "$feature = [pscustomobject]@{ Colors = 99; Bright = 99; Dark = 99 } #"
    _mutated "${_mutated}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "existence-only mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/existence-only-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("existence-only validation" VALIDATOR "${_path}"
    "missing semantic check: $feature")

if(DEFINED TASK5A_TARGETED_ONLY AND TASK5A_TARGETED_ONLY)
    message(STATUS
        "Stage11D Task5A targeted guard test passed: bad_mutations=5; harmless_decoys=2")
else()
    message(STATUS
        "Stage11D loot evidence guard self-test passed: bad_mutations=47; harmless_decoys=2")
endif()
