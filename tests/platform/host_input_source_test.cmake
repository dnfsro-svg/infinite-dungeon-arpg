if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)

function(splice_cpp_lines SOURCE OUT_SOURCE)
    set(TEXT "${SOURCE}")
    string(LENGTH "${TEXT}" TEXT_LENGTH)
    set(OUTPUT "")
    set(INDEX 0)
    while(INDEX LESS TEXT_LENGTH)
        string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
        if(CURRENT STREQUAL "\\")
            math(EXPR NEXT_INDEX "${INDEX} + 1")
            if(NEXT_INDEX LESS TEXT_LENGTH)
                string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
                if(NEXT STREQUAL "\n")
                    math(EXPR INDEX "${INDEX} + 2")
                    continue()
                endif()
                if(NEXT STREQUAL "\r")
                    math(EXPR AFTER_CR_INDEX "${INDEX} + 2")
                    if(AFTER_CR_INDEX LESS TEXT_LENGTH)
                        string(SUBSTRING "${TEXT}" ${AFTER_CR_INDEX}
                            1 AFTER_CR)
                        if(AFTER_CR STREQUAL "\n")
                            math(EXPR INDEX "${INDEX} + 3")
                            continue()
                        endif()
                    endif()
                endif()
            endif()
        endif()
        string(APPEND OUTPUT "${CURRENT}")
        math(EXPR INDEX "${INDEX} + 1")
    endwhile()
    set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
endfunction()

function(mask_cpp_literals SOURCE OUT_SOURCE)
    set(TEXT "${SOURCE}")
    string(LENGTH "${TEXT}" TEXT_LENGTH)
    set(OUTPUT "")
    set(INDEX 0)
    while(INDEX LESS TEXT_LENGTH)
        string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)

        set(RAW_END -1)
        if(CURRENT STREQUAL "R")
            math(EXPR NEXT_INDEX "${INDEX} + 1")
            if(NEXT_INDEX LESS TEXT_LENGTH)
                string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
                if(NEXT STREQUAL "\"")
                    math(EXPR DELIMITER_START "${INDEX} + 2")
                    set(SCAN ${DELIMITER_START})
                    set(OPEN_PAREN -1)
                    while(SCAN LESS TEXT_LENGTH)
                        math(EXPR DELIMITER_LENGTH
                            "${SCAN} - ${DELIMITER_START}")
                        if(DELIMITER_LENGTH GREATER 16)
                            break()
                        endif()
                        string(SUBSTRING "${TEXT}" ${SCAN} 1 RAW_CHAR)
                        if(RAW_CHAR STREQUAL "(")
                            set(OPEN_PAREN ${SCAN})
                            break()
                        endif()
                        if(RAW_CHAR STREQUAL "\r" OR RAW_CHAR STREQUAL "\n")
                            break()
                        endif()
                        math(EXPR SCAN "${SCAN} + 1")
                    endwhile()
                    if(OPEN_PAREN GREATER_EQUAL 0)
                        math(EXPR DELIMITER_LENGTH
                            "${OPEN_PAREN} - ${DELIMITER_START}")
                        string(SUBSTRING "${TEXT}" ${DELIMITER_START}
                            ${DELIMITER_LENGTH} RAW_DELIMITER)
                        set(RAW_TERMINATOR ")${RAW_DELIMITER}\"")
                        math(EXPR CONTENT_START "${OPEN_PAREN} + 1")
                        math(EXPR REMAINING_LENGTH
                            "${TEXT_LENGTH} - ${CONTENT_START}")
                        string(SUBSTRING "${TEXT}" ${CONTENT_START}
                            ${REMAINING_LENGTH} REMAINING)
                        string(FIND "${REMAINING}" "${RAW_TERMINATOR}"
                            RELATIVE_CLOSE)
                        if(RELATIVE_CLOSE GREATER_EQUAL 0)
                            string(LENGTH "${RAW_TERMINATOR}"
                                TERMINATOR_LENGTH)
                            math(EXPR RAW_END
                                "${CONTENT_START} + ${RELATIVE_CLOSE} + ${TERMINATOR_LENGTH}")
                        endif()
                    endif()
                endif()
            endif()
        endif()

        if(RAW_END GREATER_EQUAL 0)
            while(INDEX LESS RAW_END)
                string(SUBSTRING "${TEXT}" ${INDEX} 1 MASKED_CHAR)
                if(MASKED_CHAR STREQUAL "\r" OR MASKED_CHAR STREQUAL "\n")
                    string(APPEND OUTPUT "${MASKED_CHAR}")
                else()
                    string(APPEND OUTPUT " ")
                endif()
                math(EXPR INDEX "${INDEX} + 1")
            endwhile()
            continue()
        endif()

        if(CURRENT STREQUAL "\"" OR CURRENT STREQUAL "'")
            set(DELIMITER "${CURRENT}")
            string(APPEND OUTPUT " ")
            math(EXPR INDEX "${INDEX} + 1")
            while(INDEX LESS TEXT_LENGTH)
                string(SUBSTRING "${TEXT}" ${INDEX} 1 LITERAL_CHAR)
                if(LITERAL_CHAR STREQUAL "\r" OR LITERAL_CHAR STREQUAL "\n")
                    string(APPEND OUTPUT "${LITERAL_CHAR}")
                    math(EXPR INDEX "${INDEX} + 1")
                    break()
                endif()
                string(APPEND OUTPUT " ")
                math(EXPR INDEX "${INDEX} + 1")
                if(LITERAL_CHAR STREQUAL "\\")
                    if(INDEX LESS TEXT_LENGTH)
                        string(SUBSTRING "${TEXT}" ${INDEX} 1 ESCAPED_CHAR)
                        if(ESCAPED_CHAR STREQUAL "\r"
                                OR ESCAPED_CHAR STREQUAL "\n")
                            string(APPEND OUTPUT "${ESCAPED_CHAR}")
                        else()
                            string(APPEND OUTPUT " ")
                        endif()
                        math(EXPR INDEX "${INDEX} + 1")
                    endif()
                elseif(LITERAL_CHAR STREQUAL DELIMITER)
                    break()
                endif()
            endwhile()
            continue()
        endif()

        string(APPEND OUTPUT "${CURRENT}")
        math(EXPR INDEX "${INDEX} + 1")
    endwhile()
    set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
endfunction()

function(strip_cpp_comments SOURCE OUT_SOURCE)
    set(TEXT "${SOURCE}")
    string(LENGTH "${TEXT}" TEXT_LENGTH)
    set(OUTPUT "")
    set(INDEX 0)
    while(INDEX LESS TEXT_LENGTH)
        string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
        math(EXPR NEXT_INDEX "${INDEX} + 1")
        set(NEXT "")
        if(NEXT_INDEX LESS TEXT_LENGTH)
            string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
        endif()

        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "/")
            string(APPEND OUTPUT "  ")
            math(EXPR INDEX "${INDEX} + 2")
            while(INDEX LESS TEXT_LENGTH)
                string(SUBSTRING "${TEXT}" ${INDEX} 1 COMMENT_CHAR)
                if(COMMENT_CHAR STREQUAL "\r" OR COMMENT_CHAR STREQUAL "\n")
                    string(APPEND OUTPUT "${COMMENT_CHAR}")
                    math(EXPR INDEX "${INDEX} + 1")
                    break()
                endif()
                string(APPEND OUTPUT " ")
                math(EXPR INDEX "${INDEX} + 1")
            endwhile()
            continue()
        endif()

        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "*")
            string(APPEND OUTPUT "  ")
            math(EXPR INDEX "${INDEX} + 2")
            while(INDEX LESS TEXT_LENGTH)
                string(SUBSTRING "${TEXT}" ${INDEX} 1 COMMENT_CHAR)
                math(EXPR COMMENT_NEXT_INDEX "${INDEX} + 1")
                set(COMMENT_NEXT "")
                if(COMMENT_NEXT_INDEX LESS TEXT_LENGTH)
                    string(SUBSTRING "${TEXT}" ${COMMENT_NEXT_INDEX}
                        1 COMMENT_NEXT)
                endif()
                if(COMMENT_CHAR STREQUAL "*" AND COMMENT_NEXT STREQUAL "/")
                    string(APPEND OUTPUT "  ")
                    math(EXPR INDEX "${INDEX} + 2")
                    break()
                endif()
                if(COMMENT_CHAR STREQUAL "\r" OR COMMENT_CHAR STREQUAL "\n")
                    string(APPEND OUTPUT "${COMMENT_CHAR}")
                else()
                    string(APPEND OUTPUT " ")
                endif()
                math(EXPR INDEX "${INDEX} + 1")
            endwhile()
            continue()
        endif()

        string(APPEND OUTPUT "${CURRENT}")
        math(EXPR INDEX "${INDEX} + 1")
    endwhile()
    set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
endfunction()

function(strip_non_code SOURCE OUT_SOURCE)
    splice_cpp_lines("${SOURCE}" SPLICED_SOURCE)
    mask_cpp_literals("${SPLICED_SOURCE}" WITHOUT_LITERALS)
    strip_cpp_comments("${WITHOUT_LITERALS}" CODE_ONLY)
    set("${OUT_SOURCE}" "${CODE_ONLY}" PARENT_SCOPE)
endfunction()

function(require_match_count SOURCE PATTERN EXPECTED LABEL)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
    list(LENGTH MATCHES ACTUAL)
    if(NOT ACTUAL EQUAL EXPECTED)
        message(FATAL_ERROR
            "${LABEL}: expected ${EXPECTED} source matches, found ${ACTUAL}")
    endif()
endfunction()

set(DIRECT_INPUT_PATTERN
    "(platform_key_pressed|platform_key_down|IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV|IsWindowFocused)[ \t\r\n]*\\(")

set(GUARD_SELF_TEST_SOURCE [=[
const char* line_marker = "// IsKeyReleased(KEY_X)"; IsKeyDown(KEY_A);
const char* block_open = "/* GetCharPressed()";
platform_key_pressed(KEY_B);
const char* block_close = "*/";
const char slash = '/'; const char quote = '"'; IsMouseButtonUp(MOUSE_BUTTON_LEFT);
const char* escaped = "quote: \" // IsKeyUp(KEY_Q)"; GetKeyPressed();
const char* raw = R"guard(IsKeyReleased(KEY_X); // raw /* GetMouseWheelMoveV() */)guard";
GetMouseDelta();
const char* unterminated = "GetMousePosition();
IsKeyUp(KEY_Z);
// IsKeyPressed(KEY_C);
/* GetMousePosition(); */
]=])
strip_non_code("${GUARD_SELF_TEST_SOURCE}" GUARD_SELF_TEST_CODE)
require_match_count("${GUARD_SELF_TEST_CODE}" "${DIRECT_INPUT_PATTERN}"
    6 "guard synthetic real calls")
foreach(REQUIRED_REAL_CALL IN ITEMS
        IsKeyDown platform_key_pressed IsMouseButtonUp GetKeyPressed
        GetMouseDelta IsKeyUp)
    if(NOT GUARD_SELF_TEST_CODE MATCHES
            "${REQUIRED_REAL_CALL}[ \t\r\n]*\\(")
        message(FATAL_ERROR
            "guard self-test lost real ${REQUIRED_REAL_CALL} call")
    endif()
endforeach()
foreach(FORBIDDEN_FAKE_CALL IN ITEMS
        IsKeyReleased GetCharPressed GetMouseWheelMoveV
        IsKeyPressed GetMousePosition)
    if(GUARD_SELF_TEST_CODE MATCHES
            "${FORBIDDEN_FAKE_CALL}[ \t\r\n]*\\(")
        message(FATAL_ERROR
            "guard self-test retained non-code ${FORBIDDEN_FAKE_CALL} call")
    endif()
endforeach()

set(GUARD_SPLICE_LF_SOURCE [=[
const char* continued_string = "IsKey\
Down(KEY_FAKE)";
const int continued_character = 'IsKey\
Up';
const char* continued_raw = R"tag(GetChar\
Pressed())tag";
// continued comment \
platform_key_down(KEY_FAKE);
IsKey\
Released(KEY_REAL);
GetMouseX();
]=])
strip_non_code("${GUARD_SPLICE_LF_SOURCE}" GUARD_SPLICE_LF_CODE)
require_match_count("${GUARD_SPLICE_LF_CODE}" "${DIRECT_INPUT_PATTERN}"
    2 "guard LF-spliced real calls")
if(NOT GUARD_SPLICE_LF_CODE MATCHES "IsKeyReleased[ \t\r\n]*\\("
        OR NOT GUARD_SPLICE_LF_CODE MATCHES "GetMouseX[ \t\r\n]*\\(")
    message(FATAL_ERROR "guard LF splice lost a real direct-input call")
endif()
foreach(LF_FAKE_CALL IN ITEMS
        IsKeyDown IsKeyUp GetCharPressed platform_key_down)
    if(GUARD_SPLICE_LF_CODE MATCHES
            "${LF_FAKE_CALL}[ \t\r\n]*\\(")
        message(FATAL_ERROR
            "guard LF splice retained non-code ${LF_FAKE_CALL} call")
    endif()
endforeach()

string(ASCII 13 CARRIAGE_RETURN)
set(BACKSLASH "\\")
string(CONCAT GUARD_SPLICE_CRLF_SOURCE
    "const char* continued_string = \"IsMouseButton${BACKSLASH}${CARRIAGE_RETURN}\n"
    "Down(MOUSE_BUTTON_LEFT)\";\n"
    "const int continued_character = 'IsKey${BACKSLASH}${CARRIAGE_RETURN}\n"
    "Up';\n"
    "const char* continued_raw = R\"tag(GetMouseWheel${BACKSLASH}${CARRIAGE_RETURN}\n"
    "MoveV())tag\";\n"
    "// continued comment ${BACKSLASH}${CARRIAGE_RETURN}\n"
    "GetCharPressed();\n"
    "IsMouseButton${BACKSLASH}${CARRIAGE_RETURN}\n"
    "Released(MOUSE_BUTTON_RIGHT);\n"
    "GetMouseY();\n")
strip_non_code("${GUARD_SPLICE_CRLF_SOURCE}" GUARD_SPLICE_CRLF_CODE)
require_match_count("${GUARD_SPLICE_CRLF_CODE}" "${DIRECT_INPUT_PATTERN}"
    2 "guard CRLF-spliced real calls")
if(NOT GUARD_SPLICE_CRLF_CODE MATCHES
        "IsMouseButtonReleased[ \t\r\n]*\\("
        OR NOT GUARD_SPLICE_CRLF_CODE MATCHES "GetMouseY[ \t\r\n]*\\(")
    message(FATAL_ERROR "guard CRLF splice lost a real direct-input call")
endif()
foreach(CRLF_FAKE_CALL IN ITEMS
        IsMouseButtonDown IsKeyUp GetMouseWheelMoveV GetCharPressed)
    if(GUARD_SPLICE_CRLF_CODE MATCHES
            "${CRLF_FAKE_CALL}[ \t\r\n]*\\(")
        message(FATAL_ERROR
            "guard CRLF splice retained non-code ${CRLF_FAKE_CALL} call")
    endif()
endforeach()

strip_non_code("${HOST_INPUT_SOURCE}" HOST_INPUT_CODE)
strip_non_code("${HOST_SOURCE}" HOST_CODE)
strip_non_code("${INVENTORY_SOURCE}" INVENTORY_CODE)

require_match_count(
    "${HOST_INPUT_CODE}"
    "platform_key_pressed[ \t\r\n]*\\("
    1
    "pressed sampling sites")
require_match_count(
    "${HOST_INPUT_CODE}"
    "platform_key_down[ \t\r\n]*\\("
    1
    "down sampling sites")
require_match_count(
    "${HOST_INPUT_CODE}"
    "IsMouseButtonPressed[ \t\r\n]*\\("
    2
    "mouse pressed sampling sites")
require_match_count(
    "${HOST_INPUT_CODE}"
    "GetMousePosition[ \t\r\n]*\\("
    1
    "mouse position sampling sites")
require_match_count(
    "${HOST_INPUT_CODE}"
    "GetMouseWheelMove[ \t\r\n]*\\("
    1
    "mouse wheel sampling sites")
require_match_count(
    "${HOST_INPUT_CODE}"
    "IsWindowFocused[ \t\r\n]*\\("
    1
    "focus sampling sites")
require_match_count(
    "${HOST_INPUT_CODE}"
    "${DIRECT_INPUT_PATTERN}"
    7
    "all approved host input sampling sites")

if(HOST_INPUT_CODE MATCHES
        "platform_key_pressed[ \t\r\n]*\\([ \t\r\n]*KEY_(V|R|N)")
    message(FATAL_ERROR "V/R/N must be derived from the stable-key loop")
endif()
if(HOST_CODE MATCHES "${DIRECT_INPUT_PATTERN}")
    message(FATAL_ERROR "raylib_host.cpp must consume the one-sample input module")
endif()
if(HOST_CODE MATCHES
        "KEY_(W|A|S|D|J|K|L|E|I|P|R|N|V)([^A-Z0-9_]|$)")
    message(FATAL_ERROR "raylib_host.cpp retains a hard-coded gameplay/global stable key")
endif()
require_match_count(
    "${HOST_CODE}"
    "sample_physical_keys[ \t\r\n]*\\("
    1
    "host physical snapshot calls")
require_match_count(
    "${HOST_CODE}"
    "map_host_frame_input[ \t\r\n]*\\("
    1
    "host logical mapping calls")
if(INVENTORY_CODE MATCHES "${DIRECT_INPUT_PATTERN}")
    message(FATAL_ERROR "inventory renderer must consume mapped frame input")
endif()
if(NOT HOST_CODE MATCHES
        "inventory\\.process_input[ \t\r\n]*\\([^;]*frame_input")
    message(FATAL_ERROR "raylib host must pass mapped frame input to inventory")
endif()
