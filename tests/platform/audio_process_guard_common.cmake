function(arpg_audio_process_guard_process_call_detected
        SOURCE_ROOT SOURCE_FILE SOURCE_TEXT OUT_DETECTED)
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

    string(TOLOWER "${_checked_text}" _checked_lower)
    set(_process_function_family
        "(system|_wsystem|popen|_popen|_wpopen|createprocess[a-z]*|shellexecute[a-z]*|winexec|posix_spawn[a-z]*|_spawn(l|le|lp|lpe|v|ve|vp|vpe)?|_wspawn(l|le|lp|lpe|v|ve|vp|vpe)?|exec(l|le|lp|lpe|v|ve|vp|vpe)?|_exec(l|le|lp|lpe|v|ve|vp|vpe)?)")
    set(_bare_process_call_pattern
        "(^|[^A-Za-z0-9_:>.])${_process_function_family}[ \\t\\r\\n]*\\(")
    set(_std_process_call_pattern
        "(^|[^A-Za-z0-9_:>.])std::system[ \\t\\r\\n]*\\(")
    set(_global_process_call_pattern
        "(^|[^A-Za-z0-9_:>.])::${_process_function_family}[ \\t\\r\\n]*\\(")
    if(_checked_lower MATCHES "${_bare_process_call_pattern}"
            OR _checked_lower MATCHES "${_std_process_call_pattern}"
            OR _checked_lower MATCHES "${_global_process_call_pattern}")
        set(${OUT_DETECTED} TRUE PARENT_SCOPE)
    else()
        set(${OUT_DETECTED} FALSE PARENT_SCOPE)
    endif()
endfunction()
