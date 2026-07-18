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
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

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
set(STAGE11B_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}const[ \t]+PhysicalKeySnapshot[ \t]+stage11b_physical_keys[ \t]*=[ \t\r\n]*inject_stage11b_physical_edges[ \t]*\\(")
set(STAGE11C_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}const[ \t]+PhysicalKeySnapshot[ \t]+physical_keys[ \t]*=[ \t]*inject_stage11c_physical_edges[ \t]*\\(")
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
// const PhysicalKeySnapshot stage11b_physical_keys =
//     inject_stage11b_physical_edges(
//     sampled_physical_keys, config, stage11b_validation_state);
// const PhysicalKeySnapshot physical_keys = inject_stage11c_physical_edges(
//     stage11b_physical_keys, config, input_settings, current,
//     stage11c_validation_state);
// HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
// #define IsKeyDown ::arpg::platform::direct_input_poison::blocked
]=])
set(REAL_STRUCTURE [=[
#include "direct_input_poison.hpp"
static_assert(arpg::platform::direct_input_poison::active, "active");
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const PhysicalKeySnapshot stage11b_physical_keys =
    inject_stage11b_physical_edges(
    sampled_physical_keys, config, stage11b_validation_state);
const PhysicalKeySnapshot physical_keys = inject_stage11c_physical_edges(
    stage11b_physical_keys, config, input_settings, current,
    stage11c_validation_state);
HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
DeathInputGate death_gate = host_death_input_gate(
    death_saving, death_pending, frame_input.keys, physical_keys);
const bool pause_blocks_gameplay = pause_open || pause_was_open;
HostFrameGateResult host_gate{};
const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
    passive_overlay_open);
const InventoryInputGate inventory_gate = inventory_input_gate(inventory.is_open());
const bool forward_actions = passive_input_gate.forward_actions
    && inventory_gate.forward_actions
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay;
if (forward_actions) {
    const std::array<bool, 3> accepted_actions =
        submit_frame_actions(*session, frame_input);
}
if (forward_descent && frame_input.keys.e) {}
#define IsKeyDown ::arpg::platform::direct_input_poison::blocked
]=])
poison_macro_line_pattern(IsKeyDown SELF_TEST_MACRO_PATTERN)
foreach(STRUCTURE_PATTERN IN ITEMS
        POISON_INCLUDE_LINE_PATTERN
        ACTIVE_ASSERT_PATTERN
        SAMPLE_CALL_LINE_PATTERN
        STAGE11B_INJECT_CALL_LINE_PATTERN
        STAGE11C_INJECT_CALL_LINE_PATTERN
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
    "${STAGE11B_INJECT_CALL_LINE_PATTERN}"
    1
    "host stage11b physical injection calls")
require_match_count(
    "${HOST_SOURCE}"
    "${STAGE11C_INJECT_CALL_LINE_PATTERN}"
    1
    "host stage11c physical injection calls")
require_match_count(
    "${HOST_SOURCE}"
    "${MAP_CALL_LINE_PATTERN}"
    1
    "host logical mapping calls")

function(physical_input_chain_valid SOURCE OUT_VARIABLE)
    arpg_sanitize_cpp_source("${SOURCE}" SOURCE)
    string(FIND "${SOURCE}"
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys()"
        SAMPLE_INDEX)
    string(FIND "${SOURCE}"
        "const PhysicalKeySnapshot stage11b_physical_keys ="
        STAGE11B_INDEX)
    string(FIND "${SOURCE}"
        "const PhysicalKeySnapshot physical_keys = inject_stage11c_physical_edges("
        STAGE11C_INDEX)
    string(FIND "${SOURCE}"
        "HostFrameInput frame_input = map_host_frame_input("
        MAP_INDEX)
    string(FIND "${SOURCE}"
        "DeathInputGate death_gate = host_death_input_gate("
        DEATH_GATE_INDEX)
    string(FIND "${SOURCE}"
        "const bool pause_blocks_gameplay ="
        PAUSE_BLOCK_INDEX)
    string(FIND "${SOURCE}" "HostFrameGateResult host_gate{}"
        HOST_GATE_INDEX)
    string(FIND "${SOURCE}"
        "const PassiveOverlayInputGate passive_input_gate ="
        PASSIVE_GATE_INDEX)
    string(FIND "${SOURCE}"
        "const InventoryInputGate inventory_gate ="
        INVENTORY_GATE_INDEX)
    string(FIND "${SOURCE}" "const bool forward_actions ="
        FORWARD_ACTIONS_INDEX)
    string(FIND "${SOURCE}" "if (forward_actions) {"
        FORWARD_ACTIONS_IF_INDEX)
    string(FIND "${SOURCE}" "submit_frame_actions(*session, frame_input)"
        SUBMIT_ACTIONS_INDEX)
    string(FIND "${SOURCE}" "if (forward_descent && frame_input.keys.e)"
        FORWARD_DESCENT_INDEX)
    if(SAMPLE_INDEX EQUAL -1 OR STAGE11B_INDEX EQUAL -1
            OR STAGE11C_INDEX EQUAL -1 OR MAP_INDEX EQUAL -1
            OR DEATH_GATE_INDEX EQUAL -1 OR PAUSE_BLOCK_INDEX EQUAL -1
            OR HOST_GATE_INDEX EQUAL -1 OR PASSIVE_GATE_INDEX EQUAL -1
            OR INVENTORY_GATE_INDEX EQUAL -1 OR FORWARD_ACTIONS_INDEX EQUAL -1
            OR FORWARD_ACTIONS_IF_INDEX EQUAL -1 OR SUBMIT_ACTIONS_INDEX EQUAL -1
            OR FORWARD_DESCENT_INDEX EQUAL -1
            OR NOT SAMPLE_INDEX LESS STAGE11B_INDEX
            OR NOT STAGE11B_INDEX LESS STAGE11C_INDEX
            OR NOT STAGE11C_INDEX LESS MAP_INDEX
            OR NOT MAP_INDEX LESS DEATH_GATE_INDEX
            OR NOT DEATH_GATE_INDEX LESS PAUSE_BLOCK_INDEX
            OR NOT PAUSE_BLOCK_INDEX LESS HOST_GATE_INDEX
            OR NOT HOST_GATE_INDEX LESS PASSIVE_GATE_INDEX
            OR NOT PASSIVE_GATE_INDEX LESS INVENTORY_GATE_INDEX
            OR NOT INVENTORY_GATE_INDEX LESS FORWARD_ACTIONS_INDEX
            OR NOT FORWARD_ACTIONS_INDEX LESS FORWARD_ACTIONS_IF_INDEX
            OR NOT FORWARD_ACTIONS_IF_INDEX LESS SUBMIT_ACTIONS_INDEX
            OR NOT SUBMIT_ACTIONS_INDEX LESS FORWARD_DESCENT_INDEX)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR STAGE11B_LENGTH "${STAGE11C_INDEX} - ${STAGE11B_INDEX}")
    string(SUBSTRING "${SOURCE}" ${STAGE11B_INDEX} ${STAGE11B_LENGTH}
        STAGE11B_SOURCE)
    math(EXPR STAGE11C_LENGTH "${MAP_INDEX} - ${STAGE11C_INDEX}")
    string(SUBSTRING "${SOURCE}" ${STAGE11C_INDEX} ${STAGE11C_LENGTH}
        STAGE11C_SOURCE)
    string(SUBSTRING "${SOURCE}" ${MAP_INDEX} -1 MAP_SOURCE)
    string(FIND "${STAGE11B_SOURCE}"
        "sampled_physical_keys, config, stage11b_validation_state)"
        STAGE11B_INPUT_INDEX)
    string(FIND "${STAGE11C_SOURCE}"
        "stage11b_physical_keys, config, input_settings, current,"
        STAGE11C_INPUT_INDEX)
    string(FIND "${STAGE11C_SOURCE}" "stage11c_validation_state)"
        STAGE11C_STATE_INDEX)
    string(FIND "${MAP_SOURCE}" "physical_keys)" MAP_INPUT_INDEX)
    math(EXPR FORWARD_ACTIONS_LENGTH
        "${FORWARD_ACTIONS_IF_INDEX} - ${FORWARD_ACTIONS_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_ACTIONS_INDEX}
        ${FORWARD_ACTIONS_LENGTH} FORWARD_ACTIONS_SOURCE)
    math(EXPR CONTROLLED_SUBMIT_LENGTH
        "${FORWARD_DESCENT_INDEX} - ${FORWARD_ACTIONS_IF_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_ACTIONS_IF_INDEX}
        ${CONTROLLED_SUBMIT_LENGTH} CONTROLLED_SUBMIT_SOURCE)
    string(REGEX MATCHALL "submit_frame_actions[ \t\r\n]*\\("
        SUBMIT_ACTION_CALLS "${SOURCE}")
    list(LENGTH SUBMIT_ACTION_CALLS SUBMIT_ACTION_CALL_COUNT)
    if(STAGE11B_INPUT_INDEX EQUAL -1 OR STAGE11C_INPUT_INDEX EQUAL -1
            OR STAGE11C_STATE_INDEX EQUAL -1 OR MAP_INPUT_INDEX EQUAL -1
            OR NOT SUBMIT_ACTION_CALL_COUNT EQUAL 1
            OR NOT FORWARD_ACTIONS_SOURCE MATCHES
                "const bool forward_actions =[ \t\r\n]*passive_input_gate\\.forward_actions[ \t\r\n]*&&[ \t\r\n]*inventory_gate\\.forward_actions[ \t\r\n]*&&[ \t\r\n]*death_gate\\.forward_gameplay[ \t\r\n]*&&[ \t\r\n]*host_gate\\.forward_gameplay[ \t\r\n]*&&[ \t\r\n]*!pause_blocks_gameplay[ \t\r\n]*;"
            OR NOT CONTROLLED_SUBMIT_SOURCE MATCHES
                "if[ \t\r\n]*\\([ \t\r\n]*forward_actions[ \t\r\n]*\\)[ \t\r\n]*\\{[ \t\r\n]*const[ \t]+std::array<bool,[ \t]*3>[ \t]+accepted_actions[ \t\r\n]*=[ \t\r\n]*submit_frame_actions[ \t\r\n]*\\([ \t\r\n]*\\*session,[ \t\r\n]*frame_input[ \t\r\n]*\\)[ \t\r\n]*;")
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VARIABLE}" TRUE PARENT_SCOPE)
endfunction()

physical_input_chain_valid("${HOST_SOURCE}" HOST_INPUT_CHAIN_VALID)
if(NOT HOST_INPUT_CHAIN_VALID)
    message(FATAL_ERROR
        "host input chain must apply Stage11B then Stage11C before mapping")
endif()
physical_input_chain_valid("${REAL_STRUCTURE}" REFERENCE_INPUT_CHAIN_VALID)
if(NOT REFERENCE_INPUT_CHAIN_VALID)
    message(FATAL_ERROR "host input chain self-check rejected reference")
endif()
set(INPUT_CHAIN_SPOOF_ACCEPTANCES)
set(COMMENT_ONLY_INPUT_CHAIN "/*${REAL_STRUCTURE}*/")
set(STRING_ONLY_INPUT_CHAIN "R\"arpg(${REAL_STRUCTURE})arpg\"")
physical_input_chain_valid("${COMMENT_ONLY_INPUT_CHAIN}"
    COMMENT_ONLY_INPUT_CHAIN_VALID)
if(COMMENT_ONLY_INPUT_CHAIN_VALID)
    list(APPEND INPUT_CHAIN_SPOOF_ACCEPTANCES "comment-only-chain")
endif()
physical_input_chain_valid("${STRING_ONLY_INPUT_CHAIN}"
    STRING_ONLY_INPUT_CHAIN_VALID)
if(STRING_ONLY_INPUT_CHAIN_VALID)
    list(APPEND INPUT_CHAIN_SPOOF_ACCEPTANCES "string-only-chain")
endif()
if(INPUT_CHAIN_SPOOF_ACCEPTANCES)
    list(JOIN INPUT_CHAIN_SPOOF_ACCEPTANCES ", " INPUT_CHAIN_SPOOF_NAMES)
    message(FATAL_ERROR
        "host input chain accepted source spoofs: ${INPUT_CHAIN_SPOOF_NAMES}")
endif()
string(REPLACE
    "stage11b_physical_keys, config, input_settings, current,"
    "sampled_physical_keys, config, input_settings, current,"
    BYPASSED_STAGE11C_STRUCTURE "${REAL_STRUCTURE}")
physical_input_chain_valid("${BYPASSED_STAGE11C_STRUCTURE}"
    BYPASSED_STAGE11C_STRUCTURE_VALID)
if(BYPASSED_STAGE11C_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted Stage11C bypass mutation")
endif()
set(MAP_BEFORE_STAGE11C_STRUCTURE [=[
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const PhysicalKeySnapshot stage11b_physical_keys =
    inject_stage11b_physical_edges(
    sampled_physical_keys, config, stage11b_validation_state);
HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
const PhysicalKeySnapshot physical_keys = inject_stage11c_physical_edges(
    stage11b_physical_keys, config, input_settings, current,
    stage11c_validation_state);
]=])
physical_input_chain_valid("${MAP_BEFORE_STAGE11C_STRUCTURE}"
    MAP_BEFORE_STAGE11C_STRUCTURE_VALID)
if(MAP_BEFORE_STAGE11C_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted map-before-Stage11C mutation")
endif()

set(SUBMIT_DECLARATION [=[
                const std::array<bool, 3> accepted_actions =
                    submit_frame_actions(*session, frame_input);
]=])
set(HOST_GATE_DECLARATION
    "            HostFrameGateResult host_gate{};")
string(REPLACE "${SUBMIT_DECLARATION}" ""
    SUBMIT_BEFORE_GATE_STRUCTURE "${HOST_SOURCE}")
string(REPLACE "${HOST_GATE_DECLARATION}"
    "${SUBMIT_DECLARATION}\n${HOST_GATE_DECLARATION}"
    SUBMIT_BEFORE_GATE_STRUCTURE "${SUBMIT_BEFORE_GATE_STRUCTURE}")
string(REPLACE "            if (forward_actions) {"
    "            {"
    UNCONDITIONAL_SUBMIT_STRUCTURE "${HOST_SOURCE}")
set(ACCEPTED_SUBMIT_MUTATIONS)
physical_input_chain_valid("${SUBMIT_BEFORE_GATE_STRUCTURE}"
    SUBMIT_BEFORE_GATE_STRUCTURE_VALID)
if(SUBMIT_BEFORE_GATE_STRUCTURE_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "submit-before-gate")
endif()
physical_input_chain_valid("${UNCONDITIONAL_SUBMIT_STRUCTURE}"
    UNCONDITIONAL_SUBMIT_STRUCTURE_VALID)
if(UNCONDITIONAL_SUBMIT_STRUCTURE_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "unconditional-submit-bypass")
endif()
if(ACCEPTED_SUBMIT_MUTATIONS)
    list(JOIN ACCEPTED_SUBMIT_MUTATIONS ", " ACCEPTED_SUBMIT_MUTATION_NAMES)
    message(FATAL_ERROR
        "host input chain accepted mutations: ${ACCEPTED_SUBMIT_MUTATION_NAMES}")
endif()

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
    math(EXPR RECOVERY_LENGTH "${RECOVERY_PRESENT} - ${RECOVERY_START}")
    string(SUBSTRING "${CODE}" ${RECOVERY_START} ${RECOVERY_LENGTH} RECOVERY_BRANCH)
    string(REGEX MATCH
        "renderer\\.observe_presented_hud_frame\\([ \t\r\n]*HudPresentedFrame::recovery"
        RECOVERY_OWNER_MATCH "${RECOVERY_BRANCH}")
    math(EXPR NORMAL_LENGTH "${NORMAL_PRESENT} - ${AFTER_RECOVERY_PRESENT}")
    string(SUBSTRING "${CODE}" ${AFTER_RECOVERY_PRESENT} ${NORMAL_LENGTH} NORMAL_BRANCH)
    string(REGEX MATCH
        "current\\.death\\.has_value\\(\\)[ \t\r\n]*\\?[ \t\r\n]*HudPresentedFrame::death_overlay[ \t\r\n]*:[ \t\r\n]*HudPresentedFrame::normal"
        NORMAL_DEATH_OWNER_MATCH "${NORMAL_BRANCH}")
    string(REGEX MATCH
        "renderer\\.observe_presented_hud_frame\\([ \t\r\n]*hud_presented_frame"
        NORMAL_OBSERVE_OWNER_MATCH "${NORMAL_BRANCH}")
    if(RECOVERY_OWNER_MATCH STREQUAL "" OR NORMAL_DEATH_OWNER_MATCH STREQUAL ""
            OR NORMAL_OBSERVE_OWNER_MATCH STREQUAL "")
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
string(REPLACE "HudPresentedFrame::recovery" "HudPresentedFrame::normal"
    RECOVERY_OWNER_MUTATION_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${RECOVERY_OWNER_MUTATION_SOURCE}" RECOVERY_OWNER_MUTATION_VALID)
if(RECOVERY_OWNER_MUTATION_VALID)
    message(FATAL_ERROR "recovery owner mutation must not validate")
endif()
string(REPLACE "HudPresentedFrame::death_overlay" "HudPresentedFrame::normal"
    DEATH_OWNER_MUTATION_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${DEATH_OWNER_MUTATION_SOURCE}" DEATH_OWNER_MUTATION_VALID)
if(DEATH_OWNER_MUTATION_VALID)
    message(FATAL_ERROR "death owner mutation must not validate")
endif()
string(REPLACE "? HudPresentedFrame::death_overlay : HudPresentedFrame::normal"
    "? HudPresentedFrame::normal : HudPresentedFrame::death_overlay"
    TERNARY_OWNER_MUTATION_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${TERNARY_OWNER_MUTATION_SOURCE}" TERNARY_OWNER_MUTATION_VALID)
if(TERNARY_OWNER_MUTATION_VALID)
    message(FATAL_ERROR "death owner ternary mutation must not validate")
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
