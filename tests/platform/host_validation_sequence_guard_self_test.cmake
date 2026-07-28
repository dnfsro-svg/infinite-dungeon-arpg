if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()

set(_guard "${SOURCE_ROOT}/tests/platform/host_validation_sequence_guard_test.cmake")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

set(_split_fixed_step "${GUARD_TEST_ROOT}/split-fixed-step-conditions.cpp")
file(WRITE "${_split_fixed_step}" [=[
while (!exit_requested) {
    sample_physical_keys();
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
