if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/combat_renderer.cpp" COMBAT_RENDERER_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/room_renderer.cpp" ROOM_RENDERER_SOURCE)

string(FIND "${ROOM_RENDERER_SOURCE}" "\"E: DESCEND\""
    LEGACY_HOLE_INTERACTION_PROMPT)
if(NOT LEGACY_HOLE_INTERACTION_PROMPT EQUAL -1)
    message(FATAL_ERROR
        "hole interaction prompt must not hard-code the default E binding")
endif()

set(DYNAMIC_HOLE_PROMPT [=[const auto prompt = hole_interaction_prompt(
            interact_binding_label);]=])
string(FIND "${ROOM_RENDERER_SOURCE}" "${DYNAMIC_HOLE_PROMPT}"
    DYNAMIC_HOLE_PROMPT_INDEX)
if(DYNAMIC_HOLE_PROMPT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "hole renderer must format the current interact binding")
endif()

set(DYNAMIC_HOLE_DRAW [=[draw_room(world, render_plan.ground_loot,
                render_plan.material_loot, camera, interact_binding_label);]=])
string(FIND "${COMBAT_RENDERER_SOURCE}" "${DYNAMIC_HOLE_DRAW}"
    DYNAMIC_HOLE_DRAW_INDEX)
if(DYNAMIC_HOLE_DRAW_INDEX EQUAL -1)
    message(FATAL_ERROR
        "combat renderer must pass the current interact binding to the room")
endif()

set(DYNAMIC_HOLE_HOST [=[feedback, audio_ready, stable_key_label(
                        settings::binding_for(input_settings,
                            settings::SettingAction::interact)))]=])
string(FIND "${HOST_SOURCE}" "${DYNAMIC_HOLE_HOST}"
    DYNAMIC_HOLE_HOST_INDEX)
if(DYNAMIC_HOLE_HOST_INDEX EQUAL -1)
    message(FATAL_ERROR
        "host must pass the applied interact binding to world rendering")
endif()

message(STATUS
    "[CPLAY-030] hole prompt follows the applied interact binding")
