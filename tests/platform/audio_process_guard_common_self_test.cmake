if(NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "Audio process guard self-test requires GUARD_TEST_ROOT")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/audio_process_guard_common.cmake")

set(_fixture_root "${GUARD_TEST_ROOT}/src")
file(REMOVE_RECURSE "${GUARD_TEST_ROOT}")
file(MAKE_DIRECTORY "${_fixture_root}/launcher")

set(_launcher "${_fixture_root}/launcher/launcher_platform_win32.cpp")
set(_other_launcher "${_fixture_root}/launcher/other_launcher.cpp")

function(arpg_audio_process_guard_expect LABEL SOURCE CONTENT EXPECTED)
    file(WRITE "${SOURCE}" "${CONTENT}")
    arpg_audio_process_guard_process_call_detected(
        "${GUARD_TEST_ROOT}" "${SOURCE}" "${CONTENT}" _detected)
    if(NOT _detected STREQUAL EXPECTED)
        message(FATAL_ERROR
            "Audio process guard ${LABEL}: expected ${EXPECTED}, got ${_detected}")
    endif()
endfunction()

arpg_audio_process_guard_expect(
    "allows exact launcher CreateProcessW"
    "${_launcher}"
    "void launch() { CreateProcessW(); }\n"
    FALSE)
arpg_audio_process_guard_expect(
    "allows exact launcher ShellExecuteW"
    "${_launcher}"
    "void open_save() { ShellExecuteW(); }\n"
    FALSE)
arpg_audio_process_guard_expect(
    "retains launcher system detection"
    "${_launcher}"
    "void launch() { system(); }\n"
    TRUE)
arpg_audio_process_guard_expect(
    "retains launcher CreateProcessA detection"
    "${_launcher}"
    "void launch() { CreateProcessA(); }\n"
    TRUE)
arpg_audio_process_guard_expect(
    "retains CreateProcessW detection in other launcher file"
    "${_other_launcher}"
    "void launch() { CreateProcessW(); }\n"
    TRUE)
arpg_audio_process_guard_expect(
    "retains ShellExecuteW detection in other launcher file"
    "${_other_launcher}"
    "void open_save() { ShellExecuteW(); }\n"
    TRUE)

message(STATUS
    "Audio process guard allows only the two canonical launcher calls")
