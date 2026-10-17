macro(arpg_cpp_finish_sanitized_line)
    string(APPEND _arpg_sanitized "${_arpg_line}\n")
    set(_arpg_line "")
endmacro()

function(arpg_sanitize_cpp_source SOURCE_TEXT OUT_SOURCE)
    set(_arpg_state CODE)
    set(_arpg_line "")
    set(_arpg_sanitized "")
    set(_arpg_raw_closer "")
    string(LENGTH "${SOURCE_TEXT}" _arpg_source_length)
    set(_arpg_index 0)

    while(_arpg_index LESS _arpg_source_length)
        string(SUBSTRING "${SOURCE_TEXT}" ${_arpg_index} 1 _arpg_char)
        math(EXPR _arpg_next_index "${_arpg_index} + 1")
        set(_arpg_next_char "")
        if(_arpg_next_index LESS _arpg_source_length)
            string(SUBSTRING
                "${SOURCE_TEXT}" ${_arpg_next_index} 1 _arpg_next_char)
        endif()

        math(EXPR _arpg_after_next_index "${_arpg_index} + 2")
        set(_arpg_after_next_char "")
        if(_arpg_after_next_index LESS _arpg_source_length)
            string(SUBSTRING
                "${SOURCE_TEXT}"
                ${_arpg_after_next_index}
                1
                _arpg_after_next_char)
        endif()

        set(_arpg_logical_next_index ${_arpg_next_index})
        set(_arpg_logical_next_char "${_arpg_next_char}")
        if(NOT _arpg_state STREQUAL RAW_STRING)
            set(_arpg_logical_next_char "")
            while(_arpg_logical_next_index LESS _arpg_source_length)
                string(SUBSTRING
                    "${SOURCE_TEXT}"
                    ${_arpg_logical_next_index}
                    1
                    _arpg_logical_candidate)
                if(_arpg_logical_candidate STREQUAL "\\")
                    math(EXPR
                        _arpg_splice_next_index
                        "${_arpg_logical_next_index} + 1")
                    set(_arpg_splice_next_char "")
                    if(_arpg_splice_next_index LESS _arpg_source_length)
                        string(SUBSTRING
                            "${SOURCE_TEXT}"
                            ${_arpg_splice_next_index}
                            1
                            _arpg_splice_next_char)
                    endif()
                    if(_arpg_splice_next_char STREQUAL "\n")
                        math(EXPR
                            _arpg_logical_next_index
                            "${_arpg_logical_next_index} + 2")
                        continue()
                    endif()

                    math(EXPR
                        _arpg_splice_after_index
                        "${_arpg_logical_next_index} + 2")
                    set(_arpg_splice_after_char "")
                    if(_arpg_splice_after_index LESS _arpg_source_length)
                        string(SUBSTRING
                            "${SOURCE_TEXT}"
                            ${_arpg_splice_after_index}
                            1
                            _arpg_splice_after_char)
                    endif()
                    if(_arpg_splice_next_char STREQUAL "\r"
                            AND _arpg_splice_after_char STREQUAL "\n")
                        math(EXPR
                            _arpg_logical_next_index
                            "${_arpg_logical_next_index} + 3")
                        continue()
                    endif()
                endif()

                set(_arpg_logical_next_char "${_arpg_logical_candidate}")
                break()
            endwhile()
        endif()

        if(NOT _arpg_state STREQUAL RAW_STRING
                AND _arpg_char STREQUAL "\\")
            if(_arpg_next_char STREQUAL "\n")
                math(EXPR _arpg_index "${_arpg_index} + 2")
                continue()
            endif()
            if(_arpg_next_char STREQUAL "\r"
                    AND _arpg_after_next_char STREQUAL "\n")
                math(EXPR _arpg_index "${_arpg_index} + 3")
                continue()
            endif()
        endif()

        if(_arpg_state STREQUAL CODE)
            if(_arpg_char STREQUAL "/"
                    AND _arpg_logical_next_char STREQUAL "/")
                string(APPEND _arpg_line " ")
                set(_arpg_state LINE_COMMENT)
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()
            if(_arpg_char STREQUAL "/"
                    AND _arpg_logical_next_char STREQUAL "*")
                string(APPEND _arpg_line " ")
                set(_arpg_state BLOCK_COMMENT)
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()

            if(_arpg_char STREQUAL "R"
                    AND _arpg_logical_next_char STREQUAL "\"")
                math(EXPR
                    _arpg_delimiter_index
                    "${_arpg_logical_next_index} + 1")
                set(_arpg_raw_delimiter "")
                set(_arpg_valid_raw_opener FALSE)
                while(_arpg_delimiter_index LESS _arpg_source_length)
                    string(SUBSTRING
                        "${SOURCE_TEXT}"
                        ${_arpg_delimiter_index}
                        1
                        _arpg_delimiter_char)
                    if(_arpg_delimiter_char STREQUAL "\\")
                        math(EXPR
                            _arpg_delimiter_next_index
                            "${_arpg_delimiter_index} + 1")
                        set(_arpg_delimiter_next_char "")
                        if(_arpg_delimiter_next_index LESS _arpg_source_length)
                            string(SUBSTRING
                                "${SOURCE_TEXT}"
                                ${_arpg_delimiter_next_index}
                                1
                                _arpg_delimiter_next_char)
                        endif()
                        if(_arpg_delimiter_next_char STREQUAL "\n")
                            math(EXPR
                                _arpg_delimiter_index
                                "${_arpg_delimiter_index} + 2")
                            continue()
                        endif()

                        math(EXPR
                            _arpg_delimiter_after_index
                            "${_arpg_delimiter_index} + 2")
                        set(_arpg_delimiter_after_char "")
                        if(_arpg_delimiter_after_index LESS _arpg_source_length)
                            string(SUBSTRING
                                "${SOURCE_TEXT}"
                                ${_arpg_delimiter_after_index}
                                1
                                _arpg_delimiter_after_char)
                        endif()
                        if(_arpg_delimiter_next_char STREQUAL "\r"
                                AND _arpg_delimiter_after_char STREQUAL "\n")
                            math(EXPR
                                _arpg_delimiter_index
                                "${_arpg_delimiter_index} + 3")
                            continue()
                        endif()
                    endif()
                    if(_arpg_delimiter_char STREQUAL "(")
                        string(LENGTH
                            "${_arpg_raw_delimiter}" _arpg_delimiter_length)
                        if(_arpg_delimiter_length LESS_EQUAL 16)
                            set(_arpg_valid_raw_opener TRUE)
                        endif()
                        break()
                    endif()
                    if(_arpg_delimiter_char STREQUAL "\n"
                            OR _arpg_delimiter_char STREQUAL "\r")
                        break()
                    endif()
                    string(APPEND
                        _arpg_raw_delimiter "${_arpg_delimiter_char}")
                    string(LENGTH
                        "${_arpg_raw_delimiter}" _arpg_delimiter_length)
                    if(_arpg_delimiter_length GREATER 16)
                        break()
                    endif()
                    math(EXPR
                        _arpg_delimiter_index
                        "${_arpg_delimiter_index} + 1")
                endwhile()

                if(_arpg_valid_raw_opener)
                    string(APPEND _arpg_line " ")
                    set(_arpg_raw_closer ")${_arpg_raw_delimiter}\"")
                    set(_arpg_state RAW_STRING)
                    math(EXPR _arpg_index "${_arpg_delimiter_index} + 1")
                    continue()
                endif()
            endif()

            if(_arpg_char STREQUAL "\"")
                string(TOLOWER "${_arpg_line}" _arpg_line_lower)
                if(_arpg_line_lower MATCHES
                        "^[ \t]*#[ \t]*(include[ \t]*|define[ \t]+[a-z_][a-z0-9_]*[ \t]+)$")
                    string(APPEND _arpg_line "\"")
                    set(_arpg_state INCLUDE_STRING)
                else()
                    string(APPEND _arpg_line " ")
                    set(_arpg_state STRING)
                endif()
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(_arpg_char STREQUAL "'")
                string(APPEND _arpg_line " ")
                set(_arpg_state CHAR)
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_cpp_finish_sanitized_line()
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(NOT _arpg_char STREQUAL "\r")
                string(APPEND _arpg_line "${_arpg_char}")
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL LINE_COMMENT)
            if(_arpg_char STREQUAL "\n")
                arpg_cpp_finish_sanitized_line()
                set(_arpg_state CODE)
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL BLOCK_COMMENT)
            if(_arpg_char STREQUAL "*"
                    AND _arpg_logical_next_char STREQUAL "/")
                set(_arpg_state CODE)
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_cpp_finish_sanitized_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL RAW_STRING)
            string(LENGTH "${_arpg_raw_closer}" _arpg_closer_length)
            math(EXPR
                _arpg_remaining_length
                "${_arpg_source_length} - ${_arpg_index}")
            if(_arpg_remaining_length GREATER_EQUAL _arpg_closer_length)
                string(SUBSTRING
                    "${SOURCE_TEXT}"
                    ${_arpg_index}
                    ${_arpg_closer_length}
                    _arpg_closer_candidate)
                if(_arpg_closer_candidate STREQUAL _arpg_raw_closer)
                    set(_arpg_state CODE)
                    math(EXPR
                        _arpg_index
                        "${_arpg_index} + ${_arpg_closer_length}")
                    continue()
                endif()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_cpp_finish_sanitized_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL STRING OR _arpg_state STREQUAL CHAR)
            if(_arpg_char STREQUAL "\\")
                math(EXPR
                    _arpg_index "${_arpg_logical_next_index} + 1")
                continue()
            endif()
            if((_arpg_state STREQUAL STRING AND _arpg_char STREQUAL "\"")
                    OR (_arpg_state STREQUAL CHAR AND _arpg_char STREQUAL "'"))
                set(_arpg_state CODE)
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            if(_arpg_char STREQUAL "\n")
                arpg_cpp_finish_sanitized_line()
            endif()
            set(_arpg_index ${_arpg_next_index})
            continue()
        endif()

        if(_arpg_state STREQUAL INCLUDE_STRING)
            if(_arpg_char STREQUAL "\n")
                arpg_cpp_finish_sanitized_line()
                set(_arpg_state CODE)
                set(_arpg_index ${_arpg_next_index})
                continue()
            endif()
            string(APPEND _arpg_line "${_arpg_char}")
            if(_arpg_char STREQUAL "\"")
                set(_arpg_state CODE)
            endif()
            set(_arpg_index ${_arpg_next_index})
        endif()
    endwhile()

    string(APPEND _arpg_sanitized "${_arpg_line}")
    set("${OUT_SOURCE}" "${_arpg_sanitized}" PARENT_SCOPE)
endfunction()
