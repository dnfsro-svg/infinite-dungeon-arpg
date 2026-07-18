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

function(strip_cpp_noncode SOURCE OUT_VARIABLE)
    set(CODE "${SOURCE}")
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" CODE "${CODE}")
    string(REGEX REPLACE "//[^\r\n]*" "" CODE "${CODE}")
    string(REGEX REPLACE "\"[^\"]*\"" "\"\"" CODE "${CODE}")
    set("${OUT_VARIABLE}" "${CODE}" PARENT_SCOPE)
endfunction()

function(hud_present_structure_valid SOURCE OUT_VARIABLE)
    strip_cpp_noncode("${SOURCE}" CODE)
    string(REGEX MATCHALL "renderer\\.observe_presented_hud_frame\\(" OBSERVES "${CODE}")
    list(LENGTH OBSERVES OBSERVE_COUNT)
    string(REGEX MATCHALL "present_frame_and_maybe_capture\\(" PRESENTS "${CODE}")
    list(LENGTH PRESENTS PRESENT_COUNT)
    if(NOT OBSERVE_COUNT EQUAL 2 OR NOT PRESENT_COUNT EQUAL 3)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()

    string(FIND "${CODE}" "if (runtime.state() == DungeonRuntimeState::recovery_required)" RECOVERY_START)
    string(FIND "${CODE}" "draw_recovery_screen(runtime.render_status());" RECOVERY_DRAW)
    string(FIND "${CODE}" "present_frame_and_maybe_capture(capture_path.has_value()" RECOVERY_PRESENT)
    string(FIND "${CODE}" "renderer.observe_presented_hud_frame(" FIRST_OBSERVE)
    if(RECOVERY_START LESS 0 OR RECOVERY_DRAW LESS 0 OR RECOVERY_PRESENT LESS 0
            OR FIRST_OBSERVE LESS RECOVERY_START OR FIRST_OBSERVE GREATER RECOVERY_DRAW
            OR RECOVERY_DRAW GREATER RECOVERY_PRESENT)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()

    math(EXPR AFTER_FIRST_OBSERVE "${FIRST_OBSERVE} + 1")
    string(SUBSTRING "${CODE}" ${AFTER_FIRST_OBSERVE} -1 AFTER_FIRST_OBSERVE_CODE)
    string(FIND "${AFTER_FIRST_OBSERVE_CODE}" "renderer.observe_presented_hud_frame(" SECOND_OBSERVE_RELATIVE)
    if(SECOND_OBSERVE_RELATIVE LESS 0)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR SECOND_OBSERVE "${AFTER_FIRST_OBSERVE} + ${SECOND_OBSERVE_RELATIVE}")
    math(EXPR AFTER_RECOVERY_PRESENT "${RECOVERY_PRESENT} + 1")
    string(SUBSTRING "${CODE}" ${AFTER_RECOVERY_PRESENT} -1 AFTER_RECOVERY_CODE)
    string(FIND "${AFTER_RECOVERY_CODE}" "BeginDrawing();" NORMAL_BEGIN_RELATIVE)
    string(FIND "${AFTER_RECOVERY_CODE}" "present_frame_and_maybe_capture(capture_path.has_value()" NORMAL_PRESENT_RELATIVE)
    if(NORMAL_BEGIN_RELATIVE LESS 0 OR NORMAL_PRESENT_RELATIVE LESS 0)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR NORMAL_BEGIN "${AFTER_RECOVERY_PRESENT} + ${NORMAL_BEGIN_RELATIVE}")
    math(EXPR NORMAL_PRESENT "${AFTER_RECOVERY_PRESENT} + ${NORMAL_PRESENT_RELATIVE}")
    if(SECOND_OBSERVE LESS RECOVERY_PRESENT OR SECOND_OBSERVE GREATER NORMAL_BEGIN
            OR NORMAL_BEGIN GREATER NORMAL_PRESENT)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VARIABLE}" TRUE PARENT_SCOPE)
endfunction()

# Bind exactly two actual paths to the production seam after stripping comments
# and strings. Self fixtures reject a third present, a post-BeginDrawing seam,
# and comment-only tokens without registering a separate Task8 guard.
hud_present_structure_valid("${HOST_SOURCE}" HOST_PRESENT_STRUCTURE_VALID)
if(NOT HOST_PRESENT_STRUCTURE_VALID)
    message(FATAL_ERROR "host HUD presentation seam structure is invalid")
endif()
set(COMMENT_ONLY_HUD_PRESENT [=[
// renderer.observe_presented_hud_frame(HudPresentedFrame::recovery);
// present_frame_and_maybe_capture(capture_path.has_value() ? path : nullptr);
]=])
hud_present_structure_valid("${COMMENT_ONLY_HUD_PRESENT}" COMMENT_ONLY_HUD_PRESENT_VALID)
if(COMMENT_ONLY_HUD_PRESENT_VALID)
    message(FATAL_ERROR "comment-only HUD presentation tokens must not validate")
endif()
set(THIRD_PRESENT_HUD_SOURCE "${HOST_SOURCE}\npresent_frame_and_maybe_capture(nullptr);")
hud_present_structure_valid("${THIRD_PRESENT_HUD_SOURCE}" THIRD_PRESENT_HUD_VALID)
if(THIRD_PRESENT_HUD_VALID)
    message(FATAL_ERROR "third HUD present path must not validate")
endif()
string(REPLACE "renderer.observe_presented_hud_frame(hud_presented_frame,"
    "BeginDrawing();\nrenderer.observe_presented_hud_frame(hud_presented_frame,"
    MOVED_HUD_OBSERVE_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${MOVED_HUD_OBSERVE_SOURCE}" MOVED_HUD_OBSERVE_VALID)
if(MOVED_HUD_OBSERVE_VALID)
    message(FATAL_ERROR "HUD seam after BeginDrawing must not validate")
endif()

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
