cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

foreach(_name IN ITEMS GROUND_SOURCE PICKUP_SOURCE MATERIAL_SOURCE BAG_SOURCE
        INVENTORY_SOURCE HUD_SOURCE CATALOG_SOURCE)
    if(NOT DEFINED ${_name})
        if(_name STREQUAL "GROUND_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/ground_loot_view.cpp")
        elseif(_name STREQUAL "PICKUP_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/loot_pickup_feedback.cpp")
        elseif(_name STREQUAL "MATERIAL_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/material_loot_view.cpp")
        elseif(_name STREQUAL "BAG_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.cpp")
        elseif(_name STREQUAL "INVENTORY_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp")
        elseif(_name STREQUAL "HUD_SOURCE")
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp")
        else()
            set(${_name} "${SOURCE_ROOT}/src/platform/raylib/ui_text_catalog.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${_name}}")
        message(FATAL_ERROR "T3 source is missing: ${_name}=${${_name}}")
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

function(reject_dead_branch CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES
            "if[ ]*(constexpr[ ]*)?\\([ ]*(false|0|0[ ]*==[ ]*1|1[ ]*==[ ]*0)[ ]*\\)"
            OR "${CODE}" MATCHES "#[ ]*if[ ]+0([^0-9]|$)"
            OR "${CODE}" MATCHES
                "if[ \\t\\r\\n]*\\([^\\{]*&&[ \\t\\r\\n]*false[ \\t\\r\\n]*\\)[ \\t\\r\\n]*\\{")
        message(FATAL_ERROR "${MESSAGE_TEXT} cannot live in a dead branch")
    endif()
endfunction()

file(READ "${GROUND_SOURCE}" _ground_source)
file(READ "${PICKUP_SOURCE}" _pickup_source)
file(READ "${MATERIAL_SOURCE}" _material_source)
file(READ "${BAG_SOURCE}" _bag_source)
file(READ "${INVENTORY_SOURCE}" _inventory_source)
file(READ "${HUD_SOURCE}" _hud_source)
file(READ "${CATALOG_SOURCE}" _catalog_source)

foreach(_source IN ITEMS _ground_source _pickup_source
        _material_source _bag_source _catalog_source)
    reject_token("${${_source}}" "蜕变石"
        "T3 must remove the unreachable legacy material spelling")
endforeach()
reject_token("${_catalog_source}" "蜕"
    "T3 catalog glyph corpus must release the legacy codepoint")
require_token("${_catalog_source}" "材料袋"
    "T3 catalog must cover the reachable material-bag title")

foreach(_token IN ITEMS "ui_item_rarity_name(item.rarity)"
        "ui_item_base_name(item.base_id)" "物品等级 %u")
    require_token("${_ground_source}" "${_token}"
        "T3 ground equipment text must consume the Chinese catalog")
endforeach()
foreach(_token IN ITEMS "base->name" "i%u")
    reject_token("${_ground_source}" "${_token}"
        "T3 ground equipment text must not consume core English/iLvl text")
endforeach()

foreach(_token IN ITEMS "ui_item_rarity_name(receipt.rarity)"
        "ui_item_base_name(receipt.base_id)" "物品等级 %u")
    require_token("${_pickup_source}" "${_token}"
        "T3 pickup feedback must consume the Chinese catalog")
endforeach()
foreach(_token IN ITEMS "base->name" "i%u")
    reject_token("${_pickup_source}" "${_token}"
        "T3 pickup feedback must not consume core English/iLvl text")
endforeach()

require_token("${_material_source}"
    "std::string_view material_label(items::MaterialId id) noexcept"
    "T3 material-label consumer must remain reachable")
require_token("${_material_source}" "return ui_material_name(id);"
    "T3 material labels must delegate to the single catalog")

foreach(_token IN ITEMS "format_material_pickup_text("
        "material_feedback.text.truncated = false"
        "material_feedback.text.bytes.back() = '\\0'")
    require_token("${_material_source}" "${_token}"
        "T3 material aggregation must publish a complete fixed UTF-8 buffer")
endforeach()
foreach(_token IN ITEMS "ui_utf8_format(" "ui_utf8_copy("
        "remaining_material_kind_count(" "、等%zu种")
    require_token("${_material_source}" "${_token}"
        "T3 material aggregation must append only complete tokens")
endforeach()

foreach(_english IN ITEMS "\"MATERIAL BAG\"" "\"Reinf. Stone\""
        "\"Invalid\"" "\"Damage Core\"" "\"Defense Core\""
        "\"Speed Core\"" "\"Element Core\""
        "\"DANGER: FAILURE DESTROYS EQUIPMENT\""
        "\"Use one Reinforcement Stone?\"" "\"CONFIRM\"" "\"CANCEL\"")
    reject_token("${_bag_source}" "${_english}"
        "T3 material bag still exposes player-facing English")
endforeach()
reject_token("${_bag_source}" "GetFontDefault("
    "T3 material bag must fail closed on the shared font")

require_token("${_bag_source}"
    "const char* material_bag_display_name(items::MaterialId id) noexcept"
    "T3 material-bag name consumer must remain reachable")
require_token("${_bag_source}" "return ui_material_name(id);"
    "T3 material bag names must consume the catalog")

foreach(_token IN ITEMS "if (!ready || !IsFontValid(font)) return"
        "opaque_backing.a != 255U" "fit_centered_ui_text("
        "layout.position" "ui_luma_contrast_ratio(color, opaque_backing)")
    require_token("${_bag_source}" "${_token}"
        "T3 material bag text helper must center on an opaque backing")
endforeach()

foreach(_token IN ITEMS
        "constexpr Color kMaterialTextBacking{5, 9, 16, 255};"
        "constexpr Color kDangerBacking{29, 15, 18, 255};"
        "constexpr Color kConfirmBacking{74, 24, 27, 255};"
        "constexpr Color kCancelBacking{30, 42, 58, 255};"
        "DrawRectangleRec(layout.panel, kMaterialTextBacking)"
        "DrawRectangleRec(layout.confirm, kConfirmBacking)"
        "DrawRectangleRec(layout.cancel, kCancelBacking)")
    require_token("${_bag_source}" "${_token}"
        "T3 material bag and reinforcement backings must stay opaque and contrast-safe")
endforeach()

foreach(_token IN ITEMS "u8\"材料袋\"" "material_bag_display_name(id)"
        "u8\"伤害核心\"" "u8\"防御核心\""
        "u8\"速度核心\"" "u8\"元素核心\""
        "draw_material_text(" "text_container")
    require_token("${_bag_source}" "${_token}"
        "T3 material bag live draw path is incomplete")
endforeach()

foreach(_token IN ITEMS "u8\"危险：强化失败，装备丢失\""
        "u8\"确认强化石？\"" "u8\"确认\"" "u8\"取消\""
        "DrawRectangleRec(layout.panel" "DrawRectangleLinesEx(layout.panel"
        "draw_material_text(" "layout.confirm" "layout.cancel")
    require_token("${_bag_source}" "${_token}"
        "T3 reinforcement confirmation must be Chinese, opaque, bordered, and centered")
endforeach()

foreach(_token IN ITEMS
        "u8\"危险：强化失败，装备丢失\", danger_bounds"
        "u8\"确认强化石？\", question_bounds"
        "u8\"确认\", layout.confirm"
        "u8\"取消\", layout.cancel")
    require_token("${_bag_source}" "${_token}"
        "T3 reinforcement confirmation text must stay centered in its own rectangle")
endforeach()

foreach(_token IN ITEMS "u8\"成功\"" "u8\"失败\""
        "u8\"装备丢失\"" "result_bounds"
        "DrawRectangleRec(result_bounds" "DrawRectangleRoundedLinesEx(result_bounds"
        "draw_centered_hud_font_text(")
    require_token("${_inventory_source}" "${_token}"
        "T3 reinforcement result must combine Chinese text with a state border")
endforeach()

extract_cpp_function_body("${_hud_source}"
    "void HudRenderer::draw_ground_loot(" _ground_hud_body _ground_hud_code)
extract_cpp_function_body("${_hud_source}"
    "void HudRenderer::draw_material_loot(" _material_hud_body _material_hud_code)
foreach(_body IN ITEMS _ground_hud_body _material_hud_body)
    foreach(_token IN ITEMS
            "if (!IsWindowReady() || !font_ready_ || !IsFontValid(font_)) return"
            "const Color backing{5, 9, 16, 255}" "fit_centered_ui_text("
            "layout.position" "ui_luma_contrast_ratio(text, backing)"
            "draw_crisp_ui_text(font_, layout.text, layout.position")
        require_token("${${_body}}" "${_token}"
            "T3 HUD loot labels must use SharedUiFont and T0 centered fitting")
    endforeach()
    foreach(_forbidden IN ITEMS "GetFontDefault(" "Fade("
            "bounds.x + 8.0F" "bounds.y + 3.0F" "DrawText(")
        reject_token("${${_body}}" "${_forbidden}"
            "T3 HUD loot labels must not use fallback, transparency, or fixed left offsets")
    endforeach()
endforeach()
reject_dead_branch("${_ground_hud_code}" "T3 ground-loot HUD method")
reject_dead_branch("${_material_hud_code}" "T3 material-loot HUD method")

message(STATUS "T3 ground/material loot renderer source guard accepted")
