cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

foreach(_name IN ITEMS INVENTORY_SOURCE VIEW_HEADER VIEW_SOURCE)
    if(NOT DEFINED ${_name})
        if(_name STREQUAL "INVENTORY_SOURCE")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp")
        elseif(_name STREQUAL "VIEW_HEADER")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.hpp")
        else()
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${_name}}")
        message(FATAL_ERROR "T2 source is missing: ${_name}=${${_name}}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
file(READ "${INVENTORY_SOURCE}" _inventory_source)
file(READ "${VIEW_HEADER}" _view_header)
file(READ "${VIEW_SOURCE}" _view_source)

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

normalize_cpp_source("${_inventory_source}"
    _inventory_normalized _inventory_code)
normalize_cpp_source("${_view_source}" _view_normalized _view_code)
normalize_cpp_source("${_view_header}" _header_normalized _header_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _inventory_compact
    "${_inventory_normalized}")
string(REGEX REPLACE "[ \t\r\n]+" "" _header_compact
    "${_header_normalized}")

foreach(_forbidden IN ITEMS
        "GetFontDefault(" "DrawText(" "DrawTextEx(" "LoadFontEx("
        "->display_name" ".display_name" "base->name" "definition->name")
    reject_token("${_inventory_code}${_view_code}" "${_forbidden}"
        "T2 must fail closed and must not consume core display names")
endforeach()

foreach(_english IN ITEMS
        "\"EQUIPMENT\"" "\"INVENTORY\"" "\"ITEM DETAIL\""
        "\"Unknown\"" "\"Weapon\"" "\"Helmet\"" "\"Chest\""
        "\"Gloves\"" "\"Boots\"" "\"Accessory\""
        "\"Normal\"" "\"Magic\"" "\"Rare\""
        "\"LOCAL\"" "\"GLOBAL\"" "\"Pre\"" "\"Suf\""
        "\"Attribute\"" "\"Combine" "\"Slot:" "\"Rarity:"
        "\"No items" "\"Invalid\"" "\"SAVE PENDING"
        "\"DESTROYED\"" "\"SUCCESS\"" "\"FAILED\""
        "\"Click an item" "\"FULL BUILD DIFFERENCE\""
        "\"Preview unavailable\"")
    reject_token("${_inventory_normalized}${_view_normalized}" "${_english}"
        "T2 player-facing English remains active")
endforeach()

foreach(_unavailable_glyph IN ITEMS "控" "告" "除" "使")
    reject_token("${_inventory_normalized}${_view_normalized}"
        "${_unavailable_glyph}"
        "T2 renderer text uses a codepoint outside the fixed 639 glyph plan")
endforeach()

foreach(_token IN ITEMS
        "structInventoryDetailRowLayoutfinal{"
        "Rectanglelabel_bounds{};" "Rectanglevalue_bounds{};"
        "floatgap{};" "boolvalid{};"
        "InventoryDetailRowLayoutinventory_detail_row_layout(")
    require_token("${_header_compact}" "${_token}"
        "T2 fixed detail-row API is missing")
endforeach()

extract_cpp_function_body("${_view_source}"
    "void copy_skill_name(" _skill_name_body _skill_name_code)
require_token("${_skill_name_body}" "ui_skill_name(id)"
    "T2 skill names must come from ui_text_catalog")
reject_dead_branch("${_skill_name_code}" "T2 skill-name catalog consumer")

extract_cpp_function_body("${_view_source}"
    "InventoryDetailRowLayout inventory_detail_row_layout("
    _detail_layout_body _detail_layout_code)
require_token("${_detail_layout_body}" "ui_label_value_row_layout("
    "T2 detail rows must delegate to the T0 two-column layout")
reject_dead_branch("${_detail_layout_code}" "T2 detail-row layout")

extract_cpp_function_body("${_inventory_source}"
    "void draw_hud_font_text(" _text_body _text_code)
foreach(_token IN ITEMS
        "if (!ready || !IsFontValid(font)) return"
        "opaque_backing.a != 255U" "fit_centered_ui_text("
        "ui_luma_contrast_ratio(color, opaque_backing)")
    require_token("${_text_body}" "${_token}"
        "T2 text helper must use SharedUiFont fail-closed UTF-8 fitting")
endforeach()
reject_dead_branch("${_text_code}" "T2 text helper")

extract_cpp_function_body("${_inventory_source}"
    "void draw_centered_hud_font_text(" _centered_body _centered_code)
foreach(_token IN ITEMS "opaque_backing.a != 255U"
        "fit_centered_ui_text(" "layout.position"
        "ui_luma_contrast_ratio(color, opaque_backing)")
    require_token("${_centered_body}" "${_token}"
        "T2 centered text helper is incomplete")
endforeach()
reject_dead_branch("${_centered_code}" "T2 centered text helper")

extract_cpp_function_body("${_inventory_source}"
    "void draw_panel(" _panel_body _panel_code)
foreach(_token IN ITEMS
        "DrawRectangleRec(rectangle, kInventoryTextBacking)"
        "draw_centered_hud_font_text(" "title_bounds")
    require_token("${_panel_body}" "${_token}"
        "T2 panel title must use a full opaque backing and centered fit")
endforeach()
reject_dead_branch("${_panel_code}" "T2 panel title")

extract_cpp_function_body("${_inventory_source}"
    "void draw_button(" _button_body _button_code)
foreach(_token IN ITEMS
        "DrawRectangleRec(rectangle, fill)"
        "inventory_control_text_style()"
        "draw_centered_hud_font_text(" "rectangle"
        "UiTextAuditRole::inventory_status, fill")
    require_token("${_button_body}" "${_token}"
        "T2 buttons must back and center the complete interaction rectangle")
endforeach()
reject_token("${_button_body}" "MeasureTextEx("
    "T2 button backing must not be sized from text width")
reject_dead_branch("${_button_code}" "T2 button")

extract_cpp_function_body("${_inventory_source}"
    "void draw_inventory_page_button(" _tab_body _tab_code)
foreach(_token IN ITEMS
        "inventory_control_text_style()" "DrawRectangleRec(rectangle, backing)"
        "draw_centered_hud_font_text(" "rectangle"
        "UiTextAuditRole::inventory_status, backing")
    require_token("${_tab_body}" "${_token}"
        "T2 tabs must back and center the complete interaction rectangle")
endforeach()
reject_token("${_tab_body}" "MeasureTextEx("
    "T2 tab backing must not be sized from text width")
reject_dead_branch("${_tab_code}" "T2 inventory tab")

extract_cpp_function_body("${_inventory_source}"
    "void draw_active_skill_loadout(" _skill_body _skill_code)
foreach(_token IN ITEMS
        "draw_centered_hud_font_text(" "name_backing"
        "inventory_name_backing" "layout.remove_button"
        "remove_backing" "const float skill_text_scale ="
        "name_backing, ui_typography().kSkillDescriptionFontSize * skill_text_scale, 9.0F,"
        "stone.name.data(), inventory_name_backing, 18.0F * skill_text_scale, 9.0F,"
        "draw_centered_hud_font_text(font, font_ready, u8\"取出\", layout.remove_button,")
    require_token("${_skill_body}" "${_token}"
        "T2 skill titles, names and remove button must be centered")
endforeach()
reject_dead_branch("${_skill_code}" "T2 active-skill loadout renderer")

extract_cpp_function_body("${_inventory_source}"
    "void draw_detail_row(" _detail_draw_body _detail_draw_code)
foreach(_token IN ITEMS
        "inventory_detail_row_layout(" "row.label_bounds"
        "row.value_bounds" "value_x")
    require_token("${_detail_draw_body}" "${_token}"
        "T2 detail renderer must draw left labels and right values")
endforeach()
reject_dead_branch("${_detail_draw_code}" "T2 detail renderer")

extract_cpp_function_body("${_inventory_source}"
    "void InventoryRenderer::draw(" _draw_body _draw_code)
foreach(_token IN ITEMS
        "draw_centered_hud_font_text(" "text_layout.page_title"
        "ui_text(UiTextId::equipment_title)"
        "ui_text(UiTextId::inventory_title)"
        "ui_text(UiTextId::item_detail_title)"
        "ui_item_slot_name(slot)" "ui_item_rarity_name(item.rarity)"
        "ui_item_base_name(item.base_id)" "ui_item_affix_name(roll.affix_id)"
        "ui_material_name(*selected_material)" "draw_detail_row("
        "u8\"基础 [%s]\"" "u8\"T%u %s %s [%s]\""
        "u8\"%s ×%.2f\"")
    require_token("${_draw_body}" "${_token}"
        "T2 renderer must consume the live Chinese catalog/layout path")
endforeach()
reject_dead_branch("${_draw_code}" "T2 inventory renderer")

require_token("${_inventory_compact}"
    "constexprColorkInventoryTextBacking{5,9,16,255};"
    "T2 player text backing must be fully opaque")

message(STATUS "T2 inventory renderer source guard accepted")
