if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
else()
    set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
endif()
if(DEFINED STAGE_OVERRIDE)
    set(_stage "${STAGE_OVERRIDE}")
else()
    set(_stage "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.cpp")
endif()
set(_stage_header "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.hpp")
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_pause_renderer "${SOURCE_ROOT}/src/platform/raylib/pause_menu_renderer.cpp")
set(_font_source "${SOURCE_ROOT}/src/platform/raylib/death_overlay_font.cpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
else()
    set(_formal "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp")
endif()
foreach(_required IN ITEMS "${_host}" "${_stage}" "${_stage_header}" "${_header}" "${_formal}" "${_pause_renderer}" "${_font_source}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11B evidence target is missing: ${_required}")
    endif()
endforeach()

file(READ "${_host}" _host_text)
file(READ "${_stage}" _stage_text)
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_header}" _header_text)
file(READ "${_formal}" _formal_text)
file(READ "${_pause_renderer}" _pause_renderer_text)
file(READ "${_font_source}" _font_source_text)
set(_combined "${_header}\n${_host_text}\n${_formal_text}")

foreach(_required IN ITEMS
        "Stage11BValidationScenario"
        "sample_physical_keys"
        "map_host_frame_input"
        "submit_frame_actions"
        "present_frame_and_maybe_capture"
        "SettingsStore"
        "settings_store.load()"
        "settings_store.save("
        "run_child"
        "restarted_settings"
        "paused_freeze"
        "rebound_attack")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing required token: ${_required}")
    endif()
endforeach()

foreach(_stage_required IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary("
        "StableKey::j"
        "StableKey::u"
        "state.pause_capture_while_paused"
        "player_monster_hash_before="
        "load_status=")
    string(FIND "${_stage_text}" "${_stage_required}" _stage_found)
    if(_stage_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing Stage source token: ${_stage_required}")
    endif()
endforeach()
if(NOT _stage_header_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Stage11B evidence guard missing Stage state definition")
endif()
foreach(_host_definition IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary(")
    string(FIND "${_host_text}" "${_host_definition}" _host_definition_found)
    if(NOT _host_definition_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard found Stage definition in host: ${_host_definition}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "quote_command_argument"
        "run_a.sav"
        "run_b.sav"
        "establish_v6_character_slots")
    string(FIND "${_formal_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing real persistence path: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS "PauseMenuRenderer" "draw_crisp_ui_text"
        "record_ui_text_bounds" "设置已恢复默认值")
    string(FIND "${_pause_renderer_text}${_font_source_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing CJK notice rendering: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS "TestAccess" "validation_input_setter"
        "queue_action" "pause_menu.committed =" "LoadImageFromScreen()"
        "character-save-sentinel")
    string(FIND "${_formal_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard rejected forbidden token: ${_forbidden}")
    endif()
endforeach()

string(FIND "${_formal_text}" "platform::run_raylib_host(config)"
    _formal_host_call)
if(_formal_host_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires formal harness to invoke platform::run_raylib_host(config)")
endif()
string(FIND "${_formal_text}" "std::system(command.c_str())"
    _formal_child_call)
if(_formal_child_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires independent child-process invocation via std::system(command.c_str())")
endif()

foreach(_host_forbidden IN ITEMS
        "pause_input.focus_lost = false"
        "snapshot.down.fill(false)"
        "snapshot.pressed.fill(false)"
        "snapshot.escape = false"
        "snapshot.enter = false")
    string(FIND "${_host_text}" "${_host_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard rejected host bypass: ${_host_forbidden}")
    endif()
endforeach()
string(FIND "${_host_text}"
    "? !physical_keys.focus_lost : true" _forced_focus_context)
if(NOT _forced_focus_context EQUAL -1)
    message(FATAL_ERROR "Stage11B evidence guard rejected forced focused pause context")
endif()

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR _capture LESS _present)
    message(FATAL_ERROR "Stage11B evidence guard requires one post-Present capture helper")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \\t\\n]*\\(" _screen_loads "${_host_text}")
list(LENGTH _screen_loads _screen_load_count)
string(REGEX MATCHALL "runtime\\.fixed_tick[ \\t\\n]*\\(" _fixed_ticks "${_host_text}")
list(LENGTH _fixed_ticks _fixed_tick_count)
if(NOT _screen_load_count EQUAL 1)
    message(FATAL_ERROR "Stage11B evidence guard requires exactly one post-Present screen capture")
endif()
if(NOT _fixed_tick_count EQUAL 1)
    message(FATAL_ERROR "Stage11B evidence guard requires exactly one pause-gated runtime.fixed_tick path")
endif()
string(REGEX REPLACE "[ \t\r\n]+" "" _normalized_host "${_host_text}")
string(FIND "${_normalized_host}"
    "if(host_gate.forward_gameplay)" _fixed_gate)
string(FIND "${_normalized_host}"
    "runtime.fixed_tick(step_movement,loot_pickup_policy(live_settings.loot_filter_mode));"
    _live_policy_call)
string(FIND "${_normalized_host}"
    "runtime.fixed_tick(step_movement,loot_pickup_policy(pause_menu.draft.loot_filter_mode));"
    _draft_policy_call)
if(NOT _draft_policy_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard rejects draft loot policy in fixed_tick")
endif()
if(_fixed_gate EQUAL -1 OR _live_policy_call EQUAL -1
        OR NOT _fixed_gate LESS _live_policy_call)
    message(FATAL_ERROR
        "Stage11B evidence guard requires live loot policy behind host gate")
endif()
string(FIND "${_host_text}"
    "const SubmittedFrameActions submitted_actions =" _accepted_actions)
string(FIND "${_host_text}"
    "submitted_actions.combat[0] ? 1U : 0U" _accepted_attack_count)
if(_accepted_actions EQUAL -1 OR _accepted_attack_count EQUAL -1)
    message(FATAL_ERROR "Stage11B evidence guard requires accepted queue_action evidence")
endif()
string(FIND "${_host_text}" "const bool stage11b_paused_visible_capture ="
    _paused_capture_declaration)
string(FIND "${_host_text}"
    "&& pause_menu.screen != PauseScreen::closed"
    _paused_capture_menu_visible)
string(FIND "${_host_text}"
    "&& stage11b_validation_state.paused_presented >= 120U"
    _paused_capture_freeze_count)
string(FIND "${_host_text}"
    "stage11b_validation_state.pause_capture_while_paused ="
    _paused_capture_recorded)
if(_paused_capture_declaration EQUAL -1 OR _paused_capture_menu_visible EQUAL -1
        OR _paused_capture_freeze_count EQUAL -1 OR _paused_capture_recorded EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires paused screenshot before resume and Present")
endif()
string(SUBSTRING "${_host_text}" ${_paused_capture_declaration} -1
    _paused_capture_tail)
string(FIND "${_paused_capture_tail}" "present_frame_and_maybe_capture("
    _paused_capture_present_call)
if(_paused_capture_present_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires paused screenshot before resume and Present")
endif()

foreach(_ordered IN ITEMS
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "HostFrameInput frame_input = map_host_frame_input("
        "HostFrameGateResult host_gate"
        "submit_frame_actions(*session, frame_input)")
    string(FIND "${_host_text}" "${_ordered}" _order_index)
    if(_order_index EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing production pipeline step: ${_ordered}")
    endif()
    if(DEFINED _previous_order_index AND _order_index LESS _previous_order_index)
        message(FATAL_ERROR "Stage11B evidence guard rejected out-of-order physical input pipeline")
    endif()
    set(_previous_order_index ${_order_index})
endforeach()
