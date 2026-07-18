if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11b_settings_evidence_guard_test.cmake")
set(_fixture "${SOURCE_ROOT}/tests/platform/stage11b_settings_bad_host_input.txt")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")
set(_formal_mutation "${GUARD_TEST_ROOT}/stage11b_settings_formal_game_validation.cpp")
file(COPY_FILE
    "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp"
    "${_formal_mutation}")
file(APPEND "${_formal_mutation}" "\nTestAccess state{};\n")
set(_host_mutation "${GUARD_TEST_ROOT}/raylib_host.cpp")
file(COPY_FILE "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" "${_host_mutation}")
file(APPEND "${_host_mutation}" "\n// mutation: forbidden validation focus/snapshot bypass\nsnapshot.down.fill(false);\nsnapshot.pressed.fill(false);\nsnapshot.escape = false;\nsnapshot.enter = false;\npause_input.focus_lost = false;\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DFORMAL_OVERRIDE=${_formal_mutation}" -P "${_guard}"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(_result EQUAL 0)
    message(FATAL_ERROR "evidence guard self-test accepted the bad host input fixture")
endif()
if(NOT "${_stdout}${_stderr}" MATCHES "rejected forbidden token")
    message(FATAL_ERROR "evidence guard self-test failed for the wrong reason: ${_stdout}${_stderr}")
endif()
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
