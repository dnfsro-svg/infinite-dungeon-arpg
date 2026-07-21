if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/combat_renderer.cpp" RENDERER_SOURCE)
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
arpg_sanitize_cpp_source("${HOST_SOURCE}" HOST_SANITIZED_SOURCE)

if(HOST_SOURCE MATCHES "SetTargetFPS[ \t\r\n]*\\(")
    message(FATAL_ERROR "raylib host applies a software frame cap on top of VSync")
endif()
if(NOT HOST_SOURCE MATCHES "FLAG_VSYNC_HINT")
    message(FATAL_ERROR "raylib host must retain VSync")
endif()
if(RENDERER_SOURCE MATCHES
        "const float y = projected\\.ground_y;")
    message(FATAL_ERROR "monster body rendering ignores projected airborne Y")
endif()
if(RENDERER_SOURCE MATCHES
        "previous_combat\\.player\\.position, current_combat\\.player\\.position")
    message(FATAL_ERROR "local player rendering is one interpolated snapshot behind")
endif()

string(FIND
    "${HOST_SANITIZED_SOURCE}"
    "HostFrameGateResult gate_host_frame"
    PAUSE_GATE_START)
string(FIND
    "${HOST_SANITIZED_SOURCE}"
    "HostExitCode run_raylib_host"
    HOST_ENTRY_START)
string(FIND
    "${HOST_SANITIZED_SOURCE}"
    "bool settle_host_pause_command"
    SETTINGS_SETTLE_START)
if(PAUSE_GATE_START EQUAL -1 OR HOST_ENTRY_START EQUAL -1
        OR NOT PAUSE_GATE_START LESS HOST_ENTRY_START)
    message(FATAL_ERROR "raylib host pause frame gate is missing")
endif()
math(EXPR PAUSE_GATE_LENGTH "${HOST_ENTRY_START} - ${PAUSE_GATE_START}")
string(SUBSTRING
    "${HOST_SANITIZED_SOURCE}"
    ${PAUSE_GATE_START}
    ${PAUSE_GATE_LENGTH}
    PAUSE_GATE_SOURCE)
if(NOT PAUSE_GATE_SOURCE MATCHES
        "if[ \\t\\n]*\\([ \\t\\n]*paused[ \\t\\n]*\\)")
    message(FATAL_ERROR "raylib host pause gate must branch on paused")
endif()
string(FIND
    "${PAUSE_GATE_SOURCE}"
    "fixed_step.clear_accumulator()"
    PAUSE_CLEAR_INDEX)
string(FIND
    "${PAUSE_GATE_SOURCE}"
    "fixed_step.advance(frame_seconds)"
    PAUSE_ADVANCE_INDEX)
if(PAUSE_CLEAR_INDEX EQUAL -1)
    message(FATAL_ERROR "pause entry must clear the fixed-step accumulator")
endif()
if(PAUSE_ADVANCE_INDEX EQUAL -1)
    message(FATAL_ERROR "unpaused host frame must advance fixed step")
endif()
if(NOT PAUSE_CLEAR_INDEX LESS PAUSE_ADVANCE_INDEX)
    message(FATAL_ERROR "fixed_step.advance must remain outside paused branch")
endif()

string(FIND "${HOST_SANITIZED_SOURCE}"
    "DeathInputGate host_death_input_gate" DEATH_HELPER_START)
string(FIND "${HOST_SANITIZED_SOURCE}"
    "HostSettingsNotice make_host_settings_notice" NOTICE_HELPER_START)
if(DEATH_HELPER_START EQUAL -1 OR NOTICE_HELPER_START EQUAL -1
        OR NOT DEATH_HELPER_START LESS NOTICE_HELPER_START)
    message(FATAL_ERROR "fixed-E death input helper is missing")
endif()
math(EXPR DEATH_HELPER_LENGTH "${NOTICE_HELPER_START} - ${DEATH_HELPER_START}")
string(SUBSTRING "${HOST_SANITIZED_SOURCE}" ${DEATH_HELPER_START}
    ${DEATH_HELPER_LENGTH} DEATH_HELPER_SOURCE)
if(NOT DEATH_HELPER_SOURCE MATCHES
        "stable_pressed[ \\t\\n]*\\([ \\t\\n]*physical_keys[ \\t\\n]*,[ \\t\\n]*settings::StableKey::e")
    message(FATAL_ERROR "death continue must use fixed StableKey::e from the frame snapshot")
endif()

string(SUBSTRING "${HOST_SANITIZED_SOURCE}" ${HOST_ENTRY_START} -1 HOST_ENTRY_SOURCE)
string(REGEX MATCHALL "sample_physical_keys[ \\t\\n]*\\(" SAMPLE_CALLS
    "${HOST_ENTRY_SOURCE}")
list(LENGTH SAMPLE_CALLS SAMPLE_CALL_COUNT)
if(NOT SAMPLE_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR "raylib host must sample physical keys exactly once per frame")
endif()

function(arpg_physical_input_chain_is_valid SOURCE OUT_VALID)
    arpg_sanitize_cpp_source("${SOURCE}" SOURCE)
    set(WS "[ \t\r\n]*")
    set(WS1 "[ \t\r\n]+")
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys()" SAMPLE_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot stage11b_physical_keys =" STAGE11B_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot stage11c_physical_keys = inject_stage11c_physical_edges(" STAGE11C_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges(" STAGE11D_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot stage17_physical_keys =" STAGE17_INDEX)
    string(FIND "${SOURCE}" "HostFrameInput frame_input = map_host_frame_input(" MAP_INDEX)
    string(FIND "${SOURCE}" "DeathInputGate death_gate = host_death_input_gate(" DEATH_GATE_INDEX)
    string(FIND "${SOURCE}" "const bool pause_blocks_gameplay =" PAUSE_BLOCK_INDEX)
    string(FIND "${SOURCE}" "HostFrameGateResult host_gate{}" HOST_GATE_INDEX)
    string(FIND "${SOURCE}" "const PassiveOverlayInputGate passive_input_gate =" PASSIVE_GATE_INDEX)
    string(FIND "${SOURCE}" "const InventoryInputGate inventory_gate =" INVENTORY_GATE_INDEX)
    string(FIND "${SOURCE}" "const bool forward_actions =" FORWARD_ACTIONS_INDEX)
    string(FIND "${SOURCE}" "const bool forward_descent =" FORWARD_DESCENT_DECL_INDEX)
    string(FIND "${SOURCE}" "if (forward_actions) {" FORWARD_ACTIONS_IF_INDEX)
    string(FIND "${SOURCE}" "submit_frame_actions(*session, frame_input)" SUBMIT_ACTIONS_INDEX)
    string(FIND "${SOURCE}" "if (forward_descent && frame_input.keys.e)" FORWARD_DESCENT_INDEX)
    string(FIND "${SOURCE}" "session->request_descent(in_range)" REQUEST_DESCENT_INDEX)
    string(FIND "${SOURCE}" "const combat::MovementInput movement = forward_movement" MOVEMENT_INPUT_INDEX)
    if(SAMPLE_INDEX EQUAL -1 OR STAGE11B_INDEX EQUAL -1 OR STAGE11C_INDEX EQUAL -1
            OR STAGE11D_INDEX EQUAL -1 OR STAGE17_INDEX EQUAL -1
            OR MAP_INDEX EQUAL -1 OR DEATH_GATE_INDEX EQUAL -1 OR PAUSE_BLOCK_INDEX EQUAL -1
            OR HOST_GATE_INDEX EQUAL -1 OR PASSIVE_GATE_INDEX EQUAL -1 OR INVENTORY_GATE_INDEX EQUAL -1
            OR FORWARD_ACTIONS_INDEX EQUAL -1 OR FORWARD_DESCENT_DECL_INDEX EQUAL -1
            OR FORWARD_ACTIONS_IF_INDEX EQUAL -1 OR SUBMIT_ACTIONS_INDEX EQUAL -1
            OR FORWARD_DESCENT_INDEX EQUAL -1 OR REQUEST_DESCENT_INDEX EQUAL -1
            OR MOVEMENT_INPUT_INDEX EQUAL -1
            OR NOT SAMPLE_INDEX LESS STAGE11B_INDEX OR NOT STAGE11B_INDEX LESS STAGE11C_INDEX
            OR NOT STAGE11C_INDEX LESS STAGE11D_INDEX
            OR NOT STAGE11D_INDEX LESS STAGE17_INDEX
            OR NOT STAGE17_INDEX LESS MAP_INDEX
            OR NOT MAP_INDEX LESS DEATH_GATE_INDEX
            OR NOT DEATH_GATE_INDEX LESS PAUSE_BLOCK_INDEX OR NOT PAUSE_BLOCK_INDEX LESS HOST_GATE_INDEX
            OR NOT HOST_GATE_INDEX LESS PASSIVE_GATE_INDEX OR NOT PASSIVE_GATE_INDEX LESS INVENTORY_GATE_INDEX
            OR NOT INVENTORY_GATE_INDEX LESS FORWARD_ACTIONS_INDEX
            OR NOT FORWARD_ACTIONS_INDEX LESS FORWARD_DESCENT_DECL_INDEX
            OR NOT FORWARD_DESCENT_DECL_INDEX LESS FORWARD_ACTIONS_IF_INDEX
            OR NOT FORWARD_ACTIONS_IF_INDEX LESS SUBMIT_ACTIONS_INDEX
            OR NOT SUBMIT_ACTIONS_INDEX LESS FORWARD_DESCENT_INDEX
            OR NOT FORWARD_DESCENT_INDEX LESS REQUEST_DESCENT_INDEX
            OR NOT REQUEST_DESCENT_INDEX LESS MOVEMENT_INPUT_INDEX)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()

    math(EXPR STAGE11B_LENGTH "${STAGE11C_INDEX} - ${STAGE11B_INDEX}")
    string(SUBSTRING "${SOURCE}" ${STAGE11B_INDEX} ${STAGE11B_LENGTH} STAGE11B_SOURCE)
    math(EXPR STAGE11C_LENGTH "${STAGE11D_INDEX} - ${STAGE11C_INDEX}")
    string(SUBSTRING "${SOURCE}" ${STAGE11C_INDEX} ${STAGE11C_LENGTH} STAGE11C_SOURCE)
    math(EXPR STAGE11D_LENGTH "${STAGE17_INDEX} - ${STAGE11D_INDEX}")
    string(SUBSTRING "${SOURCE}" ${STAGE11D_INDEX} ${STAGE11D_LENGTH} STAGE11D_SOURCE)
    math(EXPR STAGE17_LENGTH "${MAP_INDEX} - ${STAGE17_INDEX}")
    string(SUBSTRING "${SOURCE}" ${STAGE17_INDEX} ${STAGE17_LENGTH} STAGE17_SOURCE)
    string(SUBSTRING "${SOURCE}" ${MAP_INDEX} -1 MAP_SOURCE)
    math(EXPR HOST_GATE_LENGTH "${PASSIVE_GATE_INDEX} - ${HOST_GATE_INDEX}")
    string(SUBSTRING "${SOURCE}" ${HOST_GATE_INDEX} ${HOST_GATE_LENGTH} HOST_GATE_SOURCE)
    math(EXPR HOST_CHAIN_LENGTH "${MOVEMENT_INPUT_INDEX} - ${MAP_INDEX}")
    string(SUBSTRING "${SOURCE}" ${MAP_INDEX} ${HOST_CHAIN_LENGTH} HOST_CHAIN_SOURCE)
    math(EXPR FORWARD_ACTIONS_LENGTH "${FORWARD_ACTIONS_IF_INDEX} - ${FORWARD_ACTIONS_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_ACTIONS_INDEX} ${FORWARD_ACTIONS_LENGTH} FORWARD_ACTIONS_SOURCE)
    math(EXPR CONTROLLED_SUBMIT_LENGTH "${FORWARD_DESCENT_INDEX} - ${FORWARD_ACTIONS_IF_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_ACTIONS_IF_INDEX} ${CONTROLLED_SUBMIT_LENGTH} CONTROLLED_SUBMIT_SOURCE)
    math(EXPR FORWARD_DESCENT_LENGTH "${FORWARD_DESCENT_INDEX} - ${FORWARD_DESCENT_DECL_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_DESCENT_DECL_INDEX} ${FORWARD_DESCENT_LENGTH} FORWARD_DESCENT_SOURCE)
    math(EXPR CONTROLLED_DESCENT_LENGTH "${MOVEMENT_INPUT_INDEX} - ${FORWARD_DESCENT_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_DESCENT_INDEX} ${CONTROLLED_DESCENT_LENGTH} CONTROLLED_DESCENT_SOURCE)

    string(REGEX MATCHALL "submit_frame_actions${WS}\\(" SUBMIT_ACTION_CALLS "${SOURCE}")
    list(LENGTH SUBMIT_ACTION_CALLS SUBMIT_ACTION_CALL_COUNT)
    string(REGEX MATCHALL "host_gate${WS}=${WS}gate_host_frame${WS}\\(" HOST_GATE_CALLS "${HOST_GATE_SOURCE}")
    list(LENGTH HOST_GATE_CALLS HOST_GATE_CALL_COUNT)
    string(REGEX MATCHALL "session->request_descent${WS}\\(" REQUEST_DESCENT_CALLS "${SOURCE}")
    list(LENGTH REQUEST_DESCENT_CALLS REQUEST_DESCENT_CALL_COUNT)
    string(REGEX MATCHALL "forward_descent${WS}=" FORWARD_DESCENT_ASSIGNMENTS "${HOST_CHAIN_SOURCE}")
    list(LENGTH FORWARD_DESCENT_ASSIGNMENTS FORWARD_DESCENT_ASSIGNMENT_COUNT)

    if(NOT STAGE11B_SOURCE MATCHES "inject_stage11b_physical_edges${WS}\\(${WS}sampled_physical_keys,${WS}config,${WS}stage11b_validation_state${WS}\\)"
            OR NOT STAGE11C_SOURCE MATCHES "inject_stage11c_physical_edges${WS}\\(${WS}stage11b_physical_keys,${WS}config,${WS}input_settings,${WS}current,${WS}stage11c_validation_state${WS}\\)"
            OR NOT STAGE11D_SOURCE MATCHES "inject_stage11d_physical_edges${WS}\\(${WS}stage11c_physical_keys,${WS}config,${WS}input_settings,${WS}current,${WS}stage11d_validation_state${WS}\\)"
            OR NOT STAGE17_SOURCE MATCHES "inject_stage17_physical_edges${WS}\\(${WS}physical_keys,${WS}config,${WS}input_settings,${WS}current,${WS}\\*stage17_validation_state${WS}\\)"
            OR NOT MAP_SOURCE MATCHES "map_host_frame_input${WS}\\(${WS}input_settings,${WS}stage17_physical_keys${WS}\\)"
            OR NOT SOURCE MATCHES "DeathInputGate death_gate =${WS}host_death_input_gate${WS}\\(${WS}death_saving,${WS}death_pending,${WS}frame_input\\.keys,${WS}physical_keys${WS}\\)${WS};"
            OR NOT HOST_GATE_CALL_COUNT EQUAL 2
            OR NOT HOST_GATE_SOURCE MATCHES "if${WS}\\(${WS}pause_open${WS}\\)${WS}\\{${WS}host_gate${WS}=${WS}gate_host_frame${WS}\\(${WS}fixed_step,${WS}pause_latched,${WS}true,${WS}static_cast<double>${WS}\\(${WS}frame_seconds${WS}\\)${WS}\\)${WS};"
            OR NOT HOST_GATE_SOURCE MATCHES "else${WS1}if${WS}\\(${WS}!inventory\\.is_open${WS}\\(${WS}\\)${WS}&&${WS}!inventory_toggled_this_frame${WS}\\)${WS}\\{${WS}host_gate${WS}=${WS}gate_host_frame${WS}\\(${WS}fixed_step,${WS}pause_latched,${WS}false,${WS}static_cast<double>${WS}\\(${WS}frame_seconds${WS}\\)${WS}\\)${WS};"
            OR HOST_CHAIN_SOURCE MATCHES "host_gate\\.forward_gameplay${WS}=${WS}[^=]"
            OR NOT SOURCE MATCHES "const PassiveOverlayInputGate passive_input_gate =${WS}passive_overlay_input_gate${WS}\\(${WS}passive_overlay_open${WS}\\)${WS};"
            OR NOT SOURCE MATCHES "const InventoryInputGate inventory_gate =${WS}inventory_input_gate${WS}\\(${WS}inventory\\.is_open${WS}\\(${WS}\\)${WS}\\|\\|${WS}inventory_toggled_this_frame${WS}\\)${WS};"
            OR NOT SUBMIT_ACTION_CALL_COUNT EQUAL 1
            OR NOT FORWARD_ACTIONS_SOURCE MATCHES "const bool forward_actions =${WS}passive_input_gate\\.forward_actions${WS}&&${WS}inventory_gate\\.forward_actions${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS};"
            OR NOT CONTROLLED_SUBMIT_SOURCE MATCHES "if${WS}\\(${WS}forward_actions${WS}\\)${WS}\\{${WS}const${WS1}SubmittedFrameActions${WS1}submitted_actions${WS}=${WS}submit_frame_actions${WS}\\(${WS}\\*session,${WS}frame_input${WS}\\)${WS};"
            OR NOT FORWARD_DESCENT_ASSIGNMENT_COUNT EQUAL 1
            OR NOT FORWARD_DESCENT_SOURCE MATCHES "const bool forward_descent =${WS}passive_input_gate\\.forward_descent${WS}&&${WS}inventory_gate\\.forward_descent${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS};"
            OR NOT REQUEST_DESCENT_CALL_COUNT EQUAL 1
            OR NOT CONTROLLED_DESCENT_SOURCE MATCHES "if${WS}\\(${WS}forward_descent${WS}&&${WS}frame_input\\.keys\\.e${WS}\\)${WS}\\{${WS}const auto snapshot =${WS}session->snapshot${WS}\\(${WS}\\)${WS};${WS}const bool in_range =${WS}snapshot\\.combat\\.has_value${WS}\\(${WS}\\)${WS}&&${WS}can_prompt_descent${WS}\\(${WS}snapshot,${WS}snapshot\\.combat->player\\.position${WS}\\)${WS};${WS}static_cast<void>${WS}\\(${WS}session->request_descent${WS}\\(${WS}in_range${WS}\\)${WS}\\)${WS};")
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

arpg_physical_input_chain_is_valid("${HOST_ENTRY_SOURCE}" HOST_INPUT_CHAIN_VALID)
if(NOT HOST_INPUT_CHAIN_VALID)
    message(FATAL_ERROR
        "host input must sample once, apply Stage11B, Stage11C, then Stage11D physical edges, and map that snapshot")
endif()

set(STAGE11B_INPUT_CHAIN_REFERENCE [=[
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const PhysicalKeySnapshot stage11b_physical_keys =
    inject_stage11b_physical_edges(
    sampled_physical_keys, config, stage11b_validation_state);
const PhysicalKeySnapshot stage11c_physical_keys = inject_stage11c_physical_edges(
    stage11b_physical_keys, config, input_settings, current,
    stage11c_validation_state);
const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges(
    stage11c_physical_keys, config, input_settings, current,
    stage11d_validation_state);
const PhysicalKeySnapshot stage17_physical_keys =
    inject_stage17_physical_edges(physical_keys, config, input_settings,
        current, *stage17_validation_state);
HostFrameInput frame_input = map_host_frame_input(
    input_settings, stage17_physical_keys);
DeathInputGate death_gate = host_death_input_gate(
    death_saving, death_pending, frame_input.keys, physical_keys);
const bool pause_blocks_gameplay = pause_open || pause_was_open;
HostFrameGateResult host_gate{};
if (pause_open) {
    host_gate = gate_host_frame(fixed_step, pause_latched, true,
        static_cast<double>(frame_seconds));
} else if (!inventory.is_open() && !inventory_toggled_this_frame) {
    host_gate = gate_host_frame(fixed_step, pause_latched, false,
        static_cast<double>(frame_seconds));
}
const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
    passive_overlay_open);
const InventoryInputGate inventory_gate = inventory_input_gate(
    inventory.is_open() || inventory_toggled_this_frame);
const bool forward_actions = passive_input_gate.forward_actions
    && inventory_gate.forward_actions
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay;
const bool forward_descent = passive_input_gate.forward_descent
    && inventory_gate.forward_descent
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay;
if (forward_actions) {
    const SubmittedFrameActions submitted_actions =
        submit_frame_actions(*session, frame_input);
}
if (forward_descent && frame_input.keys.e) {
    const auto snapshot = session->snapshot();
    const bool in_range = snapshot.combat.has_value()
        && can_prompt_descent(snapshot, snapshot.combat->player.position);
    static_cast<void>(session->request_descent(in_range));
}
const combat::MovementInput movement = forward_movement
    ? frame_input.movement : combat::MovementInput{};
]=])
arpg_physical_input_chain_is_valid("${STAGE11B_INPUT_CHAIN_REFERENCE}"
    STAGE11B_INPUT_CHAIN_REFERENCE_VALID)
if(NOT STAGE11B_INPUT_CHAIN_REFERENCE_VALID)
    message(FATAL_ERROR "input chain self-check rejected its reference chain")
endif()
set(INPUT_CHAIN_SPOOF_ACCEPTANCES)
set(COMMENT_ONLY_INPUT_CHAIN "/*${STAGE11B_INPUT_CHAIN_REFERENCE}*/")
set(STRING_ONLY_INPUT_CHAIN
    "R\"arpg(${STAGE11B_INPUT_CHAIN_REFERENCE})arpg\"")
arpg_physical_input_chain_is_valid("${COMMENT_ONLY_INPUT_CHAIN}"
    COMMENT_ONLY_INPUT_CHAIN_VALID)
if(COMMENT_ONLY_INPUT_CHAIN_VALID)
    list(APPEND INPUT_CHAIN_SPOOF_ACCEPTANCES "comment-only-chain")
endif()
arpg_physical_input_chain_is_valid("${STRING_ONLY_INPUT_CHAIN}"
    STRING_ONLY_INPUT_CHAIN_VALID)
if(STRING_ONLY_INPUT_CHAIN_VALID)
    list(APPEND INPUT_CHAIN_SPOOF_ACCEPTANCES "string-only-chain")
endif()
if(INPUT_CHAIN_SPOOF_ACCEPTANCES)
    list(JOIN INPUT_CHAIN_SPOOF_ACCEPTANCES ", " INPUT_CHAIN_SPOOF_NAMES)
    message(FATAL_ERROR
        "input chain self-check accepted source spoofs: ${INPUT_CHAIN_SPOOF_NAMES}")
endif()

set(STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT [=[
const PhysicalKeySnapshot stage11b_physical_keys =
    inject_stage11b_physical_edges(
    sampled_physical_keys, config, stage11b_validation_state);
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const PhysicalKeySnapshot stage11c_physical_keys = inject_stage11c_physical_edges(
    stage11b_physical_keys, config, input_settings, current,
    stage11c_validation_state);
const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges(
    stage11c_physical_keys, config, input_settings, current,
    stage11d_validation_state);
HostFrameInput frame_input = map_host_frame_input(
    input_settings, physical_keys);
]=])
arpg_physical_input_chain_is_valid("${STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT}"
    STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT_VALID)
if(STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT_VALID)
    message(FATAL_ERROR "input chain self-check accepted sample-after-inject mutation")
endif()

set(STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT [=[
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const PhysicalKeySnapshot stage11b_physical_keys =
    inject_stage11b_physical_edges(
    sampled_physical_keys, config, stage11b_validation_state);
HostFrameInput frame_input = map_host_frame_input(
    input_settings, physical_keys);
const PhysicalKeySnapshot stage11c_physical_keys = inject_stage11c_physical_edges(
    stage11b_physical_keys, config, input_settings, current,
    stage11c_validation_state);
const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges(
    stage11c_physical_keys, config, input_settings, current,
    stage11d_validation_state);
]=])
arpg_physical_input_chain_is_valid("${STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT}"
    STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT_VALID)
if(STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT_VALID)
    message(FATAL_ERROR "input chain self-check accepted map-before-inject mutation")
endif()

string(REPLACE
    "stage11b_physical_keys, config, input_settings, current,"
    "sampled_physical_keys, config, input_settings, current,"
    STAGE11C_BYPASS_CHAIN "${STAGE11B_INPUT_CHAIN_REFERENCE}")
arpg_physical_input_chain_is_valid("${STAGE11C_BYPASS_CHAIN}"
    STAGE11C_BYPASS_CHAIN_VALID)
if(STAGE11C_BYPASS_CHAIN_VALID)
    message(FATAL_ERROR "input chain self-check accepted Stage11C bypass mutation")
endif()
string(REPLACE
    "stage11c_physical_keys, config, input_settings, current,"
    "stage11b_physical_keys, config, input_settings, current,"
    STAGE11D_BYPASS_CHAIN "${STAGE11B_INPUT_CHAIN_REFERENCE}")
arpg_physical_input_chain_is_valid("${STAGE11D_BYPASS_CHAIN}"
    STAGE11D_BYPASS_CHAIN_VALID)
if(STAGE11D_BYPASS_CHAIN_VALID)
    message(FATAL_ERROR "input chain self-check accepted Stage11D bypass mutation")
endif()

set(SUBMIT_DECLARATION [=[
                const SubmittedFrameActions submitted_actions =
                    submit_frame_actions(*session, frame_input);
]=])
set(HOST_GATE_DECLARATION
    "            HostFrameGateResult host_gate{};")
string(REPLACE "${SUBMIT_DECLARATION}" ""
    SUBMIT_BEFORE_GATE_CHAIN "${HOST_ENTRY_SOURCE}")
string(REPLACE "${HOST_GATE_DECLARATION}"
    "${SUBMIT_DECLARATION}\n${HOST_GATE_DECLARATION}"
    SUBMIT_BEFORE_GATE_CHAIN "${SUBMIT_BEFORE_GATE_CHAIN}")
string(REPLACE "            if (forward_actions) {"
    "            {"
    UNCONDITIONAL_SUBMIT_CHAIN "${HOST_ENTRY_SOURCE}")
set(ACCEPTED_SUBMIT_MUTATIONS)
arpg_physical_input_chain_is_valid("${SUBMIT_BEFORE_GATE_CHAIN}"
    SUBMIT_BEFORE_GATE_CHAIN_VALID)
if(SUBMIT_BEFORE_GATE_CHAIN_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "submit-before-gate")
endif()
arpg_physical_input_chain_is_valid("${UNCONDITIONAL_SUBMIT_CHAIN}"
    UNCONDITIONAL_SUBMIT_CHAIN_VALID)
if(UNCONDITIONAL_SUBMIT_CHAIN_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "unconditional-submit-bypass")
endif()
if(ACCEPTED_SUBMIT_MUTATIONS)
    list(JOIN ACCEPTED_SUBMIT_MUTATIONS ", " ACCEPTED_SUBMIT_MUTATION_NAMES)
    message(FATAL_ERROR
        "input chain self-check accepted mutations: ${ACCEPTED_SUBMIT_MUTATION_NAMES}")
endif()

set(HOST_GATE_TRUE_ASSIGNMENT [=[
                host_gate = gate_host_frame(fixed_step, pause_latched, true,
                    static_cast<double>(frame_seconds));
]=])
set(HOST_GATE_FALSE_ASSIGNMENT [=[
                host_gate = gate_host_frame(fixed_step, pause_latched, false,
                    static_cast<double>(frame_seconds));
]=])
string(REPLACE "${HOST_GATE_TRUE_ASSIGNMENT}"
    "                host_gate.forward_gameplay = true;"
    HOST_GATE_BYPASS_CHAIN "${HOST_ENTRY_SOURCE}")
string(REPLACE "${HOST_GATE_FALSE_ASSIGNMENT}"
    "                host_gate.forward_gameplay = true;"
    HOST_GATE_BYPASS_CHAIN "${HOST_GATE_BYPASS_CHAIN}")
set(FORWARD_DESCENT_DECLARATION [=[
            const bool forward_descent = passive_input_gate.forward_descent
                && inventory_gate.forward_descent
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay;
]=])
string(REPLACE "${FORWARD_DESCENT_DECLARATION}"
    "            const bool forward_descent = true;"
    UNCONDITIONAL_DESCENT_CHAIN "${HOST_ENTRY_SOURCE}")
if(HOST_GATE_BYPASS_CHAIN STREQUAL HOST_ENTRY_SOURCE
        OR UNCONDITIONAL_DESCENT_CHAIN STREQUAL HOST_ENTRY_SOURCE)
    message(FATAL_ERROR "input gate/descent mutation setup did not modify production source")
endif()
set(ACCEPTED_GATE_DESCENT_MUTATIONS)
arpg_physical_input_chain_is_valid("${HOST_GATE_BYPASS_CHAIN}"
    HOST_GATE_BYPASS_CHAIN_VALID)
if(HOST_GATE_BYPASS_CHAIN_VALID)
    list(APPEND ACCEPTED_GATE_DESCENT_MUTATIONS "host-gate-bypass")
endif()
arpg_physical_input_chain_is_valid("${UNCONDITIONAL_DESCENT_CHAIN}"
    UNCONDITIONAL_DESCENT_CHAIN_VALID)
if(UNCONDITIONAL_DESCENT_CHAIN_VALID)
    list(APPEND ACCEPTED_GATE_DESCENT_MUTATIONS
        "unconditional-descent-bypass")
endif()
if(ACCEPTED_GATE_DESCENT_MUTATIONS)
    list(JOIN ACCEPTED_GATE_DESCENT_MUTATIONS ", "
        ACCEPTED_GATE_DESCENT_MUTATION_NAMES)
    message(FATAL_ERROR
        "input chain self-check accepted mutations: ${ACCEPTED_GATE_DESCENT_MUTATION_NAMES}")
endif()

string(FIND "${HOST_SANITIZED_SOURCE}"
    "[[nodiscard]] PhysicalKeySnapshot inject_stage11b_physical_edges("
    STAGE11B_INJECT_HELPER_START)
string(FIND "${HOST_SANITIZED_SOURCE}"
    "[[nodiscard]] bool stage11b_validation_complete("
    STAGE11B_INJECT_HELPER_END)
if(STAGE11B_INJECT_HELPER_START EQUAL -1 OR STAGE11B_INJECT_HELPER_END EQUAL -1
        OR NOT STAGE11B_INJECT_HELPER_START LESS STAGE11B_INJECT_HELPER_END)
    message(FATAL_ERROR "Stage11B physical-edge injector is missing")
endif()
math(EXPR STAGE11B_INJECT_HELPER_LENGTH
    "${STAGE11B_INJECT_HELPER_END} - ${STAGE11B_INJECT_HELPER_START}")
string(SUBSTRING "${HOST_SANITIZED_SOURCE}" ${STAGE11B_INJECT_HELPER_START}
    ${STAGE11B_INJECT_HELPER_LENGTH} STAGE11B_INJECT_HELPER_SOURCE)
if(NOT STAGE11B_INJECT_HELPER_SOURCE MATCHES
        "config\\.stage11b_validation[ \\t\\n]*==[ \\t\\n]*Stage11BValidationScenario::none"
        OR NOT STAGE11B_INJECT_HELPER_SOURCE MATCHES
        "return[ \\t\\n]+snapshot[ \\t\\n]*;")
    message(FATAL_ERROR
        "normal host input must preserve the single sampled physical snapshot")
endif()

string(FIND "${HOST_ENTRY_SOURCE}" "settings_store.load()" SETTINGS_LOAD_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "make_host_settings_notice(loaded.status)"
    SETTINGS_NOTICE_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "SetConfigFlags(initial_window_flags(committed_settings))"
    INITIAL_FLAGS_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "InitWindow(" INIT_WINDOW_INDEX)
if(SETTINGS_LOAD_INDEX EQUAL -1 OR SETTINGS_NOTICE_INDEX EQUAL -1
        OR INITIAL_FLAGS_INDEX EQUAL -1
        OR INIT_WINDOW_INDEX EQUAL -1
        OR NOT SETTINGS_LOAD_INDEX LESS SETTINGS_NOTICE_INDEX
        OR NOT SETTINGS_LOAD_INDEX LESS INITIAL_FLAGS_INDEX
        OR NOT INITIAL_FLAGS_INDEX LESS INIT_WINDOW_INDEX)
    message(FATAL_ERROR "settings must load before initial flags and InitWindow")
endif()

if(HOST_ENTRY_SOURCE MATCHES
        "frame_input\\.keys\\.e[ \\t\\n]*=[ \\t\\n]*true")
    message(FATAL_ERROR "death continue must not mutate mapped interact input")
endif()

string(FIND "${HOST_ENTRY_SOURCE}" "DeathInputGate death_gate" DEATH_GATE_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "bool escape_consumed = false" ESCAPE_GATE_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "update_pause_menu(" PAUSE_UPDATE_INDEX)
if(DEATH_GATE_INDEX EQUAL -1 OR ESCAPE_GATE_INDEX EQUAL -1
        OR PAUSE_UPDATE_INDEX EQUAL -1
        OR NOT DEATH_GATE_INDEX LESS ESCAPE_GATE_INDEX
        OR NOT ESCAPE_GATE_INDEX LESS PAUSE_UPDATE_INDEX)
    message(FATAL_ERROR "death and overlay Esc gates must precede pause update")
endif()
math(EXPR ESCAPE_GATE_LENGTH "${PAUSE_UPDATE_INDEX} - ${ESCAPE_GATE_INDEX}")
string(SUBSTRING "${HOST_ENTRY_SOURCE}" ${ESCAPE_GATE_INDEX}
    ${ESCAPE_GATE_LENGTH} ESCAPE_GATE_SOURCE)
if(NOT ESCAPE_GATE_SOURCE MATCHES "inventory\\.close[ \\t\\n]*\\("
        OR NOT ESCAPE_GATE_SOURCE MATCHES "passive_overlay_open[ \\t\\n]*=[ \\t\\n]*false"
        OR NOT ESCAPE_GATE_SOURCE MATCHES "escape_consumed[ \\t\\n]*=[ \\t\\n]*true")
    message(FATAL_ERROR "inventory/passive Esc must be consumed before normal pause")
endif()

if(SETTINGS_SETTLE_START EQUAL -1
        OR NOT SETTINGS_SETTLE_START LESS HOST_ENTRY_START)
    message(FATAL_ERROR "pause Apply transaction is missing")
endif()
math(EXPR SETTINGS_SETTLE_LENGTH
    "${HOST_ENTRY_START} - ${SETTINGS_SETTLE_START}")
string(SUBSTRING "${HOST_SANITIZED_SOURCE}" ${SETTINGS_SETTLE_START}
    ${SETTINGS_SETTLE_LENGTH} SETTINGS_SETTLE_SOURCE)
string(FIND "${SETTINGS_SETTLE_SOURCE}" "case PauseCommand::apply:"
    APPLY_CASE_INDEX)
string(FIND "${SETTINGS_SETTLE_SOURCE}" "case PauseCommand::rollback:"
    ROLLBACK_CASE_INDEX)
if(APPLY_CASE_INDEX EQUAL -1 OR ROLLBACK_CASE_INDEX EQUAL -1
        OR NOT APPLY_CASE_INDEX LESS ROLLBACK_CASE_INDEX)
    message(FATAL_ERROR "pause Apply transaction is missing")
endif()
math(EXPR APPLY_CASE_LENGTH "${ROLLBACK_CASE_INDEX} - ${APPLY_CASE_INDEX}")
string(SUBSTRING "${SETTINGS_SETTLE_SOURCE}" ${APPLY_CASE_INDEX}
    ${APPLY_CASE_LENGTH} APPLY_CASE_SOURCE)
string(FIND "${APPLY_CASE_SOURCE}" "apply_live_settings(" APPLY_LIVE_INDEX)
string(FIND "${APPLY_CASE_SOURCE}" "settings_store.save(" APPLY_SAVE_INDEX)
string(FIND "${APPLY_CASE_SOURCE}" "SettingsSaveStatus::committed" APPLY_SUCCESS_INDEX)
string(FIND "${APPLY_CASE_SOURCE}" "pause_menu.committed = saved.settings" APPLY_PUBLISH_INDEX)
string(FIND "${APPLY_CASE_SOURCE}" "rollback_live_settings(" APPLY_ROLLBACK_INDEX)
if(APPLY_LIVE_INDEX EQUAL -1 OR APPLY_SAVE_INDEX EQUAL -1
        OR APPLY_SUCCESS_INDEX EQUAL -1 OR APPLY_PUBLISH_INDEX EQUAL -1
        OR APPLY_ROLLBACK_INDEX EQUAL -1
        OR NOT APPLY_LIVE_INDEX LESS APPLY_SAVE_INDEX
        OR NOT APPLY_SAVE_INDEX LESS APPLY_SUCCESS_INDEX
        OR NOT APPLY_SUCCESS_INDEX LESS APPLY_PUBLISH_INDEX)
    message(FATAL_ERROR "Apply must preview, save, publish only on success, and rollback")
endif()

string(FIND "${HOST_ENTRY_SOURCE}"
    "const bool window_close_requested = WindowShouldClose()"
    WINDOW_CLOSE_SAMPLE_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys()"
    SAMPLED_PHYSICAL_KEYS_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "consume_host_settings_notice("
    NOTICE_CONSUME_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "settle_host_pause_command("
    SETTINGS_SETTLE_CALL_INDEX)
if(WINDOW_CLOSE_SAMPLE_INDEX EQUAL -1 OR SAMPLED_PHYSICAL_KEYS_INDEX EQUAL -1
        OR NOTICE_CONSUME_INDEX EQUAL -1 OR SETTINGS_SETTLE_CALL_INDEX EQUAL -1
        OR NOT WINDOW_CLOSE_SAMPLE_INDEX LESS SAMPLED_PHYSICAL_KEYS_INDEX
        OR NOT PAUSE_UPDATE_INDEX LESS NOTICE_CONSUME_INDEX
        OR NOT NOTICE_CONSUME_INDEX LESS SETTINGS_SETTLE_CALL_INDEX)
    message(FATAL_ERROR
        "window close must be honored only after pause notice/Apply settlement")
endif()

string(FIND "${HOST_ENTRY_SOURCE}" "core::FixedStepFrame frame = host_gate.fixed_step"
    HOST_FRAME_INDEX)
string(FIND "${HOST_ENTRY_SOURCE}" "runtime.fixed_tick(" FIXED_TICK_INDEX)
if(HOST_FRAME_INDEX EQUAL -1 OR FIXED_TICK_INDEX EQUAL -1
        OR NOT HOST_FRAME_INDEX LESS FIXED_TICK_INDEX)
    message(FATAL_ERROR "runtime.fixed_tick must consume only the gated frame steps")
endif()

string(FIND "${HOST_ENTRY_SOURCE}" "pause_menu_renderer.draw(pause_menu)" PAUSE_DRAW_INDEX)
if(PAUSE_DRAW_INDEX EQUAL -1)
    message(FATAL_ERROR "pause overlay draw is missing")
endif()
string(SUBSTRING "${HOST_ENTRY_SOURCE}" ${PAUSE_DRAW_INDEX} -1 PAUSE_DRAW_TAIL)
if(NOT PAUSE_DRAW_TAIL MATCHES "present_frame_and_maybe_capture[ \\t\\n]*\\(")
    message(FATAL_ERROR "paused frames must still present and capture after drawing")
endif()
