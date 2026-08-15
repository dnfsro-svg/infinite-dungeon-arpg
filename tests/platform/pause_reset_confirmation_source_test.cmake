cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp" _header)
file(READ "${SOURCE_ROOT}/src/platform/raylib/host_frame_gate.cpp" _gate)
file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" _host)
file(READ "${SOURCE_ROOT}/src/platform/raylib/pause_menu_state.cpp" _state)
file(READ "${SOURCE_ROOT}/src/platform/raylib/pause_menu_view.cpp" _view)

foreach(_name IN ITEMS header gate host state view)
    string(REGEX REPLACE "[ \t\r\n]+" "" _${_name} "${_${_name}}")
endforeach()

function(pause_reset_contract_valid HEADER GATE HOST STATE VIEW OUT_VALID)
    set(_valid TRUE)
    foreach(_required IN ITEMS
            "structHostRoomResetGateInputfinal{PauseScreenpause_screen{};boolgameplay_armed{};booldeath_allows_gameplay{};boolpause_reset_confirmed{};};"
            "if(!input.gameplay_armed||!input.death_allows_gameplay){returnfalse;}"
            "returninput.pause_screen==PauseScreen::reset_confirm&&input.pause_reset_confirmed;")
        string(FIND "${HEADER}${GATE}" "${_required}" _position)
        if(_position EQUAL -1)
            set(_valid FALSE)
        endif()
    endforeach()
    foreach(_forbidden IN ITEMS
            "input.reset_pressed" "input.forward_actions"
            "input.inventory_allows_room_reset")
        string(FIND "${GATE}" "${_forbidden}" _position)
        if(NOT _position EQUAL -1)
            set(_valid FALSE)
        endif()
    endforeach()

    foreach(_required IN ITEMS
            "pause_input.reset=frame_input.keys.reset;"
            "pause_input.reset=false;"
            "room_reset_gate.pause_screen=pause_screen_before;"
            "room_reset_gate.gameplay_armed=gameplay_armed&&!window_close_requested;"
            "room_reset_gate.pause_reset_confirmed=pause_command==PauseCommand::reset_room;"
            "if(pause_command==PauseCommand::reset_room&&!room_reset_requested){resolve_pause_room_reset(pause_menu,false);}"
            "resolve_pause_room_reset(pause_menu,reset!=dungeon::RequestResult::rejected);")
        string(FIND "${HOST}" "${_required}" _position)
        if(_position EQUAL -1)
            set(_valid FALSE)
        endif()
    endforeach()
    string(FIND "${HOST}" "pause_input.reset=frame_input.keys.reset;" _mapped)
    string(FIND "${HOST}" "pause_input.reset=false;" _pointer_clear)
    string(FIND "${HOST}" "constPauseCommandpause_command=update_pause_menu(" _update)
    if(_mapped EQUAL -1 OR _pointer_clear EQUAL -1 OR _update EQUAL -1
            OR NOT _mapped LESS _pointer_clear
            OR NOT _pointer_clear LESS _update)
        set(_valid FALSE)
    endif()

    foreach(_required IN ITEMS
            "open_reset_confirm(state,context,PauseRoomResetOrigin::closed);"
            "open_reset_confirm(state,context,PauseRoomResetOrigin::root);"
            "if(!cleared_abyss&&state.selected_row==0U){state.message=nullptr;returnPauseCommand::reset_room;}"
            "state.message=kResetRejected;")
        string(FIND "${STATE}" "${_required}" _position)
        if(_position EQUAL -1)
            set(_valid FALSE)
        endif()
    endforeach()

    foreach(_required IN ITEMS
            "u8\"确认重置房间？\""
            "u8\"本房间的未保存经验和地面掉落将丢失\""
            "u8\"确认放弃深渊？\""
            "u8\"本次深渊将失败，并放弃全部剩余奖励\""
            "u8\"深渊已清理\"")
        string(FIND "${VIEW}" "${_required}" _position)
        if(_position EQUAL -1)
            set(_valid FALSE)
        endif()
    endforeach()
    set(${OUT_VALID} ${_valid} PARENT_SCOPE)
endfunction()

pause_reset_contract_valid(
    "${_header}" "${_gate}" "${_host}" "${_state}" "${_view}" _valid)
if(NOT _valid)
    message(FATAL_ERROR "Pause reset confirmation direct contract failed")
endif()

function(expect_mutation_rejected LABEL SOURCE_NAME OLD NEW)
    set(_source "${_${SOURCE_NAME}}")
    string(REPLACE "${OLD}" "${NEW}" _mutation "${_source}")
    if("${_mutation}" STREQUAL "${_source}")
        message(FATAL_ERROR "Pause reset mutation anchor missing: ${LABEL}")
    endif()
    set(_mut_header "${_header}")
    set(_mut_gate "${_gate}")
    set(_mut_host "${_host}")
    set(_mut_state "${_state}")
    set(_mut_view "${_view}")
    set(_mut_${SOURCE_NAME} "${_mutation}")
    pause_reset_contract_valid(
        "${_mut_header}" "${_mut_gate}" "${_mut_host}"
        "${_mut_state}" "${_mut_view}" _mutation_valid)
    if(_mutation_valid)
        message(FATAL_ERROR "Pause reset mutation survived: ${LABEL}")
    endif()
endfunction()

expect_mutation_rejected("root direct reset" gate
    "PauseScreen::reset_confirm" "PauseScreen::root")
expect_mutation_rejected("confirmation command ignored" gate
    "&&input.pause_reset_confirmed" "&&true")
expect_mutation_rejected("pointer leaves reset edge armed" host
    "pause_input.reset=false;" "pause_input.reset=true;")
expect_mutation_rejected("blocked command is silent" host
    "resolve_pause_room_reset(pause_menu,false);"
    "static_cast<void>(pause_menu);")
expect_mutation_rejected("authority result is not returned" host
    "resolve_pause_room_reset(pause_menu,reset!=dungeon::RequestResult::rejected);"
    "static_cast<void>(reset);")
expect_mutation_rejected("closed origin is lost" state
    "open_reset_confirm(state,context,PauseRoomResetOrigin::closed);"
    "open_reset_confirm(state,context,PauseRoomResetOrigin::root);")
expect_mutation_rejected("confirm row emits no reset command" state
    "returnPauseCommand::reset_room;" "returnPauseCommand::none;")

message(STATUS "Pause reset confirmation direct contract passed")
