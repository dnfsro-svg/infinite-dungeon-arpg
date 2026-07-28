if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

set(_guard "${SOURCE_ROOT}/tests/platform/host_validation_sequence_guard_test.cmake")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

set(_sequence_cmake_mutation "${GUARD_TEST_ROOT}/sequence-cmake-comment-decoy.cmake")
file(READ "${SOURCE_ROOT}/src/platform/raylib/CMakeLists.txt" _sequence_cmake_text)
string(REPLACE "    host_validation_stage11c.cpp"
    "    # decoy;host_validation_stage11c.cpp\nmessage(STATUS \"host_validation_stage11c.cpp\")"
    _sequence_cmake_mutated "${_sequence_cmake_text}")
if(_sequence_cmake_mutated STREQUAL _sequence_cmake_text)
    message(FATAL_ERROR "sequence CMake comment mutation anchor is missing")
endif()
file(WRITE "${_sequence_cmake_mutation}" "${_sequence_cmake_mutated}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DCMAKE_OVERRIDE=${_sequence_cmake_mutation}" -P "${_guard}"
    RESULT_VARIABLE _sequence_cmake_result
    OUTPUT_VARIABLE _sequence_cmake_stdout ERROR_VARIABLE _sequence_cmake_stderr)
if(_sequence_cmake_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted CMake comment/quoted decoy")
endif()
if(NOT "${_sequence_cmake_stdout}${_sequence_cmake_stderr}" MATCHES
        "arpg_raylib does not register host_validation_stage11c.cpp")
    message(FATAL_ERROR "sequence CMake comment mutation failed for wrong reason: ${_sequence_cmake_stdout}${_sequence_cmake_stderr}")
endif()

function(arpg_expect_sequence_cmake_rejection NAME REPLACEMENT)
    set(_mutation "${GUARD_TEST_ROOT}/sequence-${NAME}.cmake")
    string(REPLACE "    host_validation_stage11c.cpp" "${REPLACEMENT}"
        _mutated "${_sequence_cmake_text}")
    if(_mutated STREQUAL _sequence_cmake_text)
        message(FATAL_ERROR "sequence ${NAME} mutation anchor is missing")
    endif()
    file(WRITE "${_mutation}" "${_mutated}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DCMAKE_OVERRIDE=${_mutation}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Host validation sequence guard accepted ${NAME}")
    endif()
    if(NOT "${_stdout}${_stderr}" MATCHES
            "arpg_raylib does not register host_validation_stage11c.cpp")
        message(FATAL_ERROR "sequence ${NAME} failed for wrong reason: ${_stdout}${_stderr}")
    endif()
endfunction()

arpg_expect_sequence_cmake_rejection(sequence_cmake_bracket_comment_decoy [==[
    #[=[
    host_validation_stage11c.cpp
    ]]
    ]=]
]==])
arpg_expect_sequence_cmake_rejection(sequence_cmake_bracket_argument_decoy [===[
    [==[
    host_validation_stage11c.cpp
    # ; (target_sources(arpg_raylib fake_target))
    ]==]
]===])
if(DEFINED SEQUENCE_CMAKE_ONLY)
    message(STATUS "Host validation sequence guard rejected all CMake decoys")
    return()
endif()

function(arpg_assert_lexical_token_equivalence LABEL SOURCE TOKEN EXPECTED_CODE)
    evidence_sanitize_cpp_for_scan("${SOURCE}" _sanitized_fixture)
    string(FIND "${_sanitized_fixture}" "${TOKEN}" _sanitized_position)
    evidence_find_cpp_code_token("${SOURCE}" "${TOKEN}" _fast_position)
    if(EXPECTED_CODE)
        if(_sanitized_position EQUAL -1 OR _fast_position EQUAL -1
                OR NOT _sanitized_position EQUAL _fast_position)
            message(FATAL_ERROR "lexical scanner mismatch for ${LABEL}: ${_sanitized_position}/${_fast_position}")
        endif()
    elseif(NOT _sanitized_position EQUAL -1 OR NOT _fast_position EQUAL -1)
        message(FATAL_ERROR "lexical scanner exposed hidden token for ${LABEL}: ${_sanitized_position}/${_fast_position}")
    endif()
endfunction()

function(arpg_expect_stage11c_sequence_rejection
        NAME TARGET_FILE SEARCH REPLACEMENT REASON)
    set(_mutation_file "${GUARD_TEST_ROOT}/${NAME}-${TARGET_FILE}")
    set(_production_file "${SOURCE_ROOT}/src/platform/raylib/${TARGET_FILE}")
    file(READ "${_production_file}" _mutated_text)
    set(_original_text "${_mutated_text}")
    string(REPLACE "${SEARCH}" "${REPLACEMENT}" _mutated_text "${_mutated_text}")
    if(_mutated_text STREQUAL _original_text)
        message(FATAL_ERROR
            "Stage11C sequence mutation ${NAME} anchor is missing: ${SEARCH}")
    endif()
    file(WRITE "${_mutation_file}" "${_mutated_text}")

    set(_override "-DSTAGE11C_SOURCE_OVERRIDE=${_mutation_file}")
    if(TARGET_FILE STREQUAL "host_validation_stage11c.hpp")
        set(_override "-DSTAGE11C_HEADER_OVERRIDE=${_mutation_file}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}" "${_override}"
            -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR
            "Host validation sequence guard accepted Stage11C ${NAME} decoy")
    endif()
    set(_combined "${_stdout}\n${_stderr}")
    string(FIND "${_combined}" "${REASON}" _reason_index)
    if(_reason_index EQUAL -1)
        message(FATAL_ERROR
            "Stage11C sequence mutation ${NAME} failed for wrong reason; "
            "expected '${REASON}', got: ${_combined}")
    endif()
endfunction()

set(_lexical_lf_fixture [=[
// lexical_line_hidden \
lexical_line_hidden
lexical_line_visible;
const char* escaped = "lexical_string_hidden \" still hidden";
const char escaped_quote = '\'';
/* lexical_block_hidden *\
/ lexical_block_visible;
]=])
string(ASCII 10 _lexical_lf)
string(ASCII 13 _lexical_cr)
string(REPLACE "${_lexical_lf}" "${_lexical_cr}${_lexical_lf}"
    _lexical_crlf_fixture "${_lexical_lf_fixture}")
foreach(_fixture_name IN ITEMS lf crlf)
    set(_fixture_source "${_lexical_${_fixture_name}_fixture}")
    arpg_assert_lexical_token_equivalence("${_fixture_name} line splice"
        "${_fixture_source}" "lexical_line_hidden" FALSE)
    arpg_assert_lexical_token_equivalence("${_fixture_name} visible code"
        "${_fixture_source}" "lexical_line_visible" TRUE)
    arpg_assert_lexical_token_equivalence("${_fixture_name} escaped string"
        "${_fixture_source}" "lexical_string_hidden" FALSE)
    arpg_assert_lexical_token_equivalence("${_fixture_name} block close splice"
        "${_fixture_source}" "lexical_block_hidden" FALSE)
    arpg_assert_lexical_token_equivalence("${_fixture_name} block close visible"
        "${_fixture_source}" "lexical_block_visible" TRUE)
endforeach()

arpg_expect_stage11c_sequence_rejection(stage_header_comment_decoy
    host_validation_stage11c.hpp
    "struct Stage11CHudValidationState final {"
    "struct Stage11CHudValidationState;\n// struct Stage11CHudValidationState final {"
    "Host validation Stage11C state definition is missing")
arpg_expect_stage11c_sequence_rejection(stage_source_comment_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "// std::uint64_t stage11c_production_snapshot_hash(\nstd::uint64_t stage11c_missing_snapshot_hash("
    "Evidence validation function is missing: std::uint64_t")
arpg_expect_stage11c_sequence_rejection(stage_source_string_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "constexpr const char* stage11c_hash_decoy = \"std::uint64_t stage11c_production_snapshot_hash(\";\nstd::uint64_t stage11c_missing_snapshot_hash("
    "Evidence validation function is missing: std::uint64_t")
arpg_expect_stage11c_sequence_rejection(stage_source_forward_decoy
    host_validation_stage11c.cpp
    "bool stage11c_hud_validation_reached(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
    "bool stage11c_hud_validation_reached(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept;\nbool stage11c_hud_validation_reached_moved(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
    "Host validation Stage11C definition is missing")
arpg_expect_stage11c_sequence_rejection(stage_source_cross_function_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "void stage11c_cross_function_decoy() {\n    const std::uint64_t stage11c_production_snapshot_hash(0);\n}\nstd::uint64_t stage11c_missing_snapshot_hash("
    "Host validation Stage11C definition is missing")
arpg_expect_stage11c_sequence_rejection(stage_source_lambda_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "const auto stage11c_lambda_decoy = [] {\n    const std::uint64_t stage11c_production_snapshot_hash(0);\n};\nstd::uint64_t stage11c_missing_snapshot_hash("
    "Host validation Stage11C definition is missing")
string(ASCII 10 _stage11c_lf)
string(ASCII 13 _stage11c_cr)
string(ASCII 92 _stage11c_backslash)
set(_stage11c_spliced_lf_decoy
    "// std::uint64_t stage11c_production_snapshot_hash( ${_stage11c_backslash}${_stage11c_lf}std::uint64_t stage11c_missing_snapshot_hash(")
set(_stage11c_spliced_crlf_decoy
    "// std::uint64_t stage11c_production_snapshot_hash( ${_stage11c_backslash}${_stage11c_cr}${_stage11c_lf}std::uint64_t stage11c_missing_snapshot_hash(")
arpg_expect_stage11c_sequence_rejection(stage_source_spliced_lf_comment_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "${_stage11c_spliced_lf_decoy}"
    "Evidence validation function is missing: std::uint64_t")
arpg_expect_stage11c_sequence_rejection(stage_source_spliced_crlf_comment_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "${_stage11c_spliced_crlf_decoy}"
    "Evidence validation function is missing: std::uint64_t")

set(_split_fixed_step "${GUARD_TEST_ROOT}/split-fixed-step-conditions.cpp")
file(WRITE "${_split_fixed_step}" [=[
HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
while (!exit_requested) {
    const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
    inject_stage11b_physical_edges();
    inject_stage11c_physical_edges();
    inject_stage11d_physical_edges();
    inject_stage17_physical_edges();
    map_host_frame_input();
    if (!step_death) {
        if (config.stage11_validation != Stage11ValidationScenario::none) {
            stage11_validation_input();
        }
        if (config.stage10_validation != Stage10ValidationScenario::none) {
            stage10_validation_input();
        }
        step_movement = movement;
    }
    runtime.fixed_tick(step_movement,
        loot_pickup_policy(live_settings.loot_filter_mode));
    write_stage11b_validation_summary();
    write_stage11c_hud_validation_summary();
    write_stage11d_loot_validation_summary();
    write_stage17_validation_summary();
}
audio.shutdown();
}
]=])

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_split_fixed_step}" -P "${_guard}"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted split fixed-step conditions")
endif()
if(NOT "${_stdout}${_stderr}" MATCHES
        "rejected fixed-step movement priority[ \t\r\n]+structure")
    message(FATAL_ERROR "split fixed-step conditions failed for wrong reason: ${_stdout}${_stderr}")
endif()

file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" _host_source)

# The raw pre-crop used by the guard must not treat a signature inside a
# comment that began before the candidate as executable code.
string(REPLACE
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys_removed();"
    _precrop_comment_host "${_host_source}")
string(FIND "${_precrop_comment_host}" "HostExitCode run_raylib_host("
    _actual_runtime_begin)
set(_precrop_comment_decoy [=[/*
HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    try {
        while (!exit_requested) {
            const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
            inject_stage11b_physical_edges();
            inject_stage11c_physical_edges();
            inject_stage11d_physical_edges();
            inject_stage17_physical_edges();
            map_host_frame_input();
            if (!step_death) {
                if (config.stage11_validation != Stage11ValidationScenario::none) {
                    step_movement = host_validation::stage11_validation_input(config, stage11_validation_state);
                } else if (config.stage10_validation != Stage10ValidationScenario::none) {
                    step_movement = host_validation::stage10_validation_input(config, stage10_validation_state);
                } else {
                    step_movement = movement;
                }
            }
            runtime.fixed_tick(step_movement,
                loot_pickup_policy(live_settings.loot_filter_mode));
        }
        host_validation::write_stage11b_validation_summary(config, stage11b_validation_state,
            pause_menu);
        write_stage11c_hud_validation_summary(config, stage11c_validation_state);
        write_stage11d_loot_validation_summary(config, stage11d_validation_state, pause_menu);
        write_stage17_validation_summary(config, *stage17_validation_state);
        audio.shutdown();
    } catch (...) {
    }
}
*/
]=])
string(SUBSTRING "${_precrop_comment_host}" 0 ${_actual_runtime_begin}
    _precrop_prefix)
string(SUBSTRING "${_precrop_comment_host}" ${_actual_runtime_begin} -1
    _precrop_suffix)
set(_precrop_comment_host
    "${_precrop_prefix}${_precrop_comment_decoy}${_precrop_suffix}")
set(_precrop_comment "${GUARD_TEST_ROOT}/precrop-comment-decoy.cpp")
file(WRITE "${_precrop_comment}" "${_precrop_comment_host}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_precrop_comment}" -P "${_guard}"
    RESULT_VARIABLE _precrop_comment_result
    OUTPUT_VARIABLE _precrop_comment_stdout ERROR_VARIABLE _precrop_comment_stderr)
if(_precrop_comment_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted pre-crop comment decoy")
endif()
if(NOT "${_precrop_comment_stdout}${_precrop_comment_stderr}" MATCHES
        "missing input injection chain token")
    message(FATAL_ERROR "pre-crop comment decoy failed for wrong reason: ${_precrop_comment_stdout}${_precrop_comment_stderr}")
endif()

string(SUBSTRING "${_precrop_comment_decoy}" 2 -1 _precrop_string_payload)
string(ASCII 10 _string_newline)
string(ASCII 92 _string_backslash)
string(REPLACE "${_string_newline}" "${_string_backslash}${_string_newline}"
    _precrop_string_payload "${_precrop_string_payload}")
set(_precrop_string_decoy
    "const char* ignored_runtime_signature = \"${_precrop_string_payload}\";")
string(REPLACE "${_string_backslash}${_string_newline}" ""
    _precrop_string_without_splices "${_precrop_string_decoy}")
string(FIND "${_precrop_string_without_splices}" "${_string_newline}"
    _precrop_unspliced_newline)
if(NOT _precrop_unspliced_newline EQUAL -1)
    message(FATAL_ERROR "pre-crop string fixture has a payload newline without a splice")
endif()
set(_precrop_string_host
    "${_precrop_prefix}${_precrop_string_decoy}${_string_newline}${_precrop_suffix}")
set(_precrop_string "${GUARD_TEST_ROOT}/precrop-string-decoy.cpp")
file(WRITE "${_precrop_string}" "${_precrop_string_host}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_precrop_string}" -P "${_guard}"
    RESULT_VARIABLE _precrop_string_result
    OUTPUT_VARIABLE _precrop_string_stdout ERROR_VARIABLE _precrop_string_stderr)
if(_precrop_string_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted pre-crop string decoy")
endif()
if(NOT "${_precrop_string_stdout}${_precrop_string_stderr}" MATCHES
        "missing input injection chain token")
    message(FATAL_ERROR "pre-crop string decoy failed for wrong reason: ${_precrop_string_stdout}${_precrop_string_stderr}")
endif()

string(REPLACE
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys_removed();\n            // decoy continues \\\nconst PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    _spliced_input_host "${_host_source}")
set(_spliced_input "${GUARD_TEST_ROOT}/spliced-input-decoy.cpp")
file(WRITE "${_spliced_input}" "${_spliced_input_host}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_spliced_input}" -P "${_guard}"
    RESULT_VARIABLE _spliced_result OUTPUT_VARIABLE _spliced_stdout ERROR_VARIABLE _spliced_stderr)
if(_spliced_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted spliced input decoy")
endif()
if(NOT "${_spliced_stdout}${_spliced_stderr}" MATCHES
        "missing input injection chain token")
    message(FATAL_ERROR "spliced input decoy failed for wrong reason: ${_spliced_stdout}${_spliced_stderr}")
endif()

string(REPLACE
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "const auto loop_input_decoy = [] {\n                const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();\n            };\n            const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys_removed();"
    _lambda_input_host "${_host_source}")
set(_lambda_input "${GUARD_TEST_ROOT}/lambda-input-decoy.cpp")
file(WRITE "${_lambda_input}" "${_lambda_input_host}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_lambda_input}" -P "${_guard}"
    RESULT_VARIABLE _lambda_result OUTPUT_VARIABLE _lambda_stdout ERROR_VARIABLE _lambda_stderr)
if(_lambda_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted lambda input decoy")
endif()
if(NOT "${_lambda_stdout}${_lambda_stderr}" MATCHES
        "rejected input injection chain token outside[ \t\r\n]+direct host scope")
    message(FATAL_ERROR "lambda input decoy failed for wrong reason: ${_lambda_stdout}${_lambda_stderr}")
endif()

string(REPLACE
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys_removed();"
    _outside_input_host "${_host_source}")
string(REPLACE "audio.shutdown();"
    "const auto outside_input_decoy = [] {\n            const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();\n        };\n        audio.shutdown();"
    _outside_input_host "${_outside_input_host}")
set(_outside_input "${GUARD_TEST_ROOT}/outside-input-decoy.cpp")
file(WRITE "${_outside_input}" "${_outside_input_host}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_outside_input}" -P "${_guard}"
    RESULT_VARIABLE _outside_result OUTPUT_VARIABLE _outside_stdout ERROR_VARIABLE _outside_stderr)
if(_outside_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted out-of-loop input decoy")
endif()
if(NOT "${_outside_stdout}${_outside_stderr}" MATCHES
        "missing input injection chain token")
    message(FATAL_ERROR "out-of-loop input decoy failed for wrong reason: ${_outside_stdout}${_outside_stderr}")
endif()

string(REPLACE
    "host_validation::write_stage11b_validation_summary(config, stage11b_validation_state,\n            pause_menu);"
    "const auto summary_decoy = [&] {\n            host_validation::write_stage11b_validation_summary(config, stage11b_validation_state,\n                pause_menu);\n        };"
    _lambda_summary_host "${_host_source}")
set(_lambda_summary "${GUARD_TEST_ROOT}/lambda-summary-decoy.cpp")
file(WRITE "${_lambda_summary}" "${_lambda_summary_host}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_lambda_summary}" -P "${_guard}"
    RESULT_VARIABLE _summary_result OUTPUT_VARIABLE _summary_stdout ERROR_VARIABLE _summary_stderr)
if(_summary_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted lambda summary decoy")
endif()
if(NOT "${_summary_stdout}${_summary_stderr}" MATCHES
        "rejected validation summary write token[ \t\r\n]+outside[ \t\r\n]+direct host scope")
    message(FATAL_ERROR "lambda summary decoy failed for wrong reason: ${_summary_stdout}${_summary_stderr}")
endif()

set(_stage_header_mutation "${GUARD_TEST_ROOT}/stage11b-header-persistence.hpp")
file(COPY_FILE "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.hpp"
    "${_stage_header_mutation}")
file(APPEND "${_stage_header_mutation}" "\n#include \"persistence/save_store.hpp\"\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DSTAGE11B_HEADER_OVERRIDE=${_stage_header_mutation}" -P "${_guard}"
    RESULT_VARIABLE _header_result OUTPUT_VARIABLE _header_stdout ERROR_VARIABLE _header_stderr)
if(_header_result EQUAL 0)
    message(FATAL_ERROR "Host validation sequence guard accepted Stage11B header persistence dependency")
endif()
if(NOT "${_header_stdout}${_header_stderr}" MATCHES
        "Host validation boundary header has a forbidden dependency")
    message(FATAL_ERROR "Stage11B header mutation failed for wrong reason: ${_header_stdout}${_header_stderr}")
endif()
