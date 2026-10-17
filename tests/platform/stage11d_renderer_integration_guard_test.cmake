if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(_combat_header_path "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.hpp")
set(_combat_source_path "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_room_source_path "${SOURCE_ROOT}/src/platform/raylib/room_renderer.cpp")
set(_hud_source_path "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp")

foreach(_required_path IN ITEMS
        "${_combat_header_path}"
        "${_combat_source_path}"
        "${_room_source_path}"
        "${_hud_source_path}")
    if(NOT EXISTS "${_required_path}")
        message(FATAL_ERROR "Stage11D renderer integration guard input missing: ${_required_path}")
    endif()
endforeach()

file(READ "${_combat_header_path}" _combat_header)
file(READ "${_combat_source_path}" _combat_source)
file(READ "${_room_source_path}" _room_source)
file(READ "${_hud_source_path}" _hud_source)

set(_legacy_tokens
    "GroundLootRenderConsumers"
    "make_ground_loot_render_consumers"
    "\.hud_view"
    "\.room_view")
foreach(_legacy_token IN LISTS _legacy_tokens)
    if(_combat_header MATCHES "${_legacy_token}" OR _combat_source MATCHES "${_legacy_token}")
        message(FATAL_ERROR "Stage11D renderer integration must not restore legacy wrapper token: ${_legacy_token}")
    endif()
endforeach()

function(stage11d_reject_consumer_rebuild SOURCE_TEXT CONSUMER_NAME)
    set(_rebuild_tokens
        "build_ground_loot_view[ \t\r\n]*\\("
        "ground_loot_visible[ \t\r\n]*\\("
        "make_combat_render_plan[ \t\r\n]*\\("
        "LootFilterMode"
        "loot_filter_mode")
    foreach(_token IN LISTS _rebuild_tokens)
        if("${SOURCE_TEXT}" MATCHES "${_token}")
            message(FATAL_ERROR
                "Stage11D ${CONSUMER_NAME} renderer must not rebuild or independently filter GroundLootView")
        endif()
    endforeach()
endfunction()

set(_icons_only_marker
    "GroundLootView CombatRenderer::draw_ground_loot_icons_only(")
set(_room_consumer_marker "void CombatRenderer::draw_room(")
string(REGEX MATCHALL
    "GroundLootView CombatRenderer::draw_ground_loot_icons_only[ \\t\\r\\n]*\\("
    _icons_only_markers "${_room_source}")
string(REGEX MATCHALL "void CombatRenderer::draw_room[ \\t\\r\\n]*\\("
    _room_consumer_markers "${_room_source}")
list(LENGTH _icons_only_markers _icons_only_marker_count)
list(LENGTH _room_consumer_markers _room_consumer_marker_count)
string(FIND "${_room_source}" "${_icons_only_marker}" _icons_only_start)
string(FIND "${_room_source}" "${_room_consumer_marker}" _room_consumer_start)
if(NOT _icons_only_marker_count EQUAL 1
        OR NOT _room_consumer_marker_count EQUAL 1
        OR _icons_only_start EQUAL -1
        OR _room_consumer_start EQUAL -1
        OR NOT _icons_only_start LESS _room_consumer_start)
    message(FATAL_ERROR
        "Stage11D renderer integration cannot isolate the Stage12 icons-only exception")
endif()
string(SUBSTRING "${_room_source}" 0 ${_icons_only_start} _room_prefix)
math(EXPR _icons_only_length
    "${_room_consumer_start} - ${_icons_only_start}")
string(SUBSTRING "${_room_source}" ${_icons_only_start}
    ${_icons_only_length} _icons_only_source)
string(SUBSTRING "${_room_source}" ${_room_consumer_start} -1 _room_tail)
set(_room_without_icons_only "${_room_prefix}\n${_room_tail}")

stage11d_reject_consumer_rebuild("${_room_without_icons_only}" "room")
stage11d_reject_consumer_rebuild("${_hud_source}" "HUD")

string(REGEX MATCHALL "build_ground_loot_view[ \\t\\r\\n]*\\("
    _icons_ground_builders "${_icons_only_source}")
string(REGEX MATCHALL "build_material_loot_view[ \\t\\r\\n]*\\("
    _icons_material_builders "${_icons_only_source}")
string(REGEX MATCHALL "make_combat_render_plan[ \\t\\r\\n]*\\("
    _icons_render_plans "${_icons_only_source}")
string(REGEX MATCHALL "ground_loot_visible[ \\t\\r\\n]*\\("
    _icons_visibility_filters "${_icons_only_source}")
string(REGEX MATCHALL "LootFilterMode" _icons_filter_types
    "${_icons_only_source}")
string(REGEX MATCHALL "loot_filter_mode" _icons_filter_reads
    "${_icons_only_source}")
list(LENGTH _icons_ground_builders _icons_ground_builder_count)
list(LENGTH _icons_material_builders _icons_material_builder_count)
list(LENGTH _icons_render_plans _icons_render_plan_count)
list(LENGTH _icons_visibility_filters _icons_visibility_filter_count)
list(LENGTH _icons_filter_types _icons_filter_type_count)
list(LENGTH _icons_filter_reads _icons_filter_read_count)
if(NOT _icons_ground_builder_count EQUAL 1
        OR NOT _icons_material_builder_count EQUAL 1
        OR NOT _icons_render_plan_count EQUAL 0
        OR NOT _icons_visibility_filter_count EQUAL 0
        OR NOT _icons_filter_type_count EQUAL 0
        OR NOT _icons_filter_read_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D icons-only exception must build one shared ground/material view without a second render plan")
endif()
string(REGEX REPLACE "[ \t\r\n]+" "" _icons_only_normalized
    "${_icons_only_source}")
foreach(_required_icons_binding IN ITEMS
        "constGroundLootViewground_loot=build_ground_loot_view(snapshot,loot_filter_mode_,width,height)"
        "constMaterialLootViewmaterial_loot=build_material_loot_view(snapshot,width,height)"
        "draw_ground_materials(snapshot,material_loot,material_pack_,width,height)"
        "draw_ground_items(snapshot,ground_loot,material_pack_,width,height)"
        "returnground_loot")
    string(FIND "${_icons_only_normalized}"
        "${_required_icons_binding}" _icons_binding_found)
    if(_icons_binding_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D icons-only exception must preserve the shared view binding: ${_required_icons_binding}")
    endif()
endforeach()

string(FIND "${_combat_source}" "GroundLootView CombatRenderer::draw(" _draw_start)
if(_draw_start EQUAL -1)
    message(FATAL_ERROR "Stage11D renderer integration requires CombatRenderer::draw")
endif()
string(SUBSTRING "${_combat_source}" ${_draw_start} -1 _draw_source)

# Normalize formatting only. Variable names remain intact and are recovered below.
string(REGEX REPLACE "[ \t\r\n]+" "" _draw_normalized "${_draw_source}")

string(REGEX MATCHALL "make_combat_render_plan\\(" _factory_calls "${_draw_normalized}")
list(LENGTH _factory_calls _factory_call_count)
if(NOT _factory_call_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D render-plan factory must be called exactly once inside CombatRenderer::draw; found ${_factory_call_count}")
endif()

string(REGEX MATCH
    "constCombatRenderPlan([A-Za-z_][A-Za-z0-9_]*)=make_combat_render_plan\\("
    _plan_declaration
    "${_draw_normalized}")
if(NOT _plan_declaration)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must store its single factory result in a const CombatRenderPlan")
endif()
set(_plan_variable "${CMAKE_MATCH_1}")
set(_canonical_ground_loot "${_plan_variable}.ground_loot")

string(REGEX MATCHALL "build_ground_loot_view[ \t\r\n]*\\(" _builder_calls "${_combat_source}")
list(LENGTH _builder_calls _builder_call_count)
if(NOT _builder_call_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderPlan factory must build GroundLootView exactly once; found ${_builder_call_count}")
endif()

# A const-reference alias is semantically the same view and avoids another container copy.
set(_ground_loot_alias "")
string(REGEX MATCH
    "const(GroundLootView|auto)&([A-Za-z_][A-Za-z0-9_]*)=${_plan_variable}\\.ground_loot;"
    _alias_declaration
    "${_draw_normalized}")
if(_alias_declaration)
    set(_ground_loot_alias "${CMAKE_MATCH_2}")
endif()

set(_room_consumer_pattern
    "draw_room\\([^,;]+,([A-Za-z_][A-Za-z0-9_.]*)(,[^;]+)?\\)")
string(REGEX MATCHALL "${_room_consumer_pattern}" _room_consumer_calls "${_draw_normalized}")
list(LENGTH _room_consumer_calls _room_consumer_count)
if(NOT _room_consumer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must have exactly one room GroundLootView consumer")
endif()
string(REGEX MATCH "${_room_consumer_pattern}" _room_consumer "${_draw_normalized}")
set(_room_argument "${CMAKE_MATCH_1}")

set(_hud_consumer_pattern
    "hud_renderer_\\.draw_ground_loot\\(([A-Za-z_][A-Za-z0-9_.]*)\\)")
string(REGEX MATCHALL "${_hud_consumer_pattern}" _hud_consumer_calls "${_draw_normalized}")
list(LENGTH _hud_consumer_calls _hud_consumer_count)
if(NOT _hud_consumer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must have exactly one HUD GroundLootView consumer")
endif()
string(REGEX MATCH "${_hud_consumer_pattern}" _hud_consumer "${_draw_normalized}")
set(_hud_argument "${CMAKE_MATCH_1}")

foreach(_consumer IN ITEMS room hud)
    set(_argument "${_${_consumer}_argument}")
    if("${_argument}" STREQUAL "${_canonical_ground_loot}")
        continue()
    endif()
    if(NOT "${_ground_loot_alias}" STREQUAL "" AND
       "${_argument}" STREQUAL "${_ground_loot_alias}")
        continue()
    endif()
    message(FATAL_ERROR
        "Stage11D consumer divergence: room and HUD consumers must use the same GroundLootView from CombatRenderPlan (${_consumer} uses ${_argument})")
endforeach()

string(REGEX MATCHALL "return[A-Za-z_]" _return_calls "${_draw_normalized}")
list(LENGTH _return_calls _return_count)
if(NOT _return_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must return its shared GroundLootView exactly once")
endif()
string(FIND "${_draw_normalized}"
    "return${_canonical_ground_loot}" _canonical_return)
set(_alias_return -1)
if(NOT "${_ground_loot_alias}" STREQUAL "")
    string(FIND "${_draw_normalized}"
        "return${_ground_loot_alias}" _alias_return)
endif()
if(_canonical_return EQUAL -1 AND _alias_return EQUAL -1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must return the same GroundLootView consumed by room and HUD")
endif()

message(STATUS
    "[stage11d-renderer-integration] factory_calls=${_factory_call_count} builder_calls=${_builder_call_count} room_consumers=${_room_consumer_count} hud_consumers=${_hud_consumer_count} shared_returns=${_return_count}")
