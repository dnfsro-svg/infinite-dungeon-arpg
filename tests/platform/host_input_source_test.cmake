if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)

function(require_match_count SOURCE PATTERN EXPECTED LABEL)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
    list(LENGTH MATCHES ACTUAL)
    if(NOT ACTUAL EQUAL EXPECTED)
        message(FATAL_ERROR
            "${LABEL}: expected ${EXPECTED} source matches, found ${ACTUAL}")
    endif()
endfunction()

require_match_count(
    "${HOST_INPUT_SOURCE}"
    "platform_key_pressed[ \t\r\n]*\\("
    1
    "pressed sampling sites")
require_match_count(
    "${HOST_INPUT_SOURCE}"
    "platform_key_down[ \t\r\n]*\\("
    1
    "down sampling sites")
require_match_count(
    "${HOST_INPUT_SOURCE}"
    "IsMouseButtonPressed[ \t\r\n]*\\("
    1
    "mouse pressed sampling sites")
require_match_count(
    "${HOST_INPUT_SOURCE}"
    "GetMousePosition[ \t\r\n]*\\("
    1
    "mouse position sampling sites")
require_match_count(
    "${HOST_INPUT_SOURCE}"
    "IsWindowFocused[ \t\r\n]*\\("
    1
    "focus sampling sites")

if(HOST_INPUT_SOURCE MATCHES
        "platform_key_pressed[ \t\r\n]*\\([ \t\r\n]*KEY_(V|R|N)")
    message(FATAL_ERROR "V/R/N must be derived from the stable-key loop")
endif()
if(HOST_INPUT_SOURCE MATCHES "(new[ \t\r\n]|malloc[ \t\r\n]*\\()")
    message(FATAL_ERROR "host input mapping must not allocate")
endif()
if(HOST_SOURCE MATCHES
        "(platform_key_pressed|platform_key_down|IsKeyPressed|IsKeyDown)[ \t\r\n]*\\(")
    message(FATAL_ERROR "raylib_host.cpp must consume the one-sample input module")
endif()
if(HOST_SOURCE MATCHES
        "KEY_(W|A|S|D|J|K|L|E|I|P|R|N|V)([^A-Z0-9_]|$)")
    message(FATAL_ERROR "raylib_host.cpp retains a hard-coded gameplay/global stable key")
endif()
if(NOT HOST_SOURCE MATCHES "sample_physical_keys[ \t\r\n]*\\(")
    message(FATAL_ERROR "raylib host does not sample a physical snapshot")
endif()
if(NOT HOST_SOURCE MATCHES "map_host_frame_input[ \t\r\n]*\\(")
    message(FATAL_ERROR "raylib host does not map the sampled snapshot")
endif()
