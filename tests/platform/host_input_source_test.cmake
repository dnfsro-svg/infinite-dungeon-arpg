if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)

function(strip_non_code SOURCE OUT_SOURCE)
    set(CODE "${SOURCE}")
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" CODE "${CODE}")
    string(REGEX REPLACE "//[^\r\n]*" "" CODE "${CODE}")
    string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "\"\"" CODE "${CODE}")
    set("${OUT_SOURCE}" "${CODE}" PARENT_SCOPE)
endfunction()

strip_non_code("${HOST_INPUT_SOURCE}" HOST_INPUT_CODE)
strip_non_code("${HOST_SOURCE}" HOST_CODE)
strip_non_code("${INVENTORY_SOURCE}" INVENTORY_CODE)

function(require_match_count SOURCE PATTERN EXPECTED LABEL)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
    list(LENGTH MATCHES ACTUAL)
    if(NOT ACTUAL EQUAL EXPECTED)
        message(FATAL_ERROR
            "${LABEL}: expected ${EXPECTED} source matches, found ${ACTUAL}")
    endif()
endfunction()

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

if(HOST_INPUT_CODE MATCHES
        "platform_key_pressed[ \t\r\n]*\\([ \t\r\n]*KEY_(V|R|N)")
    message(FATAL_ERROR "V/R/N must be derived from the stable-key loop")
endif()
if(HOST_CODE MATCHES
        "(platform_key_pressed|platform_key_down|IsKeyPressed|IsKeyDown)[ \t\r\n]*\\(")
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
if(INVENTORY_CODE MATCHES
        "(IsMouseButtonPressed|GetMousePosition|GetMouseWheelMove|platform_key_down)[ \t\r\n]*\\(")
    message(FATAL_ERROR "inventory renderer must consume mapped frame input")
endif()
if(NOT HOST_CODE MATCHES
        "inventory\\.process_input[ \t\r\n]*\\([^;]*frame_input")
    message(FATAL_ERROR "raylib host must pass mapped frame input to inventory")
endif()
