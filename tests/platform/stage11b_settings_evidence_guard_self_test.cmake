if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11b_settings_evidence_guard_test.cmake")
set(_fixture "${SOURCE_ROOT}/tests/platform/stage11b_settings_bad_host_input.txt")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")
file(READ "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp"
    _formal_source)
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
set(_paused_capture_mutation "${GUARD_TEST_ROOT}/host-paused-capture.cpp")
file(COPY_FILE "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" "${_paused_capture_mutation}")
file(READ "${_paused_capture_mutation}" _paused_capture_source)
string(REPLACE "&& pause_menu.screen != PauseScreen::closed"
    "&& pause_menu.screen == PauseScreen::closed"
    _paused_capture_source "${_paused_capture_source}")
file(WRITE "${_paused_capture_mutation}" "${_paused_capture_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${_paused_capture_mutation}" -P "${_guard}"
    RESULT_VARIABLE _paused_capture_result OUTPUT_VARIABLE _paused_capture_stdout ERROR_VARIABLE _paused_capture_stderr)
if(_paused_capture_result EQUAL 0)
    message(FATAL_ERROR "evidence guard self-test accepted resume-after pause capture mutation")
endif()
if(NOT "${_paused_capture_stdout}${_paused_capture_stderr}" MATCHES
        "paused screenshot")
    message(FATAL_ERROR "paused capture mutation failed for wrong reason: ${_paused_capture_stdout}${_paused_capture_stderr}")
endif()
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

foreach(_mutation IN ITEMS
        "Image image = LoadImageFromScreen();|requires exactly one post-Present screen capture"
        "runtime.fixed_tick({});|requires exactly one pause-gated runtime.fixed_tick path")
    string(REPLACE "|" ";" _parts "${_mutation}")
    list(GET _parts 0 _source_mutation)
    list(GET _parts 1 _expected_failure)
    set(_mutated_host "${GUARD_TEST_ROOT}/host-count-${_expected_failure}.cpp")
    file(COPY_FILE "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" "${_mutated_host}")
    file(APPEND "${_mutated_host}" "\n// copied count mutation\n${_source_mutation}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_mutated_host}" -P "${_guard}"
        RESULT_VARIABLE _mutation_result OUTPUT_VARIABLE _mutation_stdout ERROR_VARIABLE _mutation_stderr)
    if(_mutation_result EQUAL 0)
        message(FATAL_ERROR "evidence guard self-test accepted count mutation: ${_source_mutation}")
    endif()
    if(NOT "${_mutation_stdout}${_mutation_stderr}" MATCHES "${_expected_failure}")
        message(FATAL_ERROR "count mutation failed for wrong reason: ${_source_mutation}: ${_mutation_stdout}${_mutation_stderr}")
    endif()
endforeach()
