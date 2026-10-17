function(arpg_audio_process_guard_mask_launcher_calls
        SOURCE_ROOT SOURCE_FILE SOURCE_TEXT OUT_SCAN_TEXT)
    file(REAL_PATH "${SOURCE_ROOT}" _repository_root)
    file(REAL_PATH "${SOURCE_FILE}" _source_file)
    file(REAL_PATH
        "${_repository_root}/src/launcher/launcher_platform_win32.cpp"
        _launcher_platform_source)

    set(_checked_text "${SOURCE_TEXT}")
    if(_source_file STREQUAL _launcher_platform_source)
        string(REGEX REPLACE
            "(^|[^A-Za-z0-9_:>.])CreateProcessW[(]"
            "\\1arpg_audio_guard_allowed_createprocessw("
            _checked_text "${_checked_text}")
        string(REGEX REPLACE
            "(^|[^A-Za-z0-9_:>.])ShellExecuteW[(]"
            "\\1arpg_audio_guard_allowed_shellexecutew("
            _checked_text "${_checked_text}")
    endif()

    set(${OUT_SCAN_TEXT} "${_checked_text}" PARENT_SCOPE)
endfunction()
