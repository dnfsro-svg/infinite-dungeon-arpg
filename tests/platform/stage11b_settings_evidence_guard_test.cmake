if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
else()
    set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
endif()
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_pause_renderer "${SOURCE_ROOT}/src/platform/raylib/pause_menu_renderer.cpp")
set(_font_source "${SOURCE_ROOT}/src/platform/raylib/death_overlay_font.cpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
else()
    set(_formal "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp")
endif()
foreach(_required IN ITEMS "${_host}" "${_header}" "${_formal}" "${_pause_renderer}" "${_font_source}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11B evidence target is missing: ${_required}")
    endif()
endforeach()

file(READ "${_host}" _host_text)
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
foreach(_required IN ITEMS "PauseMenuRenderer" "DrawTextEx" "设置已恢复默认值")
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
string(FIND "${_host_text}" "if (host_gate.forward_gameplay)" _fixed_gate)
string(FIND "${_host_text}" "runtime.fixed_tick(step_movement);" _fixed_tick)
if(_fixed_gate EQUAL -1 OR _fixed_tick EQUAL -1 OR NOT _fixed_gate LESS _fixed_tick)
    message(FATAL_ERROR "Stage11B evidence guard requires runtime.fixed_tick behind host gate")
endif()
string(FIND "${_host_text}" "const std::array<bool, 3> accepted_actions =" _accepted_actions)
string(FIND "${_host_text}" "accepted_actions[0] ? 1U : 0U" _accepted_attack_count)
if(_accepted_actions EQUAL -1 OR _accepted_attack_count EQUAL -1)
    message(FATAL_ERROR "Stage11B evidence guard requires accepted queue_action evidence")
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
