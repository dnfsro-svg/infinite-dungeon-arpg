cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(_raylib_root "${SOURCE_ROOT}/src/platform/raylib")
foreach(_name IN ITEMS OVERLAY_HEADER OVERLAY_SOURCE DEBUG_SOURCE ACTOR_SOURCE
        COMBAT_SOURCE VIEW_MATH_SOURCE)
    if(NOT DEFINED ${_name})
        if(_name STREQUAL "OVERLAY_HEADER")
            set(${_name} "${_raylib_root}/debug_overlay_renderer.hpp")
        elseif(_name STREQUAL "OVERLAY_SOURCE")
            set(${_name} "${_raylib_root}/debug_overlay_renderer.cpp")
        elseif(_name STREQUAL "DEBUG_SOURCE")
            set(${_name} "${_raylib_root}/debug_renderer.cpp")
        elseif(_name STREQUAL "ACTOR_SOURCE")
            set(${_name} "${_raylib_root}/actor_renderer.cpp")
        elseif(_name STREQUAL "COMBAT_SOURCE")
            set(${_name} "${_raylib_root}/combat_renderer.cpp")
        else()
            set(${_name} "${_raylib_root}/combat_view_math.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${_name}}")
        message(FATAL_ERROR "Task 7 source is missing: ${_name}=${${_name}}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
file(READ "${OVERLAY_HEADER}" _overlay_header)
file(READ "${OVERLAY_SOURCE}" _overlay_source)
file(READ "${DEBUG_SOURCE}" _debug_source)
file(READ "${ACTOR_SOURCE}" _actor_source)
file(READ "${COMBAT_SOURCE}" _combat_source)
file(READ "${VIEW_MATH_SOURCE}" _view_math_source)

function(require_token TEXT TOKEN MESSAGE_TEXT)
    string(FIND "${TEXT}" "${TOKEN}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: ${TOKEN}")
    endif()
endfunction()

function(reject_dead_branch CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES "if[ ]*\\([ ]*(false|0)[ ]*\\)"
            OR "${CODE}" MATCHES
                "if[ ]*\\([^\\)]*&&[ ]*false([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR "${MESSAGE_TEXT} cannot live in a dead branch")
    endif()
endfunction()

normalize_cpp_source("${_overlay_header}" _overlay_header_normalized)
string(REGEX REPLACE "[ \t\r\n]+" "" _overlay_header_compact
    "${_overlay_header_normalized}")
require_token("${_overlay_header_compact}"
    "std::array<char,192U>text{};"
    "Task 7 overlay line must own a fixed UTF-8 buffer")
require_token("${_overlay_header_compact}"
    "std::array<DebugOverlayLine,12U>lines{};"
    "Task 7 overlay view must own exactly twelve lines")

extract_cpp_function_body("${_overlay_source}"
    "DebugOverlayView make_debug_overlay_view(" _view_body _view_code)
foreach(_token IN ITEMS
        "view.line_count = view.lines.size()"
        "ui_utf8_format(view.lines[0].text.data()"
        "ui_utf8_format(view.lines[11].text.data()")
    require_token("${_view_body}" "${_token}"
        "Task 7 overlay view must use the fixed-buffer UTF-8 model")
endforeach()
foreach(_forbidden IN ITEMS "TextFormat(" "DrawText(" "DrawTextEx("
        "GetFontDefault(" "LoadFont")
    string(FIND "${_view_code}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Task 7 overlay view uses a forbidden dynamic/default path: ${_forbidden}")
    endif()
endforeach()
reject_dead_branch("${_view_code}" "Task 7 overlay view")

extract_cpp_function_body("${_overlay_source}"
    "DebugOverlayLayout debug_overlay_layout(" _layout_body _layout_code)
foreach(_token IN ITEMS
        "screen_width < 1280 ? 1U : 2U"
        "layout.column_count == 1U ? 12U : 6U"
        "layout.backing_color = {0x0B, 0x10, 0x19, 255}"
        "layout.column_backings[column] ="
        "layout.line_bounds[index] =")
    require_token("${_layout_body}" "${_token}"
        "Task 7 responsive overlay layout contract is missing")
endforeach()
reject_dead_branch("${_layout_code}" "Task 7 overlay layout")

extract_cpp_function_body("${_overlay_source}"
    "void DebugOverlayRenderer::draw(" _draw_body _draw_code)
foreach(_token IN ITEMS
        "DrawRectangleRec(layout.column_backings[column], layout.backing_color)"
        "if (!shared_ui_font_ready || !IsFontValid(shared_ui_font)) return"
        "draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data()")
    require_token("${_draw_body}" "${_token}"
        "Task 7 renderer must draw opaque columns then shared-font text")
endforeach()
string(FIND "${_draw_body}" "DrawRectangleRec(" _backing_index)
string(FIND "${_draw_body}" "draw_crisp_ui_text(" _text_index)
if(_backing_index GREATER _text_index)
    message(FATAL_ERROR "Task 7 column backing must be drawn before text")
endif()
foreach(_forbidden IN ITEMS "DrawText(" "DrawTextEx(" "TextFormat("
        "GetFontDefault(" "LoadFont")
    string(FIND "${_draw_code}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Task 7 renderer uses a forbidden text/font path: ${_forbidden}")
    endif()
endforeach()
reject_dead_branch("${_draw_code}" "Task 7 renderer")

extract_cpp_function_body("${_combat_source}"
    "GroundLootView CombatRenderer::draw(" _combat_draw_body _combat_draw_code)
require_token("${_combat_draw_body}"
    "debug_overlay_.draw(world_ui_font_, world_ui_font_ready_,"
    "Task 7 CombatRenderer must pass its borrowed SharedUiFont")
reject_dead_branch("${_combat_draw_code}" "Task 7 CombatRenderer overlay path")

extract_cpp_function_body("${_debug_source}"
    "void draw_projected_aabb(" _aabb_body _aabb_code)
foreach(_token IN ITEMS
        "shared_ui_font_ready && IsFontValid(shared_ui_font)"
        "draw_crisp_ui_text(shared_ui_font, label")
    require_token("${_aabb_body}" "${_token}"
        "Task 7 world volume labels must use the borrowed SharedUiFont")
endforeach()
foreach(_forbidden IN ITEMS "DrawText(" "DrawTextEx(" "GetFontDefault(")
    string(FIND "${_aabb_code}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Task 7 world volume label uses a default text path: ${_forbidden}")
    endif()
endforeach()
reject_dead_branch("${_aabb_code}" "Task 7 world volume label draw")

extract_cpp_function_body("${_debug_source}"
    "void CombatRenderer::draw_debug_world_volumes(" _world_body _world_code)
foreach(_token IN ITEMS
        "debug_world_volume_label(DebugWorldVolumeKind::assist)"
        "debug_world_volume_label(DebugWorldVolumeKind::attack)"
        "debug_world_volume_label(DebugWorldVolumeKind::hurt)"
        "world_ui_font_, world_ui_font_ready_")
    require_token("${_world_body}" "${_token}"
        "Task 7 world debug label mapping is missing")
endforeach()
reject_dead_branch("${_world_code}" "Task 7 world debug labels")

extract_cpp_function_body("${_view_math_source}"
    "MonsterVisual monster_visual(" _monster_body _monster_code)
require_token("${_monster_body}" "ui_monster_name(id)"
    "Task 7 monster role labels must use ui_monster_name")
string(REGEX MATCHALL "ui_monster_name\\(id\\)" _monster_name_calls
    "${_monster_code}")
list(LENGTH _monster_name_calls _monster_name_call_count)
if(NOT _monster_name_call_count EQUAL 8)
    message(FATAL_ERROR
        "Task 7 must localize every concrete monster role label")
endif()

extract_cpp_function_body("${_view_math_source}"
    "const char* monster_phase_label(" _phase_body _phase_code)
foreach(_token IN ITEMS "待机" "移动" "预警" "生效" "恢复" "冷却" "已击败")
    require_token("${_phase_body}" "${_token}"
        "Task 7 monster phase mapping is incomplete")
endforeach()
reject_dead_branch("${_phase_code}" "Task 7 monster phase mapping")

extract_cpp_function_body("${_view_math_source}"
    "const char* affix_tier_text(" _tier_body _tier_code)
foreach(_token IN ITEMS
        "case combat::MonsterAffixTier::m1: return u8\"词缀Ⅰ\""
        "case combat::MonsterAffixTier::m2: return u8\"词缀Ⅱ\""
        "case combat::MonsterAffixTier::m3: return u8\"词缀Ⅲ\"")
    require_token("${_tier_body}" "${_token}"
        "Task 7 affix tier mapping is incomplete")
endforeach()
reject_dead_branch("${_tier_code}" "Task 7 affix tier mapping")

extract_cpp_function_body("${_actor_source}"
    "void draw_effects(" _effects_body _effects_code)
require_token("${_effects_body}" "u8\"已击败\""
    "Task 7 defeat marker must be Chinese")
extract_cpp_function_body("${_actor_source}"
    "void draw_monster_presentation(" _presentation_body _presentation_code)
require_token("${_presentation_body}" "monster_phase_label(monster.ai_phase)"
    "Task 7 monster presentation must consume the Chinese phase mapping")
foreach(_token IN ITEMS
        "std::array<char, 64U> affix_text{}"
        "ui_utf8_format(affix_text.data(), affix_text.size(), u8\"%s %s\""
        "draw_scene_label(hud_font, hud_font_ready, affix_text.data()")
    require_token("${_presentation_body}" "${_token}"
        "Task 7 affix labels must use a fixed UTF-8 stack buffer")
endforeach()
string(FIND "${_presentation_code}" "TextFormat(" _affix_textformat)
if(NOT _affix_textformat EQUAL -1)
    message(FATAL_ERROR
        "Task 7 monster presentation must not use TextFormat")
endif()
reject_dead_branch("${_effects_code}" "Task 7 defeat marker")
reject_dead_branch("${_presentation_code}" "Task 7 monster presentation")

message(STATUS "Task 7 debug overlay source guard accepted")
