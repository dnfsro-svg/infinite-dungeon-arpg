if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()

set(_guard "${SOURCE_ROOT}/tests/platform/host_validation_sequence_guard_test.cmake")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

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
string(REPLACE
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys_removed();\n            // decoy continues \\\nsample_physical_keys()"
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
