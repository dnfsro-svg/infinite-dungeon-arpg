if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

set(POISON_HEADER "${RAYLIB_SOURCE_DIR}/direct_input_poison.hpp")
if(NOT EXISTS "${POISON_HEADER}")
    message(FATAL_ERROR
        "direct input poison header is required at ${POISON_HEADER}")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" INPUT_AUTHORITY_SOURCE)
file(READ "${POISON_HEADER}" POISON_SOURCE)

function(require_match_count SOURCE PATTERN EXPECTED LABEL)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
    list(LENGTH MATCHES ACTUAL)
    if(NOT ACTUAL EQUAL EXPECTED)
        message(FATAL_ERROR
            "${LABEL}: expected ${EXPECTED} matches, found ${ACTUAL}")
    endif()
endfunction()

function(require_poison_consumer SOURCE LABEL)
    set(POISON_INCLUDE "#include \"direct_input_poison.hpp\"")
    require_match_count(
        "${SOURCE}" "${POISON_INCLUDE}" 1 "${LABEL} poison includes")
    string(FIND "${SOURCE}" "${POISON_INCLUDE}" INCLUDE_INDEX)
    string(LENGTH "${POISON_INCLUDE}" INCLUDE_LENGTH)
    math(EXPR AFTER_INCLUDE "${INCLUDE_INDEX} + ${INCLUDE_LENGTH}")
    string(SUBSTRING "${SOURCE}" ${AFTER_INCLUDE} -1 INCLUDE_SUFFIX)
    if(INCLUDE_SUFFIX MATCHES "#[ \t]*include")
        message(FATAL_ERROR
            "${LABEL} poison must be the final normal include")
    endif()
    if(NOT INCLUDE_SUFFIX MATCHES
            "static_assert[ \t\r\n]*\\([ \t\r\n]*arpg::platform::direct_input_poison::active")
        message(FATAL_ERROR
            "${LABEL} must assert the active direct input poison sentinel")
    endif()
endfunction()

require_poison_consumer("${HOST_SOURCE}" "raylib host")
require_poison_consumer("${INVENTORY_SOURCE}" "inventory renderer")
if(INPUT_AUTHORITY_SOURCE MATCHES "direct_input_poison\\.hpp")
    message(FATAL_ERROR
        "host_input.cpp is the sampling authority and must not include poison")
endif()

require_match_count(
    "${HOST_SOURCE}"
    "sample_physical_keys[ \t\r\n]*\\("
    1
    "host physical snapshot calls")
require_match_count(
    "${HOST_SOURCE}"
    "map_host_frame_input[ \t\r\n]*\\("
    1
    "host logical mapping calls")

set(POISON_TARGET
    "::arpg::platform::direct_input_poison::blocked")
set(DIRECT_INPUT_APIS
    platform_key_pressed
    platform_key_down
    IsKeyPressed
    IsKeyPressedRepeat
    IsKeyDown
    IsKeyReleased
    IsKeyUp
    GetKeyPressed
    GetCharPressed
    IsMouseButtonPressed
    IsMouseButtonDown
    IsMouseButtonReleased
    IsMouseButtonUp
    GetMouseX
    GetMouseY
    GetMousePosition
    GetMouseDelta
    GetMouseWheelMove
    GetMouseWheelMoveV
    IsWindowFocused)

list(LENGTH DIRECT_INPUT_APIS DIRECT_INPUT_API_COUNT)
if(NOT DIRECT_INPUT_API_COUNT EQUAL 20)
    message(FATAL_ERROR "direct input poison API table must contain 20 entries")
endif()

foreach(DIRECT_INPUT_API IN LISTS DIRECT_INPUT_APIS)
    require_match_count(
        "${POISON_SOURCE}"
        "#define[ \t]+${DIRECT_INPUT_API}[ \t]+${POISON_TARGET}([ \t\r\n]|$)"
        1
        "poison macro ${DIRECT_INPUT_API}")
endforeach()

require_match_count(
    "${POISON_SOURCE}"
    "#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+${POISON_TARGET}([ \t\r\n]|$)"
    20
    "complete direct input poison table")
if(NOT POISON_SOURCE MATCHES
        "inline[ \t]+constexpr[ \t]+bool[ \t]+active[ \t]*=[ \t]*true")
    message(FATAL_ERROR "direct input poison active sentinel is missing")
endif()
