if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
set(HOST_VALIDATION_RUNTIME_PATH
    "${RAYLIB_SOURCE_DIR}/host_validation_runtime.cpp")
if(NOT EXISTS "${HOST_VALIDATION_RUNTIME_PATH}")
    message(FATAL_ERROR
        "host validation runtime is required: ${HOST_VALIDATION_RUNTIME_PATH}")
endif()
file(READ "${HOST_VALIDATION_RUNTIME_PATH}" HOST_VALIDATION_RUNTIME_SOURCE)
set(STAGE17_REPORT_SOURCE
    "${RAYLIB_SOURCE_DIR}/host_validation_stage17_report.cpp")
if(NOT EXISTS "${STAGE17_REPORT_SOURCE}")
    message(FATAL_ERROR
        "Stage17 report source is required: ${STAGE17_REPORT_SOURCE}")
endif()
file(READ "${STAGE17_REPORT_SOURCE}" STAGE17_REPORT_SOURCE_TEXT)
file(READ "${RAYLIB_SOURCE_DIR}/host_validation_stage17_runtime.cpp"
    STAGE17_RUNTIME_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/host_validation_stage11b.cpp"
    STAGE11B_RUNTIME_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/combat_renderer.cpp" RENDERER_SOURCE)
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

# Stage17 runtime ownership is unconditional.  Fold phase-2 splices with the
# shared lexer, then remove every conditional region so an inactive exact copy
# cannot satisfy the BASE fingerprint for a weakened active definition.
function(stage17_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" LOGICAL_SOURCE)
    string(LENGTH "${LOGICAL_SOURCE}" SOURCE_LENGTH)
    set(CURSOR 0)
    set(CONDITIONAL_DEPTH 0)
    set(SURFACE "")
    while(CURSOR LESS SOURCE_LENGTH)
        string(SUBSTRING "${LOGICAL_SOURCE}" ${CURSOR} -1 TAIL)
        string(FIND "${TAIL}" "\n" NEWLINE)
        if(NEWLINE EQUAL -1)
            set(LINE "${TAIL}")
            set(LINE_LENGTH -1)
        else()
            math(EXPR LINE_LENGTH "${NEWLINE} + 1")
            string(SUBSTRING "${TAIL}" 0 ${LINE_LENGTH} LINE)
        endif()
        if(LINE MATCHES "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR CONDITIONAL_DEPTH "${CONDITIONAL_DEPTH} + 1")
        elseif(LINE MATCHES "^[ \t]*#[ \t]*endif([ \t\r\n]|$)")
            math(EXPR CONDITIONAL_DEPTH "${CONDITIONAL_DEPTH} - 1")
            if(CONDITIONAL_DEPTH LESS 0)
                message(FATAL_ERROR "Stage17 injector conditional is unbalanced")
            endif()
        elseif(CONDITIONAL_DEPTH EQUAL 0)
            string(APPEND SURFACE "${LINE}")
        endif()
        if(NEWLINE EQUAL -1)
            break()
        endif()
        math(EXPR CURSOR "${CURSOR} + ${LINE_LENGTH}")
    endwhile()
    if(NOT CONDITIONAL_DEPTH EQUAL 0)
        message(FATAL_ERROR "Stage17 injector conditional is unbalanced")
    endif()
    set("${OUT_SURFACE}" "${SURFACE}" PARENT_SCOPE)
endfunction()

arpg_sanitize_cpp_source("${HOST_SOURCE}" HOST_SANITIZED_SOURCE)
arpg_sanitize_cpp_source("${HOST_VALIDATION_RUNTIME_SOURCE}"
    HOST_VALIDATION_RUNTIME_SANITIZED_SOURCE)
arpg_sanitize_cpp_source("${STAGE17_RUNTIME_SOURCE}"
    STAGE17_RUNTIME_SANITIZED_SOURCE)
arpg_sanitize_cpp_source("${STAGE11B_RUNTIME_SOURCE}"
    STAGE11B_RUNTIME_SANITIZED_SOURCE)
stage17_unconditional_cpp_surface("${STAGE17_RUNTIME_SOURCE}"
    STAGE17_RUNTIME_ACTIVE_SOURCE)

if(STAGE17_RUNTIME_ACTIVE_SOURCE MATCHES
        "(^|[^A-Za-z0-9_])(sample_physical_keys|map_host_frame_input|submit_frame_actions|IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV|IsWindowFocused|GetFrameTime)[ \t\r\n]*\\(")
    message(FATAL_ERROR
        "Stage17 runtime bypasses the sampled host input snapshot")
endif()
function(stage17_injector_is_valid SANITIZED_SOURCE OUT_VALID)
    stage17_unconditional_cpp_surface("${SANITIZED_SOURCE}" ACTIVE_SOURCE)
    string(REGEX MATCHALL
        "PhysicalKeySnapshot[ \t\r\n]+inject_stage17_physical_edges[ \t\r\n]*\\("
        ACTIVE_DEFINITIONS "${ACTIVE_SOURCE}")
    list(LENGTH ACTIVE_DEFINITIONS ACTIVE_DEFINITION_COUNT)
    if(NOT ACTIVE_DEFINITION_COUNT EQUAL 1)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${ACTIVE_SOURCE}"
        "PhysicalKeySnapshot inject_stage17_physical_edges("
        FUNCTION_BEGIN FUNCTION_OPEN FUNCTION_END)
    math(EXPR FUNCTION_LENGTH "${FUNCTION_END} - ${FUNCTION_BEGIN} + 1")
    string(SUBSTRING "${ACTIVE_SOURCE}" ${FUNCTION_BEGIN}
        ${FUNCTION_LENGTH} FUNCTION_SOURCE)
    set(WS "[ \t\r\n]*")
    string(FIND "${FUNCTION_SOURCE}" "[&]" CAPTURE_BY_REFERENCE_LAMBDA)
    string(FIND "${FUNCTION_SOURCE}" "[=]" CAPTURE_BY_VALUE_LAMBDA)
    string(FIND "${FUNCTION_SOURCE}" "if (false)" DEAD_BRANCH_SPACED)
    string(FIND "${FUNCTION_SOURCE}" "if(false)" DEAD_BRANCH_COMPACT)
    string(REGEX REPLACE "[ \t\r\n]+" " " FUNCTION_NORMALIZED
        "${FUNCTION_SOURCE}")
    # Task 6A is a verbatim structural move.  Lock the complete executable
    # injector (comments and whitespace excluded), not a subset of tokens that
    # can be left behind after an early return or inside a decoy lambda.
    string(SHA256 FUNCTION_SHA256 "${FUNCTION_NORMALIZED}")
    set(EXPECTED_FUNCTION_SHA256
        "c9638dd62a65d332e6d179509b340845ab02b42cbf31110fd99bfb651aad9ddd")
    string(FIND "${FUNCTION_NORMALIZED}"
        "PhysicalKeySnapshot inject_stage17_physical_edges( PhysicalKeySnapshot snapshot, const RaylibHostConfig& config"
        SIGNATURE_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "active_skill_loadout_layout( config.window_width, config.window_height)"
        LAYOUT_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "stage17_prepare_isolated_storm( snapshot, input_settings, *current.combat, current.ecology == dungeon::checkpoint::DungeonElement::fire, state)"
        STORM_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "stage17_apply_movement( snapshot, input_settings, state.storm_threat_pull_movement)"
        MOVEMENT_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "stage17_press_action(snapshot, input_settings,"
        ACTION_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "stage17_click(snapshot, layout." CLICK_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "snapshot.focus_lost || state.suspend_injection) { return snapshot; }"
        EARLY_RETURN_BINDING)
    string(FIND "${FUNCTION_NORMALIZED}"
        "state.capture_pending = Stage17Capture::restarted; break; } return snapshot; }"
        FINAL_RETURN_BINDING)
    if(SIGNATURE_BINDING EQUAL -1
            OR NOT FUNCTION_SHA256 STREQUAL EXPECTED_FUNCTION_SHA256
            OR LAYOUT_BINDING EQUAL -1
            OR STORM_BINDING EQUAL -1
            OR MOVEMENT_BINDING EQUAL -1
            OR ACTION_BINDING EQUAL -1
            OR CLICK_BINDING EQUAL -1
            OR EARLY_RETURN_BINDING EQUAL -1
            OR FINAL_RETURN_BINDING EQUAL -1
            OR NOT CAPTURE_BY_REFERENCE_LAMBDA EQUAL -1
            OR NOT CAPTURE_BY_VALUE_LAMBDA EQUAL -1
            OR NOT DEAD_BRANCH_SPACED EQUAL -1
            OR NOT DEAD_BRANCH_COMPACT EQUAL -1)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VALID}" TRUE PARENT_SCOPE)
endfunction()

stage17_injector_is_valid("${STAGE17_RUNTIME_SANITIZED_SOURCE}"
    STAGE17_INJECTOR_VALID)
if(NOT STAGE17_INJECTOR_VALID)
    message(FATAL_ERROR
        "Stage17 runtime must augment the shared stable input snapshot")
endif()

set(STAGE17_INJECTOR_MUTATION_ACCEPTANCES)
string(REPLACE
    "const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout("
    "[&] { }; const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout("
    STAGE17_UNCALLED_LAMBDA_MUTATION
    "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
stage17_injector_is_valid("${STAGE17_UNCALLED_LAMBDA_MUTATION}"
    STAGE17_UNCALLED_LAMBDA_VALID)
if(STAGE17_UNCALLED_LAMBDA_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_inject_uncalled_lambda")
endif()
string(REPLACE "config.window_height" "0"
    STAGE17_LAYOUT_ARGUMENT_MUTATION "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
stage17_injector_is_valid("${STAGE17_LAYOUT_ARGUMENT_MUTATION}"
    STAGE17_LAYOUT_ARGUMENT_VALID)
if(STAGE17_LAYOUT_ARGUMENT_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_layout_wrong_argument")
endif()
string(REPLACE "return snapshot;" "return PhysicalKeySnapshot{};"
    STAGE17_SNAPSHOT_RETURN_MUTATION "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
stage17_injector_is_valid("${STAGE17_SNAPSHOT_RETURN_MUTATION}"
    STAGE17_SNAPSHOT_RETURN_VALID)
if(STAGE17_SNAPSHOT_RETURN_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_snapshot_return_bypass")
endif()
set(STAGE17_INJECTOR_ANCHOR
    "const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(")
string(REPLACE "${STAGE17_INJECTOR_ANCHOR}"
    "return snapshot; ${STAGE17_INJECTOR_ANCHOR}"
    STAGE17_EARLY_RETURN_MUTATION "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
stage17_injector_is_valid("${STAGE17_EARLY_RETURN_MUTATION}"
    STAGE17_EARLY_RETURN_VALID)
if(STAGE17_EARLY_RETURN_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_inject_early_return")
endif()
string(REPLACE "${STAGE17_INJECTOR_ANCHOR}"
    "auto decoy = [&snapshot, &input_settings, &state] { return snapshot; }; ${STAGE17_INJECTOR_ANCHOR}"
    STAGE17_CUSTOM_CAPTURE_MUTATION "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
stage17_injector_is_valid("${STAGE17_CUSTOM_CAPTURE_MUTATION}"
    STAGE17_CUSTOM_CAPTURE_VALID)
if(STAGE17_CUSTOM_CAPTURE_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_inject_custom_capture_lambda")
endif()
string(REPLACE "snapshot.active_skill_slots[4] = true;" ""
    STAGE17_SLOT_REMOVAL_MUTATION "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
stage17_injector_is_valid("${STAGE17_SLOT_REMOVAL_MUTATION}"
    STAGE17_SLOT_REMOVAL_VALID)
if(STAGE17_SLOT_REMOVAL_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_inject_slot_removal")
endif()
string(REPLACE "${STAGE17_INJECTOR_ANCHOR}"
    "return snapshot; ${STAGE17_INJECTOR_ANCHOR}"
    STAGE17_WEAK_ACTIVE_SOURCE "${STAGE17_RUNTIME_SANITIZED_SOURCE}")
set(STAGE17_INACTIVE_EXACT_COPY_MUTATION
    "#if 0\n${STAGE17_RUNTIME_SANITIZED_SOURCE}\n#endif\n${STAGE17_WEAK_ACTIVE_SOURCE}")
stage17_injector_is_valid("${STAGE17_INACTIVE_EXACT_COPY_MUTATION}"
    STAGE17_INACTIVE_EXACT_COPY_VALID)
if(STAGE17_INACTIVE_EXACT_COPY_VALID)
    list(APPEND STAGE17_INJECTOR_MUTATION_ACCEPTANCES
        "stage17_inject_inactive_exact_copy")
endif()
if(STAGE17_INJECTOR_MUTATION_ACCEPTANCES)
    list(JOIN STAGE17_INJECTOR_MUTATION_ACCEPTANCES ", "
        STAGE17_INJECTOR_MUTATION_NAMES)
    message(FATAL_ERROR
        "Stage17 input guard accepted mutations: ${STAGE17_INJECTOR_MUTATION_NAMES}")
endif()

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
if(HOST_SANITIZED_SOURCE MATCHES "HostValidationStateAccess"
        OR HOST_VALIDATION_RUNTIME_SANITIZED_SOURCE MATCHES
            "HostValidationStateAccess")
    message(FATAL_ERROR
        "input latency boundary still depends on HostValidationStateAccess")
endif()

function(arpg_physical_input_chain_is_valid_from_sanitized SOURCE OUT_VALID)
    set(WS "[ \t\r\n]*")
    set(WS1 "[ \t\r\n]+")
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys()" SAMPLE_INDEX)
    string(FIND "${SOURCE}" "const bool gameplay_rearm_was_required =" REARM_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot stage17_physical_keys =" FACADE_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot& physical_keys =" CACHED_INDEX)
    string(FIND "${SOURCE}" "gameplay_controls_physically_released(" RELEASED_INDEX)
    string(FIND "${SOURCE}" "runtime.acknowledge_gameplay_rearmed()" ACK_INDEX)
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
    if(SAMPLE_INDEX EQUAL -1 OR REARM_INDEX EQUAL -1 OR FACADE_INDEX EQUAL -1
            OR CACHED_INDEX EQUAL -1 OR RELEASED_INDEX EQUAL -1
            OR ACK_INDEX EQUAL -1
            OR MAP_INDEX EQUAL -1 OR DEATH_GATE_INDEX EQUAL -1 OR PAUSE_BLOCK_INDEX EQUAL -1
            OR HOST_GATE_INDEX EQUAL -1 OR PASSIVE_GATE_INDEX EQUAL -1 OR INVENTORY_GATE_INDEX EQUAL -1
            OR FORWARD_ACTIONS_INDEX EQUAL -1 OR FORWARD_DESCENT_DECL_INDEX EQUAL -1
            OR FORWARD_ACTIONS_IF_INDEX EQUAL -1 OR SUBMIT_ACTIONS_INDEX EQUAL -1
            OR FORWARD_DESCENT_INDEX EQUAL -1 OR REQUEST_DESCENT_INDEX EQUAL -1
            OR MOVEMENT_INPUT_INDEX EQUAL -1
            OR NOT SAMPLE_INDEX LESS REARM_INDEX
            OR NOT REARM_INDEX LESS FACADE_INDEX
            OR NOT FACADE_INDEX LESS CACHED_INDEX
            OR NOT CACHED_INDEX LESS RELEASED_INDEX
            OR NOT RELEASED_INDEX LESS ACK_INDEX
            OR NOT ACK_INDEX LESS MAP_INDEX
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

    math(EXPR FACADE_LENGTH "${CACHED_INDEX} - ${FACADE_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FACADE_INDEX} ${FACADE_LENGTH} FACADE_SOURCE)
    math(EXPR CACHED_LENGTH "${RELEASED_INDEX} - ${CACHED_INDEX}")
    string(SUBSTRING "${SOURCE}" ${CACHED_INDEX} ${CACHED_LENGTH} CACHED_SOURCE)
    math(EXPR RELEASED_LENGTH "${MAP_INDEX} - ${RELEASED_INDEX}")
    string(SUBSTRING "${SOURCE}" ${RELEASED_INDEX} ${RELEASED_LENGTH} RELEASED_SOURCE)
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
    string(REGEX MATCHALL "session->snapshot${WS}\\(${WS}\\)" BY_VALUE_SNAPSHOT_CALLS
        "${CONTROLLED_DESCENT_SOURCE}")
    list(LENGTH BY_VALUE_SNAPSHOT_CALLS BY_VALUE_SNAPSHOT_CALL_COUNT)
    string(REGEX MATCHALL "forward_descent${WS}=" FORWARD_DESCENT_ASSIGNMENTS "${HOST_CHAIN_SOURCE}")
    list(LENGTH FORWARD_DESCENT_ASSIGNMENTS FORWARD_DESCENT_ASSIGNMENT_COUNT)

    string(REGEX MATCHALL
        "validation_runtime->inject_physical_edges${WS}\\("
        FACADE_INJECT_CALLS "${SOURCE}")
    list(LENGTH FACADE_INJECT_CALLS FACADE_INJECT_CALL_COUNT)
    string(REGEX MATCHALL
        "validation_runtime->death_input_snapshot${WS}\\("
        CACHED_INPUT_CALLS "${SOURCE}")
    list(LENGTH CACHED_INPUT_CALLS CACHED_INPUT_CALL_COUNT)

    if(NOT FACADE_SOURCE MATCHES "validation_runtime->inject_physical_edges${WS}\\(${WS}sampled_physical_keys,${WS}input_settings,${WS}current,${WS}gameplay_rearm_was_required${WS}\\)"
            OR NOT CACHED_SOURCE MATCHES "validation_runtime->death_input_snapshot${WS}\\(${WS}\\)"
            OR NOT RELEASED_SOURCE MATCHES "gameplay_controls_physically_released${WS}\\(${WS}stage17_physical_keys${WS}\\)"
            OR NOT RELEASED_SOURCE MATCHES "runtime\\.acknowledge_gameplay_rearmed${WS}\\(${WS}\\)"
            OR NOT MAP_SOURCE MATCHES "map_host_frame_input${WS}\\(${WS}input_settings,${WS}stage17_physical_keys${WS}\\)"
            OR NOT SOURCE MATCHES "DeathInputGate death_gate =${WS}host_death_input_gate${WS}\\(${WS}death_saving,${WS}death_pending,${WS}frame_input\\.keys,${WS}physical_keys${WS}\\)${WS};"
            OR NOT FACADE_INJECT_CALL_COUNT EQUAL 1
            OR NOT CACHED_INPUT_CALL_COUNT EQUAL 1
            OR NOT HOST_GATE_CALL_COUNT EQUAL 2
            OR NOT HOST_GATE_SOURCE MATCHES "if${WS}\\(${WS}pause_open${WS}\\)${WS}\\{${WS}host_gate${WS}=${WS}gate_host_frame${WS}\\(${WS}fixed_step,${WS}pause_latched,${WS}true,${WS}static_cast<double>${WS}\\(${WS}frame_seconds${WS}\\)${WS}\\)${WS};"
            OR NOT HOST_GATE_SOURCE MATCHES "else${WS1}if${WS}\\(${WS}!inventory\\.is_open${WS}\\(${WS}\\)${WS}&&${WS}!inventory_toggled_this_frame${WS}\\)${WS}\\{${WS}host_gate${WS}=${WS}gate_host_frame${WS}\\(${WS}fixed_step,${WS}pause_latched,${WS}false,${WS}static_cast<double>${WS}\\(${WS}frame_seconds${WS}\\)${WS}\\)${WS};"
            OR HOST_CHAIN_SOURCE MATCHES "host_gate\\.forward_gameplay${WS}=${WS}[^=]"
            OR NOT SOURCE MATCHES "const PassiveOverlayInputGate passive_input_gate =${WS}passive_overlay_input_gate${WS}\\(${WS}passive_overlay_open${WS}\\)${WS};"
            OR NOT SOURCE MATCHES "const InventoryInputGate inventory_gate =${WS}inventory_input_gate${WS}\\(${WS}inventory\\.is_open${WS}\\(${WS}\\)${WS}\\|\\|${WS}inventory_toggled_this_frame${WS}\\)${WS};"
            OR NOT SUBMIT_ACTION_CALL_COUNT EQUAL 1
            OR NOT FORWARD_ACTIONS_SOURCE MATCHES "const bool forward_actions =${WS}passive_input_gate\\.forward_actions${WS}&&${WS}inventory_gate\\.forward_actions${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS}&&${WS}gameplay_armed${WS};"
            OR NOT CONTROLLED_SUBMIT_SOURCE MATCHES "if${WS}\\(${WS}forward_actions${WS}\\)${WS}\\{${WS}const${WS1}SubmittedFrameActions${WS1}submitted_actions${WS}=${WS}submit_frame_actions${WS}\\(${WS}\\*session,${WS}frame_input${WS}\\)${WS};"
            OR NOT FORWARD_DESCENT_ASSIGNMENT_COUNT EQUAL 1
            OR NOT FORWARD_DESCENT_SOURCE MATCHES "const bool forward_descent =${WS}passive_input_gate\\.forward_descent${WS}&&${WS}inventory_gate\\.forward_descent${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS}&&${WS}gameplay_armed${WS};"
            OR NOT REQUEST_DESCENT_CALL_COUNT EQUAL 1
            OR NOT BY_VALUE_SNAPSHOT_CALL_COUNT EQUAL 0
            OR NOT CONTROLLED_DESCENT_SOURCE MATCHES "if${WS}\\(${WS}forward_descent${WS}&&${WS}frame_input\\.keys\\.e${WS}\\)${WS}\\{${WS}session->snapshot${WS}\\(${WS}current${WS}\\)${WS};${WS}const bool in_range =${WS}current\\.combat\\.has_value${WS}\\(${WS}\\)${WS}&&${WS}can_prompt_descent${WS}\\(${WS}current,${WS}current\\.combat->player\\.position${WS}\\)${WS};${WS}static_cast<void>${WS}\\(${WS}session->request_descent${WS}\\(${WS}in_range${WS}\\)${WS}\\)${WS};")
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

function(arpg_physical_input_chain_is_valid SOURCE OUT_VALID)
    arpg_sanitize_cpp_source("${SOURCE}" SANITIZED_SOURCE)
    arpg_physical_input_chain_is_valid_from_sanitized(
        "${SANITIZED_SOURCE}" CHAIN_VALID)
    set(${OUT_VALID} ${CHAIN_VALID} PARENT_SCOPE)
endfunction()

function(arpg_latency_cpp_prefix_has_only_namespace_scopes
        SOURCE PREFIX_END OUT_VALID)
    if(PREFIX_END EQUAL 0)
        set(${OUT_VALID} TRUE PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" 0 ${PREFIX_END} PREFIX_SOURCE)
    string(LENGTH "${PREFIX_SOURCE}" PREFIX_LENGTH)
    math(EXPR PREFIX_LAST "${PREFIX_LENGTH} - 1")
    set(SCOPE_KINDS)
    foreach(CHARACTER_INDEX RANGE 0 ${PREFIX_LAST})
        string(SUBSTRING "${PREFIX_SOURCE}" ${CHARACTER_INDEX} 1 CHARACTER)
        if(CHARACTER STREQUAL "{")
            string(SUBSTRING "${PREFIX_SOURCE}" 0 ${CHARACTER_INDEX}
                BEFORE_OPEN)
            if(BEFORE_OPEN MATCHES
                    "namespace([ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*([ \t]*::[ \t]*[A-Za-z_][A-Za-z0-9_]*)*)?[ \t\r\n]*$")
                list(APPEND SCOPE_KINDS namespace)
            else()
                list(APPEND SCOPE_KINDS non_namespace)
            endif()
        elseif(CHARACTER STREQUAL "}")
            list(LENGTH SCOPE_KINDS SCOPE_DEPTH)
            if(SCOPE_DEPTH EQUAL 0)
                set(${OUT_VALID} FALSE PARENT_SCOPE)
                return()
            endif()
            list(POP_BACK SCOPE_KINDS)
        endif()
    endforeach()
    foreach(SCOPE_KIND IN LISTS SCOPE_KINDS)
        if(NOT SCOPE_KIND STREQUAL namespace)
            set(${OUT_VALID} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

function(arpg_latency_cpp_direct_function_surface SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" SOURCE_LENGTH)
    math(EXPR SOURCE_LAST "${SOURCE_LENGTH} - 1")
    set(DIRECT_SOURCE "")
    set(BRACE_DEPTH 0)
    foreach(CHARACTER_INDEX RANGE 0 ${SOURCE_LAST})
        string(SUBSTRING "${SOURCE}" ${CHARACTER_INDEX} 1 CHARACTER)
        if(CHARACTER STREQUAL "{")
            math(EXPR BRACE_DEPTH "${BRACE_DEPTH} + 1")
            if(BRACE_DEPTH LESS_EQUAL 1)
                string(APPEND DIRECT_SOURCE "${CHARACTER}")
            else()
                string(APPEND DIRECT_SOURCE " ")
            endif()
        elseif(CHARACTER STREQUAL "}")
            if(BRACE_DEPTH LESS_EQUAL 1)
                string(APPEND DIRECT_SOURCE "${CHARACTER}")
            else()
                string(APPEND DIRECT_SOURCE " ")
            endif()
            math(EXPR BRACE_DEPTH "${BRACE_DEPTH} - 1")
        elseif(BRACE_DEPTH LESS_EQUAL 1)
            string(APPEND DIRECT_SOURCE "${CHARACTER}")
        elseif(CHARACTER STREQUAL "\n")
            string(APPEND DIRECT_SOURCE "\n")
        else()
            string(APPEND DIRECT_SOURCE " ")
        endif()
    endforeach()
    set(${OUT_SOURCE} "${DIRECT_SOURCE}" PARENT_SCOPE)
endfunction()

function(arpg_validation_runtime_input_chain_is_valid SOURCE OUT_VALID)
    # Sanitize before locating the signature. This is intentionally one
    # raw-aware pass over production, while the mutation fixtures below are
    # only function-sized strings.
    stage17_unconditional_cpp_surface("${SOURCE}" ACTIVE_SOURCE)
    string(REGEX MATCHALL
        "PhysicalKeySnapshot[ \t\r\n]+HostValidationRuntime::inject_physical_edges[ \t\r\n]*\\("
        FUNCTION_SIGNATURES "${ACTIVE_SOURCE}")
    list(LENGTH FUNCTION_SIGNATURES FUNCTION_SIGNATURE_COUNT)
    if(NOT FUNCTION_SIGNATURE_COUNT EQUAL 1)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    list(GET FUNCTION_SIGNATURES 0 FUNCTION_SIGNATURE)
    string(FIND "${ACTIVE_SOURCE}" "${FUNCTION_SIGNATURE}" FUNCTION_BEGIN)
    string(SUBSTRING "${ACTIVE_SOURCE}" ${FUNCTION_BEGIN} -1 FUNCTION_TAIL)
    string(FIND "${FUNCTION_TAIL}" "{" FUNCTION_OPEN_RELATIVE)
    string(FIND "${FUNCTION_TAIL}" ";" DECLARATION_END_RELATIVE)
    if(FUNCTION_OPEN_RELATIVE EQUAL -1
            OR (NOT DECLARATION_END_RELATIVE EQUAL -1
                AND DECLARATION_END_RELATIVE LESS FUNCTION_OPEN_RELATIVE))
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    arpg_latency_cpp_prefix_has_only_namespace_scopes(
        "${ACTIVE_SOURCE}" ${FUNCTION_BEGIN} FUNCTION_SCOPE_VALID)
    if(NOT FUNCTION_SCOPE_VALID)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    evidence_find_cpp_function_bounds_in_sanitized(
        "${ACTIVE_SOURCE}" "${FUNCTION_SIGNATURE}"
        FUNCTION_BEGIN FUNCTION_OPEN FUNCTION_END)
    math(EXPR FUNCTION_LENGTH "${FUNCTION_END} - ${FUNCTION_BEGIN} + 1")
    string(SUBSTRING "${ACTIVE_SOURCE}"
        ${FUNCTION_BEGIN} ${FUNCTION_LENGTH} FUNCTION_SOURCE)
    arpg_latency_cpp_direct_function_surface(
        "${FUNCTION_SOURCE}" DIRECT_FUNCTION_SOURCE)
    set(WS "[ \t\r\n]*")
    set(WS1 "[ \t\r\n]+")
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "const PhysicalKeySnapshot stage11b_physical_keys =" STAGE11B_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "const PhysicalKeySnapshot stage11c_physical_keys =" STAGE11C_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "const PhysicalKeySnapshot stage11d_physical_keys =" STAGE11D_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "impl_->death_input_snapshot = stage11d_physical_keys" CACHE_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "impl_->states.stage17.suspend_injection = gameplay_rearm_required"
        SUSPEND_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "return host_validation::inject_stage17_physical_edges("
        STAGE17_INDEX)
    if(STAGE11B_INDEX EQUAL -1 OR STAGE11C_INDEX EQUAL -1
            OR STAGE11D_INDEX EQUAL -1 OR CACHE_INDEX EQUAL -1
            OR SUSPEND_INDEX EQUAL -1 OR STAGE17_INDEX EQUAL -1
            OR NOT STAGE11B_INDEX LESS STAGE11C_INDEX
            OR NOT STAGE11C_INDEX LESS STAGE11D_INDEX
            OR NOT STAGE11D_INDEX LESS CACHE_INDEX
            OR NOT CACHE_INDEX LESS SUSPEND_INDEX
            OR NOT SUSPEND_INDEX LESS STAGE17_INDEX)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])return${WS1}"
        ALL_RETURNS "${FUNCTION_SOURCE}")
    list(LENGTH ALL_RETURNS ALL_RETURN_COUNT)
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])return${WS1}"
        DIRECT_RETURNS "${DIRECT_FUNCTION_SOURCE}")
    list(LENGTH DIRECT_RETURNS DIRECT_RETURN_COUNT)
    if(NOT ALL_RETURN_COUNT EQUAL 1 OR NOT DIRECT_RETURN_COUNT EQUAL 1
            OR FUNCTION_SOURCE MATCHES
                "\\][ \t\r\n]*(\\([^{};]*\\))?[ \t\r\n]*(mutable[ \t\r\n]*)?(noexcept([ \t\r\n]*\\([^{};]*\\))?[ \t\r\n]*)?(->[^{};]*)?[ \t\r\n]*\\{"
            OR FUNCTION_SOURCE MATCHES
                "(if|while)[ \t\r\n]*(constexpr[ \t\r\n]*)?\\([ \t\r\n]*(false|0[uUlL]*|![ \t\r\n]*true)[ \t\r\n]*\\)")
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    foreach(INJECTOR IN ITEMS
            inject_stage11b_physical_edges inject_stage11c_physical_edges
            inject_stage11d_physical_edges inject_stage17_physical_edges)
        string(REGEX MATCHALL "${INJECTOR}${WS}\\("
            INJECTOR_CALLS "${FUNCTION_SOURCE}")
        list(LENGTH INJECTOR_CALLS INJECTOR_CALL_COUNT)
        string(REGEX MATCHALL "${INJECTOR}${WS}\\("
            DIRECT_INJECTOR_CALLS "${DIRECT_FUNCTION_SOURCE}")
        list(LENGTH DIRECT_INJECTOR_CALLS DIRECT_INJECTOR_CALL_COUNT)
        if(NOT INJECTOR_CALL_COUNT EQUAL 1
                OR NOT DIRECT_INJECTOR_CALL_COUNT EQUAL 1)
            set(${OUT_VALID} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    foreach(ASSIGNMENT_PATTERN IN ITEMS
            "stage11b_physical_keys${WS}="
            "stage11c_physical_keys${WS}="
            "stage11d_physical_keys${WS}="
            "impl_->death_input_snapshot${WS}="
            "impl_->states\\.stage17\\.suspend_injection${WS}=")
        string(REGEX MATCHALL "${ASSIGNMENT_PATTERN}"
            ASSIGNMENTS "${FUNCTION_SOURCE}")
        list(LENGTH ASSIGNMENTS ASSIGNMENT_COUNT)
        string(REGEX MATCHALL "${ASSIGNMENT_PATTERN}"
            DIRECT_ASSIGNMENTS "${DIRECT_FUNCTION_SOURCE}")
        list(LENGTH DIRECT_ASSIGNMENTS DIRECT_ASSIGNMENT_COUNT)
        if(NOT ASSIGNMENT_COUNT EQUAL 1
                OR NOT DIRECT_ASSIGNMENT_COUNT EQUAL 1)
            set(${OUT_VALID} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    if(NOT DIRECT_FUNCTION_SOURCE MATCHES
            "PhysicalKeySnapshot${WS1}HostValidationRuntime::inject_physical_edges${WS}\\(${WS}PhysicalKeySnapshot${WS1}snapshot,${WS}const${WS1}settings::SettingsData&${WS1}input_settings,${WS}const${WS1}dungeon::DungeonSnapshot&${WS1}dungeon_snapshot,${WS}bool${WS1}gameplay_rearm_required${WS}\\)${WS}noexcept${WS}\\{"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
            "const${WS1}PhysicalKeySnapshot${WS1}stage11b_physical_keys${WS}=${WS}host_validation::inject_stage11b_physical_edges${WS}\\(${WS}snapshot,${WS}\\*impl_->config,${WS}impl_->states\\.stage11b${WS}\\)${WS};"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
            "const${WS1}PhysicalKeySnapshot${WS1}stage11c_physical_keys${WS}=${WS}host_validation::inject_stage11c_physical_edges${WS}\\(${WS}stage11b_physical_keys,${WS}\\*impl_->config,${WS}input_settings,${WS}dungeon_snapshot,${WS}impl_->states\\.stage11c${WS}\\)${WS};"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
            "const${WS1}PhysicalKeySnapshot${WS1}stage11d_physical_keys${WS}=${WS}host_validation::inject_stage11d_physical_edges${WS}\\(${WS}stage11c_physical_keys,${WS}\\*impl_->config,${WS}input_settings,${WS}dungeon_snapshot,${WS}impl_->states\\.stage11d${WS}\\)${WS};"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
            "impl_->death_input_snapshot${WS}=${WS}stage11d_physical_keys${WS};"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
            "impl_->states\\.stage17\\.suspend_injection${WS}=${WS}gameplay_rearm_required${WS};"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
            "return${WS1}host_validation::inject_stage17_physical_edges${WS}\\(${WS}stage11d_physical_keys,${WS}\\*impl_->config,${WS}input_settings,${WS}dungeon_snapshot,${WS}impl_->states\\.stage17${WS}\\)${WS};${WS}\\}${WS}$")
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

arpg_physical_input_chain_is_valid_from_sanitized(
    "${HOST_ENTRY_SOURCE}" HOST_INPUT_CHAIN_VALID)
if(NOT HOST_INPUT_CHAIN_VALID)
    message(FATAL_ERROR
        "host input must sample once, apply Stage11B, Stage11C, then Stage11D physical edges, and map that snapshot")
endif()
arpg_validation_runtime_input_chain_is_valid(
    "${HOST_VALIDATION_RUNTIME_SOURCE}"
    VALIDATION_RUNTIME_INPUT_CHAIN_VALID)
if(NOT VALIDATION_RUNTIME_INPUT_CHAIN_VALID)
    message(FATAL_ERROR
        "host validation runtime must apply Stage11B, Stage11C, Stage11D, then Stage17 physical edges")
endif()

set(STAGE11B_INPUT_CHAIN_REFERENCE [=[
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const bool gameplay_rearm_was_required =
    runtime.gameplay_rearm_required();
const PhysicalKeySnapshot stage17_physical_keys =
    validation_runtime->inject_physical_edges(
        sampled_physical_keys, input_settings, current,
        gameplay_rearm_was_required);
const PhysicalKeySnapshot& physical_keys =
    validation_runtime->death_input_snapshot();
if (gameplay_rearm_was_required
        && gameplay_controls_physically_released(stage17_physical_keys)) {
    runtime.acknowledge_gameplay_rearmed();
}
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
    && host_gate.forward_gameplay && !pause_blocks_gameplay
    && gameplay_armed;
const bool forward_descent = passive_input_gate.forward_descent
    && inventory_gate.forward_descent
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay
    && gameplay_armed;
if (forward_actions) {
    const SubmittedFrameActions submitted_actions =
        submit_frame_actions(*session, frame_input);
}
if (forward_descent && frame_input.keys.e) {
    session->snapshot(current);
    const bool in_range = current.combat.has_value()
        && can_prompt_descent(current, current.combat->player.position);
    static_cast<void>(session->request_descent(in_range));
}
const combat::MovementInput movement = forward_movement
    ? frame_input.movement : combat::MovementInput{};
]=])
set(VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE [=[
PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges(
    PhysicalKeySnapshot snapshot, const settings::SettingsData& input_settings,
    const dungeon::DungeonSnapshot& dungeon_snapshot,
    bool gameplay_rearm_required) noexcept {
    const PhysicalKeySnapshot stage11b_physical_keys =
        host_validation::inject_stage11b_physical_edges(
            snapshot, *impl_->config, impl_->states.stage11b);
    const PhysicalKeySnapshot stage11c_physical_keys =
        host_validation::inject_stage11c_physical_edges(
            stage11b_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11c);
    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11d);
    impl_->death_input_snapshot = stage11d_physical_keys;
    impl_->states.stage17.suspend_injection = gameplay_rearm_required;
    return host_validation::inject_stage17_physical_edges(
        stage11d_physical_keys, *impl_->config, input_settings,
        dungeon_snapshot, impl_->states.stage17);
}
]=])
arpg_physical_input_chain_is_valid("${STAGE11B_INPUT_CHAIN_REFERENCE}"
    STAGE11B_INPUT_CHAIN_REFERENCE_VALID)
if(NOT STAGE11B_INPUT_CHAIN_REFERENCE_VALID)
    message(FATAL_ERROR "input chain self-check rejected its reference chain")
endif()
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}"
    VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE_VALID)
if(NOT VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE_VALID)
    message(FATAL_ERROR
        "input chain self-check rejected its validation-runtime reference")
endif()
set(PREALLOCATED_DESCENT_CAPTURE [=[
    session->snapshot(current);
    const bool in_range = current.combat.has_value()
        && can_prompt_descent(current, current.combat->player.position);
]=])
set(BY_VALUE_DESCENT_CAPTURE [=[
    const auto snapshot = session->snapshot();
    const bool in_range = snapshot.combat.has_value()
        && can_prompt_descent(snapshot, snapshot.combat->player.position);
]=])
string(REPLACE "${PREALLOCATED_DESCENT_CAPTURE}"
    "${BY_VALUE_DESCENT_CAPTURE}"
    BY_VALUE_SNAPSHOT_CHAIN "${STAGE11B_INPUT_CHAIN_REFERENCE}")
if(BY_VALUE_SNAPSHOT_CHAIN STREQUAL STAGE11B_INPUT_CHAIN_REFERENCE)
    message(FATAL_ERROR "by-value snapshot mutation setup did not modify reference chain")
endif()
arpg_physical_input_chain_is_valid("${BY_VALUE_SNAPSHOT_CHAIN}"
    BY_VALUE_SNAPSHOT_CHAIN_VALID)
if(BY_VALUE_SNAPSHOT_CHAIN_VALID)
    message(FATAL_ERROR "input chain self-check accepted by-value descent snapshot")
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

set(_SAMPLE_DECLARATION
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();")
string(REPLACE "${_SAMPLE_DECLARATION}" ""
    STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT
    "${STAGE11B_INPUT_CHAIN_REFERENCE}")
string(REPLACE
    "const PhysicalKeySnapshot& physical_keys ="
    "const PhysicalKeySnapshot& physical_keys =\n${_SAMPLE_DECLARATION}"
    STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT
    "${STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT}")
arpg_physical_input_chain_is_valid("${STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT}"
    STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT_VALID)
if(STAGE11B_INPUT_CHAIN_SAMPLE_AFTER_INJECT_VALID)
    message(FATAL_ERROR "input chain self-check accepted sample-after-inject mutation")
endif()

set(_MAP_DECLARATION [=[HostFrameInput frame_input = map_host_frame_input(
    input_settings, stage17_physical_keys);
]=])
string(REPLACE "${_MAP_DECLARATION}" ""
    STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT
    "${STAGE11B_INPUT_CHAIN_REFERENCE}")
string(REPLACE
    "const PhysicalKeySnapshot stage17_physical_keys ="
    "${_MAP_DECLARATION}const PhysicalKeySnapshot stage17_physical_keys ="
    STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT
    "${STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT}")
arpg_physical_input_chain_is_valid("${STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT}"
    STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT_VALID)
if(STAGE11B_INPUT_CHAIN_MAP_BEFORE_INJECT_VALID)
    message(FATAL_ERROR "input chain self-check accepted map-before-inject mutation")
endif()

string(REPLACE
    "stage11b_physical_keys, *impl_->config, input_settings,"
    "snapshot, *impl_->config, input_settings,"
    STAGE11C_BYPASS_CHAIN "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
arpg_validation_runtime_input_chain_is_valid("${STAGE11C_BYPASS_CHAIN}"
    STAGE11C_BYPASS_CHAIN_VALID)
if(STAGE11C_BYPASS_CHAIN_VALID)
    message(FATAL_ERROR "input chain self-check accepted Stage11C bypass mutation")
endif()
string(REPLACE
    "stage11c_physical_keys, *impl_->config, input_settings,"
    "stage11b_physical_keys, *impl_->config, input_settings,"
    STAGE11D_BYPASS_CHAIN "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
arpg_validation_runtime_input_chain_is_valid("${STAGE11D_BYPASS_CHAIN}"
    STAGE11D_BYPASS_CHAIN_VALID)
if(STAGE11D_BYPASS_CHAIN_VALID)
    message(FATAL_ERROR "input chain self-check accepted Stage11D bypass mutation")
endif()

set(VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES)
set(VALIDATION_RUNTIME_FIRST_DECLARATION
    "    const PhysicalKeySnapshot stage11b_physical_keys =")
set(VALIDATION_RUNTIME_STAGE11B_DECLARATION [=[    const PhysicalKeySnapshot stage11b_physical_keys =
        host_validation::inject_stage11b_physical_edges(
            snapshot, *impl_->config, impl_->states.stage11b);
]=])
set(VALIDATION_RUNTIME_STAGE11C_DECLARATION [=[    const PhysicalKeySnapshot stage11c_physical_keys =
        host_validation::inject_stage11c_physical_edges(
            stage11b_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11c);
]=])
set(VALIDATION_RUNTIME_STAGE11D_DECLARATION [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11d);
]=])
set(VALIDATION_RUNTIME_STAGE11B_DISCARDED_RETURN [=[    const PhysicalKeySnapshot stage11b_physical_keys = snapshot;
    static_cast<void>(host_validation::inject_stage11b_physical_edges(
        snapshot, *impl_->config, impl_->states.stage11b));
]=])
set(VALIDATION_RUNTIME_STAGE11C_DISCARDED_RETURN [=[    const PhysicalKeySnapshot stage11c_physical_keys =
        stage11b_physical_keys;
    static_cast<void>(host_validation::inject_stage11c_physical_edges(
        stage11b_physical_keys, *impl_->config, input_settings,
        dungeon_snapshot, impl_->states.stage11c));
]=])
set(VALIDATION_RUNTIME_STAGE11D_DISCARDED_RETURN [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        stage11c_physical_keys;
    static_cast<void>(host_validation::inject_stage11d_physical_edges(
        stage11c_physical_keys, *impl_->config, input_settings,
        dungeon_snapshot, impl_->states.stage11d));
]=])
foreach(STAGE IN ITEMS 11B 11C 11D)
    string(REPLACE
        "${VALIDATION_RUNTIME_STAGE${STAGE}_DECLARATION}"
        "${VALIDATION_RUNTIME_STAGE${STAGE}_DISCARDED_RETURN}"
        VALIDATION_RUNTIME_STAGE${STAGE}_DISCARDED_RETURN_MUTATION
        "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
    if(VALIDATION_RUNTIME_STAGE${STAGE}_DISCARDED_RETURN_MUTATION STREQUAL
            VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE)
        message(FATAL_ERROR
            "validation-runtime Stage${STAGE} return-binding mutation setup did not modify reference")
    endif()
    arpg_validation_runtime_input_chain_is_valid(
        "${VALIDATION_RUNTIME_STAGE${STAGE}_DISCARDED_RETURN_MUTATION}"
        VALIDATION_RUNTIME_STAGE${STAGE}_DISCARDED_RETURN_VALID)
    if(VALIDATION_RUNTIME_STAGE${STAGE}_DISCARDED_RETURN_VALID)
        list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES
            "stage${STAGE}-discarded-return")
    endif()
endforeach()

string(REPLACE "${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    "    return snapshot;\n${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    VALIDATION_RUNTIME_EARLY_RETURN_MUTATION
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
if(VALIDATION_RUNTIME_EARLY_RETURN_MUTATION STREQUAL
        VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE)
    message(FATAL_ERROR
        "validation-runtime early-return mutation setup did not modify reference")
endif()
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_EARLY_RETURN_MUTATION}"
    VALIDATION_RUNTIME_EARLY_RETURN_VALID)
if(VALIDATION_RUNTIME_EARLY_RETURN_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES early-return)
endif()

set(VALIDATION_RUNTIME_INACTIVE_EXACT_COPY_MUTATION
    "#if 0\n${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}#endif\n${VALIDATION_RUNTIME_EARLY_RETURN_MUTATION}")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_INACTIVE_EXACT_COPY_MUTATION}"
    VALIDATION_RUNTIME_INACTIVE_EXACT_COPY_VALID)
if(VALIDATION_RUNTIME_INACTIVE_EXACT_COPY_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES
        inactive-exact-copy)
endif()

set(VALIDATION_RUNTIME_RAW_DECOY_PREFIX [=[const char* runtime_chain_decoy = R"arpg(
legacy scanner sees an internal " quote and a )" non-closing pair
]=])
set(VALIDATION_RUNTIME_RAW_DECOY_SUFFIX [=[)arpg";
]=])
string(CONCAT VALIDATION_RUNTIME_RAW_STRING_MUTATION
    "${VALIDATION_RUNTIME_RAW_DECOY_PREFIX}"
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}"
    "${VALIDATION_RUNTIME_RAW_DECOY_SUFFIX}"
    "${VALIDATION_RUNTIME_EARLY_RETURN_MUTATION}")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_RAW_STRING_MUTATION}"
    VALIDATION_RUNTIME_RAW_STRING_VALID)
if(VALIDATION_RUNTIME_RAW_STRING_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES
        raw-string-internal-quote)
endif()

set(VALIDATION_RUNTIME_DUPLICATE_CALL [=[    static_cast<void>(
        host_validation::inject_stage11b_physical_edges(
            snapshot, *impl_->config, impl_->states.stage11b));
]=])
string(REPLACE "${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    "${VALIDATION_RUNTIME_DUPLICATE_CALL}${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    VALIDATION_RUNTIME_DUPLICATE_MUTATION
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_DUPLICATE_MUTATION}"
    VALIDATION_RUNTIME_DUPLICATE_VALID)
if(VALIDATION_RUNTIME_DUPLICATE_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES duplicate-call)
endif()

set(VALIDATION_RUNTIME_DUPLICATE_DEFINITION_MUTATION
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}\n${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_DUPLICATE_DEFINITION_MUTATION}"
    VALIDATION_RUNTIME_DUPLICATE_DEFINITION_VALID)
if(VALIDATION_RUNTIME_DUPLICATE_DEFINITION_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES
        duplicate-definition)
endif()

string(REPLACE "${VALIDATION_RUNTIME_STAGE11C_DECLARATION}"
    "    {\n${VALIDATION_RUNTIME_STAGE11C_DECLARATION}    }\n"
    VALIDATION_RUNTIME_CROSS_SCOPE_MUTATION
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
if(VALIDATION_RUNTIME_CROSS_SCOPE_MUTATION STREQUAL
        VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE)
    message(FATAL_ERROR
        "validation-runtime cross-scope mutation setup did not modify reference")
endif()
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_CROSS_SCOPE_MUTATION}"
    VALIDATION_RUNTIME_CROSS_SCOPE_VALID)
if(VALIDATION_RUNTIME_CROSS_SCOPE_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES cross-scope)
endif()

string(REPLACE "${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    "    auto input_decoy = [&] { };\n${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    VALIDATION_RUNTIME_LAMBDA_MUTATION
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_LAMBDA_MUTATION}"
    VALIDATION_RUNTIME_LAMBDA_VALID)
if(VALIDATION_RUNTIME_LAMBDA_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES lambda)
endif()

string(REPLACE "${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    "    if (false)\n${VALIDATION_RUNTIME_FIRST_DECLARATION}"
    VALIDATION_RUNTIME_DEAD_BRANCH_MUTATION
    "${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_DEAD_BRANCH_MUTATION}"
    VALIDATION_RUNTIME_DEAD_BRANCH_VALID)
if(VALIDATION_RUNTIME_DEAD_BRANCH_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES dead-branch)
endif()

set(VALIDATION_RUNTIME_NESTED_FUNCTION_MUTATION
    "void runtime_chain_decoy_owner() {\n${VALIDATION_RUNTIME_INPUT_CHAIN_REFERENCE}}\n")
arpg_validation_runtime_input_chain_is_valid(
    "${VALIDATION_RUNTIME_NESTED_FUNCTION_MUTATION}"
    VALIDATION_RUNTIME_NESTED_FUNCTION_VALID)
if(VALIDATION_RUNTIME_NESTED_FUNCTION_VALID)
    list(APPEND VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES
        nested-function-definition)
endif()

if(VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES)
    list(JOIN VALIDATION_RUNTIME_INPUT_MUTATION_ACCEPTANCES ", "
        VALIDATION_RUNTIME_INPUT_MUTATION_NAMES)
    message(FATAL_ERROR
        "input chain self-check accepted validation-runtime mutations: ${VALIDATION_RUNTIME_INPUT_MUTATION_NAMES}")
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
arpg_physical_input_chain_is_valid_from_sanitized("${SUBMIT_BEFORE_GATE_CHAIN}"
    SUBMIT_BEFORE_GATE_CHAIN_VALID)
if(SUBMIT_BEFORE_GATE_CHAIN_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "submit-before-gate")
endif()
arpg_physical_input_chain_is_valid_from_sanitized("${UNCONDITIONAL_SUBMIT_CHAIN}"
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
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
]=])
string(REPLACE "${FORWARD_DESCENT_DECLARATION}"
    "            const bool forward_descent = true;"
    UNCONDITIONAL_DESCENT_CHAIN "${HOST_ENTRY_SOURCE}")
if(HOST_GATE_BYPASS_CHAIN STREQUAL HOST_ENTRY_SOURCE
        OR UNCONDITIONAL_DESCENT_CHAIN STREQUAL HOST_ENTRY_SOURCE)
    message(FATAL_ERROR "input gate/descent mutation setup did not modify production source")
endif()
set(ACCEPTED_GATE_DESCENT_MUTATIONS)
arpg_physical_input_chain_is_valid_from_sanitized("${HOST_GATE_BYPASS_CHAIN}"
    HOST_GATE_BYPASS_CHAIN_VALID)
if(HOST_GATE_BYPASS_CHAIN_VALID)
    list(APPEND ACCEPTED_GATE_DESCENT_MUTATIONS "host-gate-bypass")
endif()
arpg_physical_input_chain_is_valid_from_sanitized("${UNCONDITIONAL_DESCENT_CHAIN}"
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

string(FIND "${STAGE11B_RUNTIME_SANITIZED_SOURCE}"
    "PhysicalKeySnapshot inject_stage11b_physical_edges("
    STAGE11B_INJECT_HELPER_START)
string(FIND "${STAGE11B_RUNTIME_SANITIZED_SOURCE}"
    "bool stage11b_validation_complete("
    STAGE11B_INJECT_HELPER_END)
if(STAGE11B_INJECT_HELPER_START EQUAL -1 OR STAGE11B_INJECT_HELPER_END EQUAL -1
        OR NOT STAGE11B_INJECT_HELPER_START LESS STAGE11B_INJECT_HELPER_END)
    message(FATAL_ERROR "Stage11B physical-edge injector is missing")
endif()
math(EXPR STAGE11B_INJECT_HELPER_LENGTH
    "${STAGE11B_INJECT_HELPER_END} - ${STAGE11B_INJECT_HELPER_START}")
string(SUBSTRING "${STAGE11B_RUNTIME_SANITIZED_SOURCE}" ${STAGE11B_INJECT_HELPER_START}
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

string(FIND "${HOST_ENTRY_SOURCE}"
    "pause_menu_renderer.draw(pause_menu, renderer.material_pack())"
    PAUSE_DRAW_INDEX)
if(PAUSE_DRAW_INDEX EQUAL -1)
    message(FATAL_ERROR "pause overlay draw is missing")
endif()
string(SUBSTRING "${HOST_ENTRY_SOURCE}" ${PAUSE_DRAW_INDEX} -1 PAUSE_DRAW_TAIL)
if(NOT PAUSE_DRAW_TAIL MATCHES "present_frame_and_maybe_capture[ \\t\\n]*\\(")
    message(FATAL_ERROR "paused frames must still present and capture after drawing")
endif()
