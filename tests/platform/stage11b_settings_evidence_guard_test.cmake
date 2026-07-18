if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
else()
    set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
endif()
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
else()
    set(_formal "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp")
endif()
foreach(_required IN ITEMS "${_host}" "${_header}" "${_formal}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11B evidence target is missing: ${_required}")
    endif()
endforeach()

file(READ "${_host}" _host_text)
file(READ "${_header}" _header_text)
file(READ "${_formal}" _formal_text)
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

foreach(_forbidden IN ITEMS "TestAccess" "validation_input_setter"
        "queue_action" "pause_menu.committed =")
    string(FIND "${_formal_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard rejected forbidden token: ${_forbidden}")
    endif()
endforeach()

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
