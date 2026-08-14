cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RAYLIB_SOURCE_DIR AND DEFINED SOURCE_ROOT)
    set(RAYLIB_SOURCE_DIR "${SOURCE_ROOT}/src/platform/raylib")
endif()
if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR or SOURCE_ROOT is required")
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

function(require_no_dead_branch BODY LABEL)
    string(REGEX MATCH
        "if([ ]+constexpr)?[ ]*\\([ ]*(false|0)[ ]*\\)"
        DEAD_BRANCH "${BODY}")
    if(NOT DEAD_BRANCH STREQUAL "")
        message(FATAL_ERROR
            "${LABEL} cannot hide renderer requirements in a dead branch")
    endif()
endfunction()

function(require_no_uncalled_lambda BODY LABEL)
    set(REMAINDER "${BODY}")
    while(TRUE)
        string(REGEX MATCH
            "(const[ ]+)?auto([ ]*&&|[ ]*&)?[ ]+([A-Za-z_][A-Za-z0-9_]*)[ ]*=[ ]*\\["
            LAMBDA_DECLARATION "${REMAINDER}")
        if(LAMBDA_DECLARATION STREQUAL "")
            break()
        endif()
        set(LAMBDA_NAME "${CMAKE_MATCH_3}")
        string(LENGTH "${LAMBDA_DECLARATION}" DECLARATION_LENGTH)
        string(FIND "${REMAINDER}" "${LAMBDA_DECLARATION}"
            DECLARATION_INDEX)
        math(EXPR AFTER_DECLARATION
            "${DECLARATION_INDEX} + ${DECLARATION_LENGTH}")
        string(SUBSTRING "${REMAINDER}" ${AFTER_DECLARATION} -1
            AFTER_DECLARATION_TEXT)
        string(REGEX MATCH
            "(^|[^A-Za-z0-9_])(${LAMBDA_NAME}[ ]*\\(|std::invoke[ ]*\\([ ]*${LAMBDA_NAME}([^A-Za-z0-9_]|$))"
            LAMBDA_INVOKED "${AFTER_DECLARATION_TEXT}")
        if(LAMBDA_INVOKED STREQUAL "")
            message(FATAL_ERROR
                "${LABEL} cannot hide renderer requirements in an uncalled lambda")
        endif()
        set(REMAINDER "${AFTER_DECLARATION_TEXT}")
    endwhile()
endfunction()

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

string(FIND "${ROOM_TEXT}" "void draw_world_prompt(" WORLD_PROMPT_START)
if(WORLD_PROMPT_START EQUAL -1)
    message(FATAL_ERROR "Unable to find room world-prompt helper")
endif()
string(SUBSTRING "${ROOM_TEXT}" ${WORLD_PROMPT_START} -1 WORLD_PROMPT_TEXT)
extract_cpp_function_body("${WORLD_PROMPT_TEXT}" ") noexcept {"
    WORLD_PROMPT_BODY WORLD_PROMPT_CODE)
foreach(REQUIRED_TOKEN IN ITEMS
        "MeasureTextEx(font, text, font_size, 1.0F)"
        "door_bounds != nullptr"
        "door_world_prompt_layout(anchor, measured, *door_bounds, hud_blocker, screen_offset, screen_width, screen_height)"
        ": world_prompt_layout( anchor, measured, screen_width, screen_height)"
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
foreach(FORBIDDEN_DRAW IN ITEMS
        "DrawText(" "DrawTextEx(" "MeasureText(" "GetFontDefault(")
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
        "active_skill_hud_layout( static_cast<int>(screen_width), static_cast<int>(screen_height))"
        "active_skill_layout.valid ? active_skill_layout.visual_bounds : Rectangle{}"
        "const Rectangle hole_bounds{geometry.center.x - geometry.radius_x, geometry.center.y - geometry.radius_y, geometry.radius_x * 2.0F, geometry.radius_y * 2.0F}"
        "interact_binding_label != nullptr && interact_binding_label[0] != '\\0'"
        "hole_interaction_prompt( interact_binding_label, HoleInteractionAction::descend)"
        "draw_world_prompt(label, {geometry.center.x, geometry.center.y - 12.0F}, 20.0F, label_color, font, font_ready, screen_width, screen_height, &hole_bounds, hud_blocker, screen_offset)"
        "draw_world_prompt(prompt.data(), {geometry.center.x, geometry.center.y + 36.0F}, 26.0F, raylib_color(world_prompt_style().primary), font, font_ready, screen_width, screen_height, &hole_bounds, hud_blocker, screen_offset)")
    string(FIND "${HOLE_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "hole renderer must use its live bounds, HUD blocker, offset, dynamic key, and shared font: ${REQUIRED_TOKEN}")
    endif()
endforeach()
string(REGEX MATCHALL "active_skill_hud_layout\\(" HOLE_ACTIVE_SKILL_LAYOUT_CALLS
    "${HOLE_CODE}")
list(LENGTH HOLE_ACTIVE_SKILL_LAYOUT_CALLS HOLE_ACTIVE_SKILL_LAYOUT_CALL_COUNT)
if(NOT HOLE_ACTIVE_SKILL_LAYOUT_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR
        "hole renderer must compute the active-skill layout exactly once per draw")
endif()
string(FIND "${HOLE_CODE}" "active_skill_layout.bounds" HOLE_SLOT_ONLY_INDEX)
if(NOT HOLE_SLOT_ONLY_INDEX EQUAL -1)
    message(FATAL_ERROR "hole renderer must use the complete active-skill visual bounds")
endif()
string(FIND "${HOLE_BODY}" "} else { draw_world_prompt(label"
    HOLE_SINGLE_PROMPT_BRANCH_INDEX)
if(HOLE_SINGLE_PROMPT_BRANCH_INDEX EQUAL -1)
    message(FATAL_ERROR
        "hole renderer must draw either the dynamic interaction prompt or the static label")
endif()
string(REGEX MATCHALL "draw_world_prompt\\(" HOLE_WORLD_PROMPT_CALLS
    "${HOLE_CODE}")
list(LENGTH HOLE_WORLD_PROMPT_CALLS HOLE_WORLD_PROMPT_CALL_COUNT)
if(NOT HOLE_WORLD_PROMPT_CALL_COUNT EQUAL 2)
    message(FATAL_ERROR
        "hole renderer must contain exactly one dynamic and one static prompt call")
endif()

extract_cpp_function_body("${ROOM_TEXT}" "void draw_doors("
    DOOR_BODY DOOR_CODE)
extract_cpp_function_body("${ROOM_TEXT}" "Rectangle material_destination_bounds("
    MATERIAL_DESTINATION_BODY MATERIAL_DESTINATION_CODE)
extract_cpp_function_body("${ROOM_TEXT}" "void draw_abyss("
    ABYSS_BODY ABYSS_CODE)
foreach(BODY_NAME IN ITEMS HOLE_CODE DOOR_CODE ABYSS_CODE)
    foreach(FORBIDDEN_DRAW IN ITEMS
            "DrawText(" "DrawTextEx(" "MeasureText(" "GetFontDefault(")
        string(FIND "${${BODY_NAME}}" "${FORBIDDEN_DRAW}" FORBIDDEN_INDEX)
        if(NOT FORBIDDEN_INDEX EQUAL -1)
            message(FATAL_ERROR
                "room text function must use only the shared CJK path: ${FORBIDDEN_DRAW}")
        endif()
    endforeach()
endforeach()
foreach(REQUIRED_TOKEN IN ITEMS
        "const MaterialManifestDefinition manifest = default_material_manifest()"
        "const MaterialFrameDefinition& frame = manifest.frames[index]"
        "frame.id != id"
        "foot_position.x - frame.foot_anchor.x * scale"
        "foot_position.y - frame.foot_anchor.y * scale"
        "frame.source.width * scale"
        "frame.source.height * scale")
    string(FIND "${MATERIAL_DESTINATION_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "door material bounds must mirror MaterialPack::draw: ${REQUIRED_TOKEN}")
    endif()
endforeach()
foreach(FORBIDDEN_CONSTANT IN ITEMS "256.0F" "244.0F")
    string(FIND "${MATERIAL_DESTINATION_CODE}" "${FORBIDDEN_CONSTANT}"
        FORBIDDEN_INDEX)
    if(NOT FORBIDDEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "door material bounds must come from the manifest, not duplicated constants")
    endif()
endforeach()
foreach(REQUIRED_TOKEN IN ITEMS
        "active_skill_hud_layout( static_cast<int>(width), static_cast<int>(height))"
        "active_skill_layout.valid ? active_skill_layout.visual_bounds : Rectangle{}"
        "material_destination_bounds( visual.sprite, {projected.x, projected.ground_y}, material_scale)"
        "const bool material_drawn = material_pack.draw(visual.sprite, {projected.x, projected.ground_y}, false, material_scale, body_tint)"
        "material_drawn && material_destination.width > 0.0F && material_destination.height > 0.0F ? material_destination : frame"
        "draw_world_prompt(visual.label, {projected.x, frame.y - label_size - 14.0F}, label_size, text_color, font, font_ready, width, height, &door_bounds, hud_blocker, screen_offset)")
    string(FIND "${DOOR_BODY}" "${REQUIRED_TOKEN}" TOKEN_INDEX)
    if(TOKEN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "door renderer must consume actual bounds, HUD blocker, and screen offset: ${REQUIRED_TOKEN}")
    endif()
endforeach()
string(REGEX MATCHALL "active_skill_hud_layout\\(" ACTIVE_SKILL_LAYOUT_CALLS
    "${DOOR_CODE}")
list(LENGTH ACTIVE_SKILL_LAYOUT_CALLS ACTIVE_SKILL_LAYOUT_CALL_COUNT)
if(NOT ACTIVE_SKILL_LAYOUT_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR
        "door renderer must compute the active-skill layout exactly once per draw")
endif()
string(FIND "${DOOR_CODE}" "active_skill_layout.bounds" SLOT_ONLY_INDEX)
if(NOT SLOT_ONLY_INDEX EQUAL -1)
    message(FATAL_ERROR "door renderer must use the complete active-skill visual bounds")
endif()
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
        "draw_doors(current, camera, width, height, material_pack_, world_ui_font_, world_ui_font_ready_, screen_offset)"
        "draw_hole(current, material_pack_, camera, interact_binding_label, world_ui_font_, world_ui_font_ready_, screen_offset)")
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
set(COMBAT_ROOM_CALL
    "draw_room(world, render_plan.ground_loot, render_plan.material_loot, camera, interact_binding_label, {camera_offset.x, camera_offset.y})")
string(FIND "${COMBAT_DRAW_BODY}" "${COMBAT_ROOM_CALL}"
    COMBAT_DYNAMIC_KEY_INDEX)
if(COMBAT_DYNAMIC_KEY_INDEX EQUAL -1)
    message(FATAL_ERROR
        "CombatRenderer must forward the same interact key and live camera offset")
endif()

foreach(CODE_NAME IN ITEMS WORLD_PROMPT_CODE HOLE_CODE DOOR_CODE DRAW_ROOM_CODE
        COMBAT_DRAW_BODY MATERIAL_DESTINATION_CODE)
    string(FIND "${${CODE_NAME}}" "GetFontDefault(" DEFAULT_FONT_INDEX)
    if(NOT DEFAULT_FONT_INDEX EQUAL -1)
        message(FATAL_ERROR
            "door-specific renderer paths must not use the default font")
    endif()
    require_no_dead_branch("${${CODE_NAME}}" "${CODE_NAME}")
    require_no_uncalled_lambda("${${CODE_NAME}}" "${CODE_NAME}")
endforeach()

set(HOST_DYNAMIC_KEY [=[feedback, audio_ready, stable_key_label(
                        settings::binding_for(input_settings,
                            settings::SettingAction::interact)))]=])
string(FIND "${HOST_TEXT}" "${HOST_DYNAMIC_KEY}" HOST_DYNAMIC_KEY_INDEX)
if(HOST_DYNAMIC_KEY_INDEX EQUAL -1)
    message(FATAL_ERROR
        "host must pass the applied structured interact binding to world rendering")
endif()

message(STATUS
    "[CPLAY-030] door and hole prompts use live bounds/offset/HUD blocker; abyss stays generic")
