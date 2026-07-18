if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11b_settings_evidence_guard_test.cmake")
set(_fixture "${SOURCE_ROOT}/tests/platform/stage11b_settings_bad_host_input.txt")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DFORMAL_OVERRIDE=${_fixture}" -P "${_guard}"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(_result EQUAL 0)
    message(FATAL_ERROR "evidence guard self-test accepted the bad host input fixture")
endif()
if(NOT "${_stdout}${_stderr}" MATCHES "rejected forbidden token")
    message(FATAL_ERROR "evidence guard self-test failed for the wrong reason: ${_stdout}${_stderr}")
endif()
