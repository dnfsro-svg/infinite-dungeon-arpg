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

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR _capture LESS _present)
    message(FATAL_ERROR "Stage11B evidence guard requires one post-Present capture helper")
endif()
