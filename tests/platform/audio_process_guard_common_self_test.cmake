if(NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "Audio process guard self-test requires GUARD_TEST_ROOT")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/audio_process_guard_common.cmake")

set(_fixture_root "${GUARD_TEST_ROOT}/src")
file(REMOVE_RECURSE "${GUARD_TEST_ROOT}")
file(MAKE_DIRECTORY "${_fixture_root}/launcher")

set(_launcher "${_fixture_root}/launcher/launcher_platform_win32.cpp")
set(_other_launcher "${_fixture_root}/launcher/other_launcher.cpp")

set(_stage14_process_function_family
    "(system|_wsystem|popen|_popen|_wpopen|createprocess[a-z]*|shellexecute[a-z]*|winexec|posix_spawn[a-z]*|_spawn(l|le|lp|lpe|v|ve|vp|vpe)?|_wspawn(l|le|lp|lpe|v|ve|vp|vpe)?|exec(l|le|lp|lpe|v|ve|vp|vpe)?|_exec(l|le|lp|lpe|v|ve|vp|vpe)?)")
set(_stage14_bare_process_call_pattern
    "(^|[^A-Za-z0-9_:>.])${_stage14_process_function_family}[ \\t\\r\\n]*\\(")
set(_stage14_std_process_call_pattern
    "(^|[^A-Za-z0-9_:>.])std::system[ \\t\\r\\n]*\\(")
set(_stage14_global_process_call_pattern
    "(^|[^A-Za-z0-9_:>.])::${_stage14_process_function_family}[ \\t\\r\\n]*\\(")
set(_stage15_process_pattern
    "(^|[^A-Za-z0-9_:>.])(system|_wsystem|popen|_popen|createprocess[a-z]*|shellexecute[a-z]*|winexec|posix_spawn[a-z]*|_spawn[a-z]*|_wspawn[a-z]*|exec[a-z]*|_exec[a-z]*)[ \\t\\r\\n]*\\(")

function(arpg_audio_process_guard_expect
        LABEL SOURCE CONTENT EXPECTED_STAGE14 EXPECTED_STAGE15)
    file(WRITE "${SOURCE}" "${CONTENT}")
    arpg_audio_process_guard_mask_launcher_calls(
        "${GUARD_TEST_ROOT}" "${SOURCE}" "${CONTENT}" _scan_text)
    string(TOLOWER "${_scan_text}" _scan_lower)
    set(_stage14_detected FALSE)
    if(_scan_lower MATCHES "${_stage14_bare_process_call_pattern}"
            OR _scan_lower MATCHES "${_stage14_std_process_call_pattern}"
            OR _scan_lower MATCHES "${_stage14_global_process_call_pattern}")
        set(_stage14_detected TRUE)
    endif()
    set(_stage15_detected FALSE)
    if(_scan_lower MATCHES "${_stage15_process_pattern}"
            OR _scan_lower MATCHES "std::system[ \\t\\r\\n]*\\(")
        set(_stage15_detected TRUE)
    endif()
    if(NOT _stage14_detected STREQUAL EXPECTED_STAGE14
            OR NOT _stage15_detected STREQUAL EXPECTED_STAGE15)
        message(FATAL_ERROR
            "Audio process guard ${LABEL}: expected Stage14=${EXPECTED_STAGE14} "
            "Stage15=${EXPECTED_STAGE15}, got Stage14=${_stage14_detected} "
            "Stage15=${_stage15_detected}")
    endif()
endfunction()

arpg_audio_process_guard_expect(
    "allows exact launcher CreateProcessW"
    "${_launcher}"
    "void launch() { CreateProcessW(); }\n"
    FALSE FALSE)
arpg_audio_process_guard_expect(
    "allows exact launcher ShellExecuteW"
    "${_launcher}"
    "void open_save() { ShellExecuteW(); }\n"
    FALSE FALSE)
arpg_audio_process_guard_expect(
    "retains launcher system detection"
    "${_launcher}"
    "void launch() { system(); }\n"
    TRUE TRUE)
arpg_audio_process_guard_expect(
    "retains launcher CreateProcessA detection"
    "${_launcher}"
    "void launch() { CreateProcessA(); }\n"
    TRUE TRUE)
arpg_audio_process_guard_expect(
    "retains launcher popen detection"
    "${_launcher}"
    "void launch() { popen(); }\n"
    TRUE TRUE)
arpg_audio_process_guard_expect(
    "retains launcher WinExec detection"
    "${_launcher}"
    "void launch() { WinExec(); }\n"
    TRUE TRUE)
arpg_audio_process_guard_expect(
    "preserves Stage15 arbitrary spawn suffix detection"
    "${_launcher}"
    "void launch() { _spawncustom(); }\n"
    FALSE TRUE)
arpg_audio_process_guard_expect(
    "retains CreateProcessW detection in other launcher file"
    "${_other_launcher}"
    "void launch() { CreateProcessW(); }\n"
    TRUE TRUE)
arpg_audio_process_guard_expect(
    "retains ShellExecuteW detection in other launcher file"
    "${_other_launcher}"
    "void open_save() { ShellExecuteW(); }\n"
    TRUE TRUE)

message(STATUS
    "Audio process guard allows only the two canonical launcher calls")
