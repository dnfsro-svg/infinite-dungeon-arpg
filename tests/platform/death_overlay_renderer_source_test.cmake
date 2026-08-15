cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

foreach(_name IN ITEMS RENDERER_SOURCE RENDERER_HEADER VIEW_SOURCE VIEW_HEADER
        DUNGEON_SOURCE DUNGEON_HEADER COMBAT_RENDERER_SOURCE)
    if(NOT DEFINED ${_name})
        if(_name STREQUAL "RENDERER_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/death_overlay_renderer.cpp")
        elseif(_name STREQUAL "RENDERER_HEADER")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/death_overlay_renderer.hpp")
        elseif(_name STREQUAL "VIEW_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.cpp")
        elseif(_name STREQUAL "VIEW_HEADER")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.hpp")
        elseif(_name STREQUAL "DUNGEON_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/dungeon_view_math.cpp")
        elseif(_name STREQUAL "DUNGEON_HEADER")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/dungeon_view_math.hpp")
        else()
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${_name}}")
        message(FATAL_ERROR "T5 source is missing: ${_name}=${${_name}}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")

function(require_token TEXT TOKEN MESSAGE_TEXT)
    string(FIND "${TEXT}" "${TOKEN}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: ${TOKEN}")
    endif()
endfunction()

function(reject_token TEXT TOKEN MESSAGE_TEXT)
    string(FIND "${TEXT}" "${TOKEN}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: ${TOKEN}")
    endif()
endfunction()

function(remove_first_token TEXT TOKEN OUTPUT_VARIABLE)
    string(FIND "${TEXT}" "${TOKEN}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "T5 guard internal anchor is missing: ${TOKEN}")
    endif()
    string(LENGTH "${TOKEN}" _token_length)
    math(EXPR _after_index "${_found} + ${_token_length}")
    string(SUBSTRING "${TEXT}" 0 ${_found} _before)
    string(SUBSTRING "${TEXT}" ${_after_index} -1 _after)
    set(${OUTPUT_VARIABLE} "${_before}${_after}" PARENT_SCOPE)
endfunction()

function(reject_dead_branch CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES
            "if[ ]*(constexpr[ ]*)?\\([ ]*(false|0|0[ ]*==[ ]*1|1[ ]*==[ ]*0)[ ]*\\)"
            OR "${CODE}" MATCHES "#[ ]*if[ ]+0([^0-9]|$)"
            OR "${CODE}" MATCHES
                "if[ \\t\\r\\n]*\\([^\\)]*(false[ \\t]*&&|![ \\t]*true|0[ \\t]*!=[ \\t]*0|1[ \\t]*!=[ \\t]*1)[^\\)]*\\)"
            OR "${CODE}" MATCHES
                "if[ \\t\\r\\n]*\\([^\\{]*&&[ \\t\\r\\n]*false[ \\t\\r\\n]*\\)[ \\t\\r\\n]*\\{")
        message(FATAL_ERROR "${MESSAGE_TEXT} cannot live in a dead branch")
    endif()
endfunction()

file(READ "${RENDERER_SOURCE}" _renderer_source)
file(READ "${RENDERER_HEADER}" _renderer_header)
file(READ "${VIEW_SOURCE}" _view_source)
file(READ "${VIEW_HEADER}" _view_header)
file(READ "${DUNGEON_SOURCE}" _dungeon_source)
file(READ "${DUNGEON_HEADER}" _dungeon_header)
file(READ "${COMBAT_RENDERER_SOURCE}" _combat_renderer_source)

normalize_cpp_source("${_renderer_source}" _renderer_normalized _renderer_code)
normalize_cpp_source("${_renderer_header}" _renderer_header_normalized
    _renderer_header_code)
normalize_cpp_source("${_view_source}" _view_normalized _view_code)
normalize_cpp_source("${_view_header}" _view_header_normalized _view_header_code)
normalize_cpp_source("${_dungeon_source}" _dungeon_normalized _dungeon_code)
normalize_cpp_source("${_dungeon_header}" _dungeon_header_normalized
    _dungeon_header_code)
normalize_cpp_source("${_combat_renderer_source}" _combat_normalized _combat_code)

string(REGEX REPLACE "[ \t\r\n]+" "" _renderer_header_compact
    "${_renderer_header_normalized}")
string(REGEX REPLACE "[ \t\r\n]+" "" _view_header_compact
    "${_view_header_normalized}")
string(REGEX REPLACE "[ \t\r\n]+" "" _dungeon_header_compact
    "${_dungeon_header_normalized}")

foreach(_token IN ITEMS
        "enumclassDeathOverlayTextDrawMode{unavailable,shared_ui_font,};"
        "structDeathOverlayTextStylefinal{"
        "Colorbacking{5,9,16,255};"
        "Colorprimary{248,246,238,255};"
        "Colordanger{255,152,152,255};"
        "Colorwarning{255,210,118,255};"
        "Colorerror{255,152,152,255};"
        "death_overlay_text_style()noexcept"
        "death_overlay_text_draw_mode(boolshared_font_ready)noexcept"
        "constchar*continue_binding_label)constnoexcept")
    require_token("${_renderer_header_compact}" "${_token}"
        "T5 renderer public fail-closed/style contract is missing")
endforeach()

require_token("${_view_header_compact}"
    "build_death_overlay_view(constdungeon::DungeonSnapshot&snapshot,constchar*continue_binding_label"
    "T5 Chinese view must consume a dynamic continue binding")
require_token("${_dungeon_header_compact}"
    "constchar*abyss_rule_label(abyss::AbyssRuleIdrule)noexcept;"
    "T5 abyss-rule label must be a public read-only noexcept API")

foreach(_forbidden IN ITEMS
        "GetFontDefault(" "DrawText(" "DrawTextEx(" "LoadFontEx("
        "build_death_overlay_ascii_view(")
    reject_token("${_renderer_code}" "${_forbidden}"
        "T5 formal renderer must fail closed without ASCII/default-font fallback")
endforeach()

extract_cpp_function_body("${_view_source}"
    "const char* death_monster_name(" _monster_body _monster_code)
require_token("${_monster_body}"
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "T5 monster attribution must use ui_text_catalog")
reject_token("${_monster_body}" "std::array"
    "T5 death view must not copy the monster-name table")
reject_token("${_monster_body}" "switch"
    "T5 death view must not copy the monster-name table")
reject_dead_branch("${_monster_code}" "T5 monster catalog consumer")

extract_cpp_function_body("${_view_source}"
    "const char* death_affix_name(" _affix_body _affix_code)
require_token("${_affix_body}"
    "return ui_monster_affix_name(static_cast<combat::MonsterAffixId>(id));"
    "T5 affix attribution must use ui_text_catalog")
reject_token("${_affix_body}" "std::array"
    "T5 death view must not copy the monster-affix table")
reject_token("${_affix_body}" "switch"
    "T5 death view must not copy the monster-affix table")
reject_dead_branch("${_affix_code}" "T5 affix catalog consumer")

extract_cpp_function_body("${_view_source}"
    "const char* death_abyss_rule_name(" _abyss_body _abyss_code)
require_token("${_abyss_body}"
    "return abyss_rule_label(static_cast<abyss::AbyssRuleId>(id));"
    "T5 death view must reuse dungeon_view_math abyss labels")
reject_token("${_abyss_body}" "switch"
    "T5 death view must not copy the abyss-rule table")
reject_token("${_abyss_body}" "std::array"
    "T5 death view must not copy the abyss-rule table")
reject_dead_branch("${_abyss_code}" "T5 abyss-rule catalog consumer")

extract_cpp_function_body("${_view_source}"
    "void add_source_line(" _source_body _source_code)
extract_cpp_function_body("${_view_source}"
    "DeathOverlayView build_death_overlay_view(" _view_body _view_body_code)
extract_cpp_function_body("${_view_source}"
    "const char* ecology_name(" _ecology_body _ecology_code)
extract_cpp_function_body("${_view_source}"
    "const char* damage_name(" _damage_body _damage_code)
extract_cpp_function_body("${_view_source}"
    "const char* hazard_name(" _hazard_body _hazard_code)
foreach(_token IN ITEMS
        "continue_binding_label" "ui_utf8_format(" "u8\"死亡回顾\""
        "u8\"正在记录死亡｜ESC：退出游戏\""
        "u8\"保存失败，%s：重试｜ESC：退出游戏\""
        "u8\"%s：继续｜ESC：退出游戏\"")
    require_token("${_view_body}" "${_token}"
        "T5 formal Chinese view is incomplete")
endforeach()
foreach(_token IN ITEMS
        "death_monster_name(" "death_affix_name(" "death_abyss_rule_name("
        "u8\"怪物攻击\"" "u8\"投射物\"" "u8\"地面危险\""
        "u8\"怪物词缀\"" "u8\"深渊环境\"" "u8\"致死来源：")
    require_token("${_source_body}" "${_token}"
        "T5 source attribution path is incomplete")
endforeach()
require_token("${_view_source}" "ui_utf8_copy("
    "T5 fixed text buffers must use the UTF-8-safe copy helper")
reject_dead_branch("${_source_code}" "T5 source attribution")
reject_dead_branch("${_view_body_code}" "T5 formal Chinese view")
reject_dead_branch("${_ecology_code}" "T5 death ecology mapping")
reject_dead_branch("${_damage_code}" "T5 death damage mapping")
reject_dead_branch("${_hazard_code}" "T5 death hazard mapping")

set(_formal_view_text
    "${_ecology_body}${_damage_body}${_hazard_body}${_source_body}${_view_body}")
foreach(_english IN ITEMS
        "DEATH RECAP" "Depth " "Room " "Source:" "Last hit"
        "Recent 5s" "Defense at death" "Save failed" "Continue"
        "MONSTER ATTACK" "PROJECTILE" "GROUND HAZARD"
        "MONSTER AFFIX" "ABYSS ENVIRONMENT" "PHYSICAL DAMAGE"
        "FIRE DAMAGE" "WATER DAMAGE" "LIGHTNING DAMAGE" "CHAOS DAMAGE")
    reject_token("${_formal_view_text}" "${_english}"
        "T5 player-facing English remains active in the formal view")
endforeach()

extract_cpp_function_body("${_dungeon_source}"
    "const char* abyss_rule_label(" _dungeon_abyss_body _dungeon_abyss_code)
foreach(_token IN ITEMS
        "AbyssRuleId::thunderstorm" "u8\"雷暴\""
        "AbyssRuleId::life_sacrifice" "u8\"生命献祭\""
        "AbyssRuleId::none" "u8\"无规则\"")
    require_token("${_dungeon_abyss_body}" "${_token}"
        "T5 public abyss-rule label mapping is incomplete")
endforeach()
reject_dead_branch("${_dungeon_abyss_code}" "T5 public abyss-rule mapping")

extract_cpp_function_body("${_renderer_source}"
    "void draw_text(" _draw_text_body _draw_text_code)
foreach(_token IN ITEMS
        "death_overlay_text_style()" "style.backing.a != 255U"
        "ui_luma_contrast_ratio(color, style.backing) < 4.5F"
        "DrawRectangleRec(rectangle(bounds), style.backing)"
        "fit_centered_ui_text(" "layout.position")
    require_token("${_draw_text_body}" "${_token}"
        "T5 text helper must use opaque high-contrast fitted backing")
endforeach()
reject_token("${_draw_text_body}" "MeasureTextEx("
    "T5 text backing/position must not be rebuilt from raw text width")
reject_dead_branch("${_draw_text_code}" "T5 formal text helper")

extract_cpp_function_body("${_renderer_source}"
    "DeathOverlayTextDrawMode death_overlay_text_draw_mode("
    _mode_body _mode_code)
require_token("${_mode_body}" "DeathOverlayTextDrawMode::shared_ui_font"
    "T5 draw mode must select the shared UI font")
require_token("${_mode_body}" "DeathOverlayTextDrawMode::unavailable"
    "T5 draw mode must fail closed")
reject_dead_branch("${_mode_code}" "T5 death-overlay draw mode")

extract_cpp_function_body("${_renderer_source}"
    "void DeathOverlayRenderer::draw(" _draw_body _draw_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _draw_body_compact "${_draw_body}")
if("${_draw_code}" MATCHES
        "(^|[=({,;:])[ ]*\\[[^\\]]*\\][ ]*(<|\\(|mutable|constexpr|consteval|noexcept|->|\\{)")
    message(FATAL_ERROR
        "T5 renderer live draw path must not be hidden in a lambda")
endif()
require_token("${_draw_body_compact}"
    "if(death_overlay_text_draw_mode(font_ready_)!=DeathOverlayTextDrawMode::shared_ui_font||!IsFontValid(font_)){return;}"
    "T5 renderer must immediately return when the shared font is unavailable")
require_token("${_draw_body_compact}"
    "if(death_overlay_text_draw_mode(font_ready_)!=DeathOverlayTextDrawMode::shared_ui_font||!IsFontValid(font_)){return;}constDeathOverlayViewview=build_death_overlay_view(snapshot,continue_binding_label);"
    "T5 renderer must enter the live Chinese view immediately after the font gate")
set(_draw_live_path_previous -1)
foreach(_draw_live_path_token IN ITEMS
        "constDeathOverlayViewview=build_death_overlay_view(snapshot,continue_binding_label);"
        "constDeathOverlayMaterialPlanplan=death_overlay_material_plan(view.visible);"
        "if(!plan.visible)return;"
        "constDeathOverlayLayoutlayout=death_overlay_layout("
        "draw_text(font_,view.title.data(),layout.title"
        "for(std::size_tindex=0U;index<view.line_count;++index)"
        "draw_text(font_,view.lines[index].text.data(),bounds"
        "draw_text(font_,view.prompt.data(),layout.prompt")
    string(FIND "${_draw_body_compact}" "${_draw_live_path_token}"
        _draw_live_path_index)
    if(_draw_live_path_index LESS 0
            OR NOT _draw_live_path_previous LESS _draw_live_path_index)
        message(FATAL_ERROR
            "T5 renderer live draw path is missing or out of order: ${_draw_live_path_token}")
    endif()
    set(_draw_live_path_previous ${_draw_live_path_index})
endforeach()

set(_draw_return_probe "${_draw_body_compact}")
set(_allowed_font_gate
    "if(death_overlay_text_draw_mode(font_ready_)!=DeathOverlayTextDrawMode::shared_ui_font||!IsFontValid(font_)){return;}")
set(_allowed_visible_gate "if(!plan.visible)return;")
remove_first_token("${_draw_return_probe}" "${_allowed_font_gate}"
    _draw_return_probe)
remove_first_token("${_draw_return_probe}" "${_allowed_visible_gate}"
    _draw_return_probe)
reject_token("${_draw_return_probe}" "return;"
    "T5 renderer draw path contains an unapproved early return")
foreach(_token IN ITEMS
        "death_overlay_text_draw_mode(font_ready_)"
        "DeathOverlayTextDrawMode::shared_ui_font" "!IsFontValid(font_)"
        "build_death_overlay_view(snapshot, continue_binding_label)"
        "draw_text(font_, view.title.data(), layout.title"
        "style.danger, true)" "for (std::size_t index = 0U;"
        "index < view.line_count; ++index)"
        "draw_text(font_, view.lines[index].text.data(), bounds"
        "draw_text(font_, view.prompt.data(), layout.prompt"
        "snapshot.death->continue_failed ? style.error : style.warning, true)")
    require_token("${_draw_body}" "${_token}"
        "T5 formal renderer fail-closed/centering path is incomplete")
endforeach()
string(FIND "${_draw_body}" "death_overlay_text_draw_mode(font_ready_)"
    _fail_closed_index)
string(FIND "${_draw_body}"
    "build_death_overlay_view(snapshot, continue_binding_label)" _view_index)
if(_fail_closed_index LESS 0 OR _view_index LESS 0
        OR NOT _fail_closed_index LESS _view_index)
    message(FATAL_ERROR "T5 renderer must fail closed before building/drawing text")
endif()
reject_dead_branch("${_draw_code}" "T5 formal renderer")

extract_cpp_function_body("${_combat_renderer_source}"
    "GroundLootView CombatRenderer::draw(" _combat_draw_body _combat_draw_code)
require_token("${_combat_draw_body}"
    "death_overlay_.draw(current_hud, material_pack_, stable_key_label(settings::StableKey::e))"
    "T5 real consumer must use the fixed E death action binding")
reject_token("${_combat_draw_body}"
    "death_overlay_.draw(current_hud, material_pack_, interact_binding_label)"
    "T5 death action must not inherit the room interaction binding")
reject_dead_branch("${_combat_draw_code}" "T5 fixed death binding consumer")

message(STATUS "T5 death-overlay renderer source guard accepted")
