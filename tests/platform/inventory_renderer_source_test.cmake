cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

foreach(_name IN ITEMS INVENTORY_HEADER INVENTORY_SOURCE VIEW_HEADER VIEW_SOURCE
        MATERIAL_HEADER MATERIAL_SOURCE)
    if(NOT DEFINED ${_name})
        if(_name STREQUAL "INVENTORY_HEADER")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.hpp")
        elseif(_name STREQUAL "INVENTORY_SOURCE")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp")
        elseif(_name STREQUAL "VIEW_HEADER")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.hpp")
        elseif(_name STREQUAL "VIEW_SOURCE")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.cpp")
        elseif(_name STREQUAL "MATERIAL_HEADER")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.hpp")
        else()
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${_name}}")
        message(FATAL_ERROR "T2 source is missing: ${_name}=${${_name}}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
file(READ "${INVENTORY_HEADER}" _inventory_header)
file(READ "${INVENTORY_SOURCE}" _inventory_source)
file(READ "${VIEW_HEADER}" _view_header)
file(READ "${VIEW_SOURCE}" _view_source)
file(READ "${MATERIAL_HEADER}" _material_header)
file(READ "${MATERIAL_SOURCE}" _material_source)

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

function(require_token_order TEXT FIRST SECOND MESSAGE_TEXT)
    string(FIND "${TEXT}" "${FIRST}" _first)
    string(FIND "${TEXT}" "${SECOND}" _second)
    if(_first EQUAL -1 OR _second EQUAL -1)
        message(FATAL_ERROR
            "${MESSAGE_TEXT}: expected ${FIRST} before ${SECOND}")
    endif()
    if(NOT _first LESS _second)
        message(FATAL_ERROR
            "${MESSAGE_TEXT}: expected ${FIRST} before ${SECOND}")
    endif()
endfunction()

function(require_unique_token TEXT TOKEN MESSAGE_TEXT)
    string(FIND "${TEXT}" "${TOKEN}" _first)
    if(_first EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: missing ${TOKEN}")
    endif()
    string(LENGTH "${TOKEN}" _token_length)
    math(EXPR _tail_start "${_first} + ${_token_length}")
    string(SUBSTRING "${TEXT}" ${_tail_start} -1 _tail)
    string(FIND "${_tail}" "${TOKEN}" _second)
    if(NOT _second EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: duplicate ${TOKEN}")
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

function(reject_lambda CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES
            "(^|[=({,;:])[ ]*\\[[^\\]]*\\][ ]*(<|\\(|mutable|constexpr|consteval|noexcept|->|\\{)")
        message(FATAL_ERROR
            "${MESSAGE_TEXT} required behavior cannot be hidden in a lambda")
    endif()
endfunction()

normalize_cpp_source("${_inventory_source}"
    _inventory_normalized _inventory_code)
normalize_cpp_source("${_inventory_header}"
    _inventory_header_normalized _inventory_header_code)
normalize_cpp_source("${_view_source}" _view_normalized _view_code)
normalize_cpp_source("${_view_header}" _header_normalized _header_code)
normalize_cpp_source("${_material_header}"
    _material_header_normalized _material_header_code)
normalize_cpp_source("${_material_source}"
    _material_normalized _material_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _inventory_compact
    "${_inventory_normalized}")
string(REGEX REPLACE "[ \t\r\n]+" "" _inventory_header_compact
    "${_inventory_header_normalized}")
string(REGEX REPLACE "[ \t\r\n]+" "" _header_compact
    "${_header_normalized}")
string(REGEX REPLACE "[ \t\r\n]+" "" _material_header_compact
    "${_material_header_normalized}")

require_token("${_inventory_compact}"
    "priority.material_target=left_pressed&&page_==InventoryPage::equipment&&selected_material.has_value()&&direct_hit.item!=nullptr;"
    "T4 a selected material must target every directly hit item, including equipped slots")
reject_token("${_inventory_compact}"
    "priority.material_target=left_pressed&&page_==InventoryPage::equipment&&selected_material.has_value()&&direct_hit.grid_item;"
    "T4 equipped material targets must not be restricted to inventory-grid items")

foreach(_forbidden IN ITEMS
        "GetFontDefault(" "DrawText(" "DrawTextEx(" "LoadFontEx("
        "->display_name" ".display_name" "base->name" "definition->name")
    reject_token("${_inventory_code}${_view_code}${_material_code}" "${_forbidden}"
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
    reject_token("${_inventory_normalized}${_view_normalized}${_material_normalized}" "${_english}"
        "T2 player-facing English remains active")
endforeach()

foreach(_unavailable_glyph IN ITEMS "控" "告" "除" "使")
    reject_token("${_inventory_normalized}${_view_normalized}"
        "${_unavailable_glyph}"
        "T2 renderer text uses a codepoint outside the fixed 639 glyph plan")
endforeach()

foreach(_token IN ITEMS
        "structInventoryFrameLayoutfinal{"
        "InventoryShellLayoutshell{};"
        "ActiveSkillLoadoutLayoutskills{};"
        "MaterialBagLayoutmaterials{};"
        "structInventoryTooltipFramefinal{"
        "InventoryTooltipInteractionStatetooltip_state_{};"
        "InventoryTransientStatusStatetransient_status_state_{};"
        "Fonttooltip_measure_font_{};"
        "booltooltip_measure_font_ready_{};")
    require_token("${_inventory_header_compact}" "${_token}"
        "T4 renderer frame/tooltip state contract is missing")
endforeach()

foreach(_legacy IN ITEMS
        "structInventoryLayoutfinal{" "equipment_materials=equipment"
        "structInventoryTextSafeLayoutfinal{"
        "structInventoryDetailRowLayoutfinal{"
        "InventoryDetailRowLayoutinventory_detail_row_layout("
        "Rectanglematerial_bag_detail_bounds("
        "Rectangledetail_line_rectangle("
        "ActiveSkillLoadoutLayoutactive_skill_loadout_layout(intwidth,intheight)"
        "MaterialBagLayoutmaterial_bag_layout(intwidth,intheight)"
        "constitems::ItemOwnershipState&,intwidth,intheight"
        "voiddraw(constitems::ItemOwnershipState&,constMaterialPack&,Fontfont,boolfont_ready,intwidth,intheight)")
    reject_token("${_header_compact}${_material_header_compact}" "${_legacy}"
        "T4 legacy inventory/detail/material API remains public")
endforeach()

extract_cpp_function_body("${_view_source}"
    "void copy_skill_name(" _skill_name_body _skill_name_code)
require_token("${_skill_name_body}" "ui_skill_name(id)"
    "T2 skill names must come from ui_text_catalog")
reject_dead_branch("${_skill_name_code}" "T2 skill-name catalog consumer")

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
    "void draw_centered_shell_text(" _shell_text_body _shell_text_code)
foreach(_token IN ITEMS "ui_viewport_scale("
        "physical_size / viewport_scale"
        "physical_minimum_size / viewport_scale"
        "draw_centered_hud_font_text(")
    require_token("${_shell_text_body}" "${_token}"
        "T4 shell text must consume already-scaled physical typography exactly once")
endforeach()
reject_dead_branch("${_shell_text_code}" "T4 shell text")

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
string(REGEX REPLACE "[ \t\r\n]+" " " _skill_compact "${_skill_code}")
foreach(_token IN ITEMS
        "draw_centered_hud_font_text(" "name_backing"
        "inventory_name_backing" "layout.main_slots"
        "layout.inventory_slots" "const float skill_text_scale ="
        "std::max(12.0F, 18.0F * skill_text_scale), 12.0F,"
        "ui_text(UiTextId::inventory_skill_empty)")
    require_token("${_skill_body}" "${_token}"
        "T4 skill content must use the shared shell layout and centered text")
endforeach()
require_token("${_skill_compact}"
    "std::max(12.0F, ui_typography().kSkillDescriptionFontSize * skill_text_scale), 12.0F,"
    "T4 skill name size must consume the local shell scale")
reject_dead_branch("${_skill_code}" "T2 active-skill loadout renderer")

extract_cpp_function_body("${_inventory_source}"
    "void draw_inventory_item_overlay(" _overlay_body _overlay_code)
foreach(_token IN ITEMS "outline_color(visual.outline)"
        "visual.selected_indicator" "visual.recipe_ordinal" "u/3")
    require_token("${_overlay_body}" "${_token}"
        "T4 item semantic overlay is incomplete")
endforeach()
reject_dead_branch("${_overlay_code}" "T4 item semantic overlay")

extract_cpp_function_body("${_inventory_source}"
    "void draw_equipment_overview(" _overview_body _overview_code)
foreach(_token IN ITEMS "draw_character_silhouette(shell)"
        "shell.equipment_slots" "inventory_item_visual_state("
        "draw_inventory_item_overlay("
        "inventory_equipment_slot_label(slot, item)")
    require_token("${_overview_body}" "${_token}"
        "T4 equipment overview must draw the silhouette and six semantic slots")
endforeach()
require_token_order("${_overview_body}" "assets.draw_horizontal_slice("
    "draw_inventory_item_overlay("
    "T4 equipped-slot semantics must be drawn after the slot asset/fallback")
reject_dead_branch("${_overview_code}" "T4 equipment overview")

extract_cpp_function_body("${_inventory_source}"
    "void draw_inventory_tooltip(" _tooltip_draw_body _tooltip_draw_code)
foreach(_token IN ITEMS "DrawRectangleRec(frame.layout.backing"
        "frame.layout.placements" "draw_crisp_ui_text(")
    require_token("${_tooltip_draw_body}" "${_token}"
        "T4 tooltip backing and segment text path is incomplete")
endforeach()
require_token_order("${_tooltip_draw_body}"
    "DrawRectangleRec(frame.layout.backing" "frame.layout.placements"
    "T4 tooltip backing must be established before segment traversal")
reject_dead_branch("${_tooltip_draw_code}" "T4 tooltip draw")

extract_cpp_function_body("${_inventory_source}"
    "void draw_inventory_status(" _status_body _status_code)
foreach(_token IN ITEMS "DrawRectangleRec(shell.status_bar, kInventoryTextBacking)"
        "status_tone_color(presentation.tone)"
        "shell.status_bounds" "shell.action_bounds" "shell.hint_bounds"
        "presentation.action_visible" "presentation.action_enabled")
    require_token("${_status_body}" "${_token}"
        "T4 single bottom status/action/hint renderer is incomplete")
endforeach()
reject_dead_branch("${_status_code}" "T4 inventory status draw")

extract_cpp_function_body("${_inventory_source}"
    "void InventoryRenderer::refresh_frame_layout("
    _frame_body _frame_code)
foreach(_token IN ITEMS
        "GetScreenWidth()" "GetScreenHeight()"
        "inventory_shell_layout(width, height)"
        "active_skill_loadout_layout(next.shell)"
        "material_bag_layout(next.shell.right_panel, next.shell.geometry_scale)"
        "tooltip_state_ = {}"
        "tooltip_measure_font_ = {}"
        "tooltip_measure_font_ready_ = false" "frame_layout_ = next")
    require_token("${_frame_body}" "${_token}"
        "T4 resize-aware frame cache is incomplete")
endforeach()
reject_token("${_frame_body}" "transient_status_state_"
    "T4 resize must not touch the 180-frame transient status lifetime")
reject_dead_branch("${_frame_code}" "T4 frame-layout cache")

extract_cpp_function_body("${_inventory_source}"
    "void InventoryRenderer::sync(" _sync_body _sync_code)
reject_token("${_sync_body}" "refresh_comparison("
    "T4 sync must not refresh a selected-only comparison")
reject_dead_branch("${_sync_code}" "T4 inventory sync")

extract_cpp_function_body("${_inventory_source}"
    "void InventoryRenderer::refresh_comparison("
    _comparison_body _comparison_code)
foreach(_token IN ITEMS "presentation_item_id" "comparison_item_id_"
        "find_item(state, presentation_item_id)" "compare_player_builds(")
    require_token("${_comparison_body}" "${_token}"
        "T4 tooltip comparison must belong to the presentation item")
endforeach()
reject_token("${_comparison_body}" "selected_item_id_"
    "T4 comparison must not silently fall back to persistent selection")
reject_dead_branch("${_comparison_code}" "T4 presentation comparison")

extract_cpp_function_body("${_inventory_source}"
    "InventoryTooltipFrame InventoryRenderer::build_tooltip_frame("
    _tooltip_frame_body _tooltip_frame_code)
foreach(_token IN ITEMS "refresh_comparison(session, snapshot, presentation_item_id)"
        "find_item(state, presentation_item_id)" "inventory_tooltip_view("
        "inventory_tooltip_layout(frame_layout_.shell, source,"
        "source_is_in_right_panel" "measure_inventory_text")
    require_token("${_tooltip_frame_body}" "${_token}"
        "T4 shared tooltip-frame builder is incomplete")
endforeach()
reject_dead_branch("${_tooltip_frame_code}" "T4 tooltip-frame builder")

foreach(_lifecycle_signature IN ITEMS
        "void InventoryRenderer::open("
        "void InventoryRenderer::close(")
    extract_cpp_function_body("${_inventory_source}"
        "${_lifecycle_signature}" _lifecycle_body _lifecycle_code)
    foreach(_token IN ITEMS "frame_layout_ = {}" "tooltip_state_ = {}"
            "transient_status_state_ = {}" "tooltip_measure_font_ = {}"
            "tooltip_measure_font_ready_ = false")
        require_token("${_lifecycle_body}" "${_token}"
            "T4 inventory lifecycle must clear all volatile layout/status/font state")
    endforeach()
    reject_dead_branch("${_lifecycle_code}" "T4 inventory lifecycle")
endforeach()

extract_cpp_function_body("${_inventory_source}"
    "void InventoryRenderer::show_skill_stones_page("
    _show_skill_page_body _show_skill_page_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _show_skill_page_compact
    "${_show_skill_page_code}")
reject_token("${_show_skill_page_compact}"
    "active_skill_selection_.selected_slot=0U;"
    "T4 opening the skill page must not arm slot zero before player input")
reject_dead_branch("${_show_skill_page_code}" "T4 skill-page showcase entry")

extract_cpp_function_body("${_inventory_source}"
    "bool InventoryRenderer::process_input(" _input_body _input_code)
string(REGEX REPLACE "[ \t\r\n]+" " " _input_compact "${_input_code}")
string(REGEX REPLACE "[ \t\r\n]+" "" _input_tight "${_input_code}")
reject_token("${_input_tight}"
    "active_skill_selection_.selected_slot=0U;"
    "T4 switching to the skill tab must not arm slot zero before player input")
foreach(_resolver IN ITEMS
        "const InventoryVisibleItemSource frozen_source = inventory_visible_item_source(frame_layout_.shell, page_, state, view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);"
        "const InventoryVisibleItemSource selected_source = inventory_visible_item_source(frame_layout_.shell, page_, state, view_cache_, selected_item_id_, scroll_rows_);")
    require_token("${_input_compact}" "${_resolver}"
        "T4 input must resolve frozen and selected sources from the current frame")
endforeach()
require_token_order("${_input_compact}"
    "const InventoryVisibleItemSource frozen_source = inventory_visible_item_source(frame_layout_.shell, page_, state, view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);"
    "const InventoryVisibleItemSource selected_source = inventory_visible_item_source(frame_layout_.shell, page_, state, view_cache_, selected_item_id_, scroll_rows_);"
    "T4 input must resolve frozen before selected fallback")
foreach(_token IN ITEMS
        "refresh_frame_layout()" "frame_layout_.shell"
        "frame_layout_.skills" "frame_layout_.materials"
        "inventory_visible_item_source(" "tooltip_state_.frozen_item_id"
        "selected_item_id_" "build_tooltip_frame("
        "inventory_tooltip_transition(" "inventory_grid_scroll_after_wheel("
        "inventory_pointer_layer(" "InventoryPointerLayer::confirmation"
        "InventoryPointerLayer::tab" "InventoryPointerLayer::tooltip"
        "InventoryPointerLayer::directed_material"
        "InventoryPointerLayer::material_cancel"
        "InventoryPointerLayer::material_select"
        "InventoryPointerLayer::material_target"
        "InventoryPointerLayer::page_action" "InventoryPointerLayer::filter"
        "InventoryPointerLayer::skill_content" "InventoryPointerLayer::recipe"
        "InventoryPointerLayer::equipment"
        "InventoryPointerLayer::background_clear"
        "active_skill_loadout_command_after_click("
        "advance_active_skill_loadout_selection("
        "material_bag_.process_click(mouse, state,"
        "material_bag_.cycle_directed_category("
        "RequestResult::accepted")
    require_token("${_input_body}" "${_token}"
        "T4 renderer input chain is not fully wired")
endforeach()
foreach(_builder IN ITEMS "inventory_shell_layout("
        "active_skill_loadout_layout(" "material_bag_layout(")
    reject_token("${_input_body}" "${_builder}"
        "T4 input must consume the cached frame layout")
endforeach()
reject_token("${_input_body}" "inventory_transient_status_for_frame("
    "T4 input must not consume transient draw-frame lifetime")
require_token_order("${_input_compact}"
    "if (direct_hit.source.valid)"
    "else if (selected_source.valid && (tooltip_state_.frozen_item_id == 0U || tooltip_state_.frozen_item_id == selected_source.item_id))"
    "T4 tooltip candidate priority must prefer direct hit before selected fallback")
require_token("${_input_compact}"
    "const bool pointer_inside_retained_tooltip = retained_frame.valid && contains(retained_frame.layout.backing, mouse);"
    "T4 retained tooltip hit must come only from the current backing")
require_token("${_input_compact}"
    "const bool retained_backing_blocks = tooltip_state_.visible && pointer_inside_retained_tooltip;"
    "T4 only the tooltip backing may block source-card input")
reject_token("${_input_body}" "else if (frozen_source.valid)"
    "T4 an old frozen hover must not re-enter as a new candidate")
require_token_order("${_input_body}"
    "if (!retained_backing_blocks && page_ == InventoryPage::equipment)"
    "inventory_direct_item_hit(frame_layout_.shell"
    "T4 retained tooltip blocker must gate lower item hit-testing")
reject_dead_branch("${_input_code}" "T4 renderer input chain")

extract_cpp_function_body("${_inventory_source}"
    "InventoryItemVisualInput inventory_item_visual_input_with_material("
    _material_visual_body _material_visual_code)
string(REGEX REPLACE "[ \t\r\n]+" " " _material_visual_compact
    "${_material_visual_code}")
foreach(_token IN ITEMS
        "output.material_target_mode = material_target.material.has_value();"
        "output.material_target_enabled = output.material_target_mode && item != nullptr && inventory_material_target_enabled(state, *item, *material_target.material, material_target.directed_category, material_target.root_seed, material_target.operation_nonce, material_target.authority_enabled);"
        "output.material_target_rejected = material_target.rejection_active && material_target.rejected_item_id == item_id;")
    require_token("${_material_visual_compact}" "${_token}"
        "T4 shared equipped/grid material target visuals are incomplete")
endforeach()
reject_dead_branch("${_material_visual_code}"
    "T4 shared equipped/grid material target visuals")
require_token("${_overview_body}"
    "inventory_item_visual_input_with_material("
    "T4 equipped slots must consume the shared material target visual path")

extract_cpp_function_body("${_inventory_source}"
    "void InventoryRenderer::draw(" _draw_body _draw_code)
reject_lambda("${_draw_code}" "T4 inventory live draw")
string(REGEX REPLACE "[ \t\r\n]+" " " _draw_compact "${_draw_code}")
foreach(_resolver IN ITEMS
        "const InventoryVisibleItemSource frozen_source = inventory_visible_item_source(shell, page_, state, view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);"
        "const InventoryVisibleItemSource selected_source = inventory_visible_item_source(shell, page_, state, view_cache_, selected_item_id_, scroll_rows_);")
    require_token("${_draw_compact}" "${_resolver}"
        "T4 draw must resolve frozen and selected sources from the current frame")
endforeach()
require_token_order("${_draw_compact}"
    "const InventoryVisibleItemSource frozen_source = inventory_visible_item_source(shell, page_, state, view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);"
    "const InventoryVisibleItemSource selected_source = inventory_visible_item_source(shell, page_, state, view_cache_, selected_item_id_, scroll_rows_);"
    "T4 draw must resolve frozen before selected fallback")
require_token("${_draw_compact}"
    "inventory_page_title(page_, inventory_binding_label)"
    "T4 inventory title must consume the live binding label")
foreach(_token IN ITEMS
        "refresh_frame_layout()" "frame_layout_.shell"
        "InventoryPage::equipment" "InventoryPage::skill_stones"
        "InventoryPage::materials" "inventory_page_title("
        "shell.tabs" "shell.slot_filter_bounds"
        "shell.rarity_filter_bounds"
        "draw_equipment_overview("
        "inventory_empty_presentation("
        "inventory_item_visual_state(" "build_tooltip_frame("
        "InventoryTransientStatusInput" "inventory_transient_status_for_frame("
        "transient_status_state_ = transient_result.next"
        "inventory_status_presentation("
        "inventory_grid_item_metadata(item)"
        "ui_item_base_name(item.base_id)"
        "material_bag_.draw(state, material_pack, hud_font,"
        "draw_active_skill_loadout(" "draw_inventory_tooltip("
        "draw_inventory_status(")
    require_token("${_draw_body}" "${_token}"
        "T4 renderer must consume the live three-page shell path")
endforeach()
foreach(_token IN ITEMS
        "InventoryMaterialTargetContext material_target{};"
        "if (page_ == InventoryPage::equipment) { material_target.material = selected_material;"
        "material_target.root_seed = snapshot.root_seed;"
        "material_target.operation_nonce = snapshot.commit_generation;"
        "material_target.authority_enabled = authority_enabled;"
        "material_target.rejection_active = material_rejection_frames_ != 0U;"
        "material_target.rejected_item_id = rejected_material_item_id_;"
        "inventory_item_visual_input_with_material(state, &item, item.id,"
        "draw_equipment_overview(state, shell, material_pack, hovered_item_id, selected_item_id_, recipe_, material_target,")
    require_token("${_draw_compact}" "${_token}"
        "T4 live draw must share the real material target context with equipped and grid items")
endforeach()
foreach(_builder IN ITEMS "inventory_shell_layout("
        "active_skill_loadout_layout(" "material_bag_layout(")
    reject_token("${_draw_body}" "${_builder}"
        "T4 draw must consume the cached frame layout")
endforeach()
foreach(_token IN ITEMS "same_font_identity(tooltip_measure_font_, hud_font)"
        "font_identity_changed" "!font_identity_changed && current_font_ready"
        "tooltip_measure_font_ = hud_font")
    require_token("${_draw_body}" "${_token}"
        "T4 tooltip input/draw font identity handshake is incomplete")
endforeach()
require_unique_token("${_draw_body}"
    "inventory_transient_status_for_frame("
    "T4 each draw frame must advance transient status exactly once")
require_unique_token("${_draw_body}" "draw_inventory_status("
    "T4 each draw frame must publish one bottom status presentation")
require_token_order("${_draw_body}"
    "material_pack.draw_horizontal_slice" "draw_inventory_item_overlay("
    "T4 item semantics must be drawn after the grid card asset/fallback")
reject_dead_branch("${_draw_code}" "T2 inventory renderer")

foreach(_legacy IN ITEMS "InventoryLayout" "InventoryTextSafeLayout"
        "InventoryDetailRowLayout" "inventory_layout("
        "inventory_text_safe_layout(" "draw_detail_row(" "detail_line_rectangle("
        "material_bag_detail_bounds(" "GetFontDefault(")
    reject_token("${_inventory_code}${_material_code}" "${_legacy}"
        "T4 live renderer retains a legacy detail/font/binding path")
endforeach()
reject_token("${_inventory_normalized}${_material_normalized}" "\"I\""
    "T4 live renderer hard-codes the default inventory binding")

extract_cpp_function_body("${_material_source}"
    "bool MaterialBagRenderer::process_click(" _material_click_body _material_click_code)
require_token("${_material_click_body}" "layout.slots"
    "T4 material clicks must consume the caller frame layout")
reject_dead_branch("${_material_click_code}" "T4 material click")

extract_cpp_function_body("${_material_source}"
    "void MaterialBagRenderer::draw(" _material_draw_body _material_draw_code)
foreach(_token IN ITEMS "layout.contains_all_slots()"
        "ui_material_description(id)" "state.materials[index]")
    require_token("${_material_draw_body}" "${_token}"
        "T4 material page draw must consume shared layout and catalog text")
endforeach()
reject_dead_branch("${_material_draw_code}" "T4 material page draw")

require_token("${_inventory_compact}"
    "constexprColorkInventoryTextBacking{5,9,16,255};"
    "T2 player text backing must be fully opaque")

message(STATUS "T2 inventory renderer source guard accepted")
