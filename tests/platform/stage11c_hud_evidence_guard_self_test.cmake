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
    "EndDrawing();\n    if (path == nullptr) return;\n    Image image = LoadImageFromScreen();"
    "Image image = LoadImageFromScreen();\n    EndDrawing();\n    if (path == nullptr) return;"
    _pre_present_source "${_host_source}")
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
