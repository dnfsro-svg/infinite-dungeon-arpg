cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

foreach(SOURCE_NAME IN ITEMS HOST_SOURCE COMBAT_RENDERER_SOURCE ROOM_RENDERER_SOURCE)
    if(NOT DEFINED ${SOURCE_NAME})
        if(SOURCE_NAME STREQUAL "HOST_SOURCE")
            set(${SOURCE_NAME} "${RAYLIB_SOURCE_DIR}/raylib_host.cpp")
        elseif(SOURCE_NAME STREQUAL "COMBAT_RENDERER_SOURCE")
            set(${SOURCE_NAME} "${RAYLIB_SOURCE_DIR}/combat_renderer.cpp")
        else()
            set(${SOURCE_NAME} "${RAYLIB_SOURCE_DIR}/room_renderer.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${SOURCE_NAME}}")
        message(FATAL_ERROR "${SOURCE_NAME} is required")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
file(READ "${HOST_SOURCE}" HOST_TEXT)
file(READ "${COMBAT_RENDERER_SOURCE}" COMBAT_TEXT)
file(READ "${ROOM_RENDERER_SOURCE}" ROOM_TEXT)
normalize_cpp_source("${ROOM_TEXT}" ROOM_NORMALIZED ROOM_CODE)

foreach(LEGACY_TEXT IN ITEMS
        "E: DESCEND" "SEALED" "READY" "SAVING" "FAULTED"
        "ABYSS HIGH" "ABYSS FURY")
    string(FIND "${ROOM_NORMALIZED}" "\"${LEGACY_TEXT}\"" LEGACY_INDEX)
    if(NOT LEGACY_INDEX EQUAL -1)
        message(FATAL_ERROR
            "room renderer contains legacy player text: ${LEGACY_TEXT}")
    endif()
endforeach()

extract_cpp_function_body("${ROOM_TEXT}" "void draw_world_prompt("
    WORLD_PROMPT_BODY WORLD_PROMPT_CODE)
foreach(REQUIRED_TOKEN IN ITEMS
        "MeasureTextEx(font, text, font_size, 1.0F)"
        "world_prompt_layout( anchor, measured, screen_width, screen_height)"
        "color = raylib_color(world_prompt_text_color( {color.r, color.g, color.b, color.a}))"
        "DrawRectangleRec(layout.backing, raylib_color(style.backing))"
        "draw_crisp_ui_text(font, text, layout.text_position, font_size * layout.text_scale, 1.0F * layout.text_scale, color)")
    string(FIND "${WORLD_PROMPT_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "world prompt must measure, back, and draw shared CJK text: ${REQUIRED_TOKEN}")
    endif()
endforeach()
string(FIND "${WORLD_PROMPT_BODY}"
    "DrawRectangleRec(layout.backing" BACKING_INDEX)
string(FIND "${WORLD_PROMPT_BODY}"
    "draw_crisp_ui_text(font, text" CJK_DRAW_INDEX)
if(BACKING_INDEX GREATER CJK_DRAW_INDEX)
    message(FATAL_ERROR "world prompt backing must be drawn before text")
endif()
if(NOT WORLD_PROMPT_CODE MATCHES "if[ ]*\\([ ]*!font_ready")
    message(FATAL_ERROR "world prompt must reject an unavailable shared font")
endif()
if(WORLD_PROMPT_CODE MATCHES "if[ ]*\\([ ]*(false|0)[ ]*\\)")
    message(FATAL_ERROR "world prompt requirements cannot live in a dead branch")
endif()
foreach(FORBIDDEN_DRAW IN ITEMS "DrawText(" "DrawTextEx(" "MeasureText(")
    string(FIND "${WORLD_PROMPT_CODE}" "${FORBIDDEN_DRAW}" FORBIDDEN_INDEX)
    if(NOT FORBIDDEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "world prompt must not use the default/legacy text path: ${FORBIDDEN_DRAW}")
    endif()
endforeach()

extract_cpp_function_body("${ROOM_TEXT}" "void draw_hole("
    HOLE_BODY HOLE_CODE)
foreach(REQUIRED_TOKEN IN ITEMS
        "hole_visual_label(hole)"
        "hole_interaction_prompt( interact_binding_label, HoleInteractionAction::descend)"
        "draw_world_prompt(label, {geometry.center.x, geometry.center.y - 12.0F}, 20.0F, label_color, font, font_ready, screen_width, screen_height)"
        "draw_world_prompt(prompt.data(), {geometry.center.x, geometry.center.y + 36.0F}, 26.0F, raylib_color(world_prompt_style().primary), font, font_ready, screen_width, screen_height)")
    string(FIND "${HOLE_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "hole renderer must consume labels, dynamic key, and shared font: ${REQUIRED_TOKEN}")
    endif()
endforeach()

extract_cpp_function_body("${ROOM_TEXT}" "void draw_doors("
    DOOR_BODY DOOR_CODE)
extract_cpp_function_body("${ROOM_TEXT}" "void draw_abyss("
    ABYSS_BODY ABYSS_CODE)
foreach(BODY_NAME IN ITEMS HOLE_CODE DOOR_CODE ABYSS_CODE)
    foreach(FORBIDDEN_DRAW IN ITEMS "DrawText(" "DrawTextEx(" "MeasureText(")
        string(FIND "${${BODY_NAME}}" "${FORBIDDEN_DRAW}" FORBIDDEN_INDEX)
        if(NOT FORBIDDEN_INDEX EQUAL -1)
            message(FATAL_ERROR
                "room text function must use only the shared CJK path: ${FORBIDDEN_DRAW}")
        endif()
    endforeach()
endforeach()
foreach(REQUIRED_TOKEN IN ITEMS "draw_world_prompt(visual.label")
    string(FIND "${DOOR_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR "door renderer must use the shared world prompt path")
    endif()
endforeach()
foreach(REQUIRED_TOKEN IN ITEMS "draw_world_prompt(label")
    string(FIND "${ABYSS_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR "abyss renderer must use the shared world prompt path")
    endif()
endforeach()

extract_cpp_function_body("${ROOM_TEXT}"
    "void CombatRenderer::draw_abyss_overlay(" ABYSS_OVERLAY_BODY ABYSS_OVERLAY_CODE)
set(ABYSS_SHARED_FONT_CALL
    "draw_abyss(current, static_cast<float>(GetTime()), world_ui_font_, world_ui_font_ready_)")
string(FIND "${ABYSS_OVERLAY_BODY}" "${ABYSS_SHARED_FONT_CALL}"
    ABYSS_SHARED_FONT_INDEX)
if(ABYSS_SHARED_FONT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "CombatRenderer abyss overlay must pass its shared CJK font")
endif()
string(REGEX MATCHALL "draw_abyss\\(" ABYSS_DRAW_CALLS
    "${ABYSS_OVERLAY_CODE}")
list(LENGTH ABYSS_DRAW_CALLS ABYSS_DRAW_CALL_COUNT)
if(NOT ABYSS_DRAW_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR
        "CombatRenderer abyss overlay must contain exactly one live draw call")
endif()
foreach(FORBIDDEN_TOKEN IN ITEMS "GetFontDefault(" "if(false)" "if (false)"
        "if(0)" "if (0)")
    string(FIND "${ABYSS_OVERLAY_CODE}" "${FORBIDDEN_TOKEN}"
        FORBIDDEN_INDEX)
    if(NOT FORBIDDEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "CombatRenderer abyss overlay contains a default-font or dead decoy path")
    endif()
endforeach()

extract_cpp_function_body("${ROOM_TEXT}" "void CombatRenderer::draw_room("
    DRAW_ROOM_BODY DRAW_ROOM_CODE)
foreach(REQUIRED_TOKEN IN ITEMS
        "draw_doors(current, camera, width, height, material_pack_, world_ui_font_, world_ui_font_ready_)"
        "draw_hole(current, material_pack_, camera, interact_binding_label, world_ui_font_, world_ui_font_ready_)")
    string(FIND "${DRAW_ROOM_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR "CombatRenderer room path must pass its shared CJK font")
    endif()
endforeach()

extract_cpp_function_body("${COMBAT_TEXT}"
    "bool CombatRenderer::initialize_resources(" INITIALIZE_BODY)
extract_cpp_function_body("${COMBAT_TEXT}"
    "void CombatRenderer::shutdown_resources(" SHUTDOWN_BODY)
extract_cpp_function_body("${COMBAT_TEXT}"
    "GroundLootView CombatRenderer::draw(" COMBAT_DRAW_BODY)
foreach(REQUIRED_TOKEN IN ITEMS
        "world_ui_font_ = shared_ui_font.font()"
        "world_ui_font_ready_ = shared_ui_font.ready()")
    string(FIND "${INITIALIZE_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR "CombatRenderer must borrow the shared UI font")
    endif()
endforeach()
foreach(REQUIRED_TOKEN IN ITEMS "world_ui_font_ = {}"
        "world_ui_font_ready_ = false")
    string(FIND "${SHUTDOWN_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR "CombatRenderer must clear its borrowed UI font")
    endif()
endforeach()
string(FIND "${COMBAT_DRAW_BODY}"
    "draw_room(world, render_plan.ground_loot, render_plan.material_loot, camera, interact_binding_label)"
    COMBAT_DYNAMIC_KEY_INDEX)
if(COMBAT_DYNAMIC_KEY_INDEX EQUAL -1)
    message(FATAL_ERROR "CombatRenderer must forward the same interact key variable")
endif()

set(HOST_DYNAMIC_KEY [=[feedback, audio_ready, stable_key_label(
                        settings::binding_for(input_settings,
                            settings::SettingAction::interact)))]=])
string(FIND "${HOST_TEXT}" "${HOST_DYNAMIC_KEY}" HOST_DYNAMIC_KEY_INDEX)
if(HOST_DYNAMIC_KEY_INDEX EQUAL -1)
    message(FATAL_ERROR
        "host must pass the applied structured interact binding to world rendering")
endif()

message(STATUS
    "[CPLAY-030] hole prompt and room text use the applied binding and shared CJK font")
