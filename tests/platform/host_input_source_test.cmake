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

set(SOURCE_LINE_START "(^|\n)[ \t]*")
set(SOURCE_LINE_END "[ \t]*(\r?\n|$)")
set(POISON_INCLUDE_LINE_PATTERN
    "${SOURCE_LINE_START}#[ \t]*include[ \t]+\"direct_input_poison\\.hpp\"${SOURCE_LINE_END}")
set(ANY_INCLUDE_LINE_PATTERN
    "${SOURCE_LINE_START}#[ \t]*include[ \t]+")
set(ACTIVE_ASSERT_PATTERN
    "${SOURCE_LINE_START}static_assert[ \t]*\\([ \t]*arpg::platform::direct_input_poison::active[ \t]*(,|\\))")
set(SAMPLE_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}const[ \t]+PhysicalKeySnapshot[ \t]+sampled_physical_keys[ \t]*=[ \t]*sample_physical_keys[ \t]*\\([ \t]*\\)")
set(INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}const[ \t]+PhysicalKeySnapshot[ \t]+physical_keys[ \t]*=[ \t]*inject_stage11b_physical_edges[ \t]*\\(")
set(MAP_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}HostFrameInput[ \t]+frame_input[ \t]*=[ \t]*map_host_frame_input[ \t]*\\(")
set(ACTIVE_SENTINEL_PATTERN
    "${SOURCE_LINE_START}inline[ \t]+constexpr[ \t]+bool[ \t]+active[ \t]*=[ \t]*true")

function(poison_macro_line_pattern API OUT_PATTERN)
    set("${OUT_PATTERN}"
        "(^|\n)#define[ \t]+${API}[ \t]+::arpg::platform::direct_input_poison::blocked${SOURCE_LINE_END}"
        PARENT_SCOPE)
endfunction()

set(COMMENT_ONLY_STRUCTURE [=[
// #include "direct_input_poison.hpp"
// static_assert(arpg::platform::direct_input_poison::active);
// const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
// const PhysicalKeySnapshot physical_keys = inject_stage11b_physical_edges(
//     sampled_physical_keys, config, stage11b_validation_state);
// HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
// #define IsKeyDown ::arpg::platform::direct_input_poison::blocked
]=])
set(REAL_STRUCTURE [=[
#include "direct_input_poison.hpp"
static_assert(arpg::platform::direct_input_poison::active, "active");
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const PhysicalKeySnapshot physical_keys = inject_stage11b_physical_edges(
    sampled_physical_keys, config, stage11b_validation_state);
HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
#define IsKeyDown ::arpg::platform::direct_input_poison::blocked
]=])
poison_macro_line_pattern(IsKeyDown SELF_TEST_MACRO_PATTERN)
foreach(STRUCTURE_PATTERN IN ITEMS
        POISON_INCLUDE_LINE_PATTERN
        ACTIVE_ASSERT_PATTERN
        SAMPLE_CALL_LINE_PATTERN
        INJECT_CALL_LINE_PATTERN
        MAP_CALL_LINE_PATTERN
        SELF_TEST_MACRO_PATTERN)
    require_match_count(
        "${COMMENT_ONLY_STRUCTURE}"
        "${${STRUCTURE_PATTERN}}"
        0
        "commented ${STRUCTURE_PATTERN}")
    require_match_count(
        "${REAL_STRUCTURE}"
        "${${STRUCTURE_PATTERN}}"
        1
        "real ${STRUCTURE_PATTERN}")
endforeach()

function(require_poison_consumer SOURCE LABEL)
    require_match_count(
        "${SOURCE}"
        "${POISON_INCLUDE_LINE_PATTERN}"
        1
        "${LABEL} poison includes")
    string(REGEX MATCH
        "${POISON_INCLUDE_LINE_PATTERN}"
        POISON_INCLUDE_LINE
        "${SOURCE}")
    string(FIND "${SOURCE}" "${POISON_INCLUDE_LINE}" INCLUDE_INDEX)
    string(LENGTH "${POISON_INCLUDE_LINE}" INCLUDE_LENGTH)
    math(EXPR AFTER_INCLUDE "${INCLUDE_INDEX} + ${INCLUDE_LENGTH}")
    string(SUBSTRING "${SOURCE}" ${AFTER_INCLUDE} -1 INCLUDE_SUFFIX)
    if(INCLUDE_SUFFIX MATCHES "${ANY_INCLUDE_LINE_PATTERN}")
        message(FATAL_ERROR
            "${LABEL} poison must be the final normal include")
    endif()
    require_match_count(
        "${INCLUDE_SUFFIX}"
        "${ACTIVE_ASSERT_PATTERN}"
        1
        "${LABEL} active poison assertions")
endfunction()

require_poison_consumer("${HOST_SOURCE}" "raylib host")
require_poison_consumer("${INVENTORY_SOURCE}" "inventory renderer")
require_match_count(
    "${INPUT_AUTHORITY_SOURCE}"
    "${POISON_INCLUDE_LINE_PATTERN}"
    0
    "host input authority poison includes")

require_match_count(
    "${HOST_SOURCE}"
    "${SAMPLE_CALL_LINE_PATTERN}"
    1
    "host physical snapshot calls")
require_match_count(
    "${HOST_SOURCE}"
    "${INJECT_CALL_LINE_PATTERN}"
    1
    "host stage11b physical injection calls")
require_match_count(
    "${HOST_SOURCE}"
    "${MAP_CALL_LINE_PATTERN}"
    1
    "host logical mapping calls")

# Every host path that presents through the shared EndDrawing helper must use
# the production HUD presentation seam exactly once.  This binds normal,
# recovery and death-overlay ownership without registering a Task8 guard.
require_match_count(
    "${HOST_SOURCE}"
    "renderer\\.observe_presented_hud_frame\\("
    2
    "host presented HUD observation seam calls")
require_match_count(
    "${HOST_SOURCE}"
    "HudPresentedFrame::recovery"
    1
    "host recovery HUD observation owner")
require_match_count(
    "${HOST_SOURCE}"
    "HudPresentedFrame::death_overlay"
    1
    "host death HUD observation owner")

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
    poison_macro_line_pattern(
        "${DIRECT_INPUT_API}" DIRECT_INPUT_MACRO_PATTERN)
    require_match_count(
        "${POISON_SOURCE}"
        "${DIRECT_INPUT_MACRO_PATTERN}"
        1
        "poison macro ${DIRECT_INPUT_API}")
endforeach()

require_match_count(
    "${POISON_SOURCE}"
    "(^|\n)#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+${POISON_TARGET}"
    20
    "complete direct input poison table")
require_match_count(
    "${POISON_SOURCE}"
    "${ACTIVE_SENTINEL_PATTERN}"
    1
    "direct input poison active sentinels")
