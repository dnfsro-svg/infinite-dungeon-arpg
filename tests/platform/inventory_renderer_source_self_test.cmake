cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(_guard
    "${SOURCE_ROOT}/tests/platform/inventory_renderer_source_test.cmake")
set(_renderer_header
    "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.hpp")
set(_renderer
    "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp")
set(_view_header
    "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.hpp")
set(_view_source
    "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.cpp")
set(_material_header
    "${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.hpp")
set(_material_source
    "${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.cpp")
foreach(_path IN ITEMS _guard _renderer_header _renderer _view_header
        _view_source _material_header _material_source)
    if(NOT EXISTS "${${_path}}")
        message(FATAL_ERROR "T2 self-test input is missing: ${${_path}}")
    endif()
endforeach()

file(READ "${_renderer_header}" _renderer_header_original)
file(READ "${_renderer}" _renderer_original)
file(READ "${_view_header}" _header_original)
file(READ "${_view_source}" _view_original)
file(READ "${_material_header}" _material_header_original)
file(READ "${_material_source}" _material_source_original)
file(READ "${_guard}" _guard_original)
string(REPLACE "\r\n" "\n" _guard_original "${_guard_original}")
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
set_property(GLOBAL PROPERTY T2_INVENTORY_RENDERER_MISSED_MUTATIONS "")

set(_dead_guard_function [=[function(reject_dead_branch CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES
            "if[ ]*(constexpr[ ]*)?\\([ ]*(false|0|0[ ]*==[ ]*1|1[ ]*==[ ]*0)[ ]*\\)"
            OR "${CODE}" MATCHES "#[ ]*if[ ]+0([^0-9]|$)"
            OR "${CODE}" MATCHES
                "if[ \\t\\r\\n]*\\([^\\{]*&&[ \\t\\r\\n]*false[ \\t\\r\\n]*\\)[ \\t\\r\\n]*\\{")
        message(FATAL_ERROR "${MESSAGE_TEXT} cannot live in a dead branch")
    endif()
endfunction()]=])
set(_no_dead_guard_function [=[function(reject_dead_branch CODE MESSAGE_TEXT)
endfunction()]=])
string(REPLACE "${_dead_guard_function}" "${_no_dead_guard_function}"
    _guard_without_dead_check_text "${_guard_original}")
if(_guard_without_dead_check_text STREQUAL _guard_original)
    message(FATAL_ERROR "T2 dead-branch guard probe anchor is missing")
endif()
set(_guard_without_dead_check
    "${MUTATION_ROOT}/inventory_renderer_source_test_without_dead_check.cmake")
file(WRITE "${_guard_without_dead_check}" "${_guard_without_dead_check_text}")
set(_draw_lambda_guard_call
    [=[reject_lambda("${_draw_code}" "T4 inventory live draw")]=])
string(REPLACE "${_draw_lambda_guard_call}" ""
    _guard_without_draw_lambda_check_text "${_guard_original}")
if(_guard_without_draw_lambda_check_text STREQUAL _guard_original)
    message(FATAL_ERROR "T2 draw-lambda guard probe anchor is missing")
endif()
set(_guard_without_draw_lambda_check
    "${MUTATION_ROOT}/inventory_renderer_source_test_without_draw_lambda_check.cmake")
file(WRITE "${_guard_without_draw_lambda_check}"
    "${_guard_without_draw_lambda_check_text}")
file(COPY "${SOURCE_ROOT}/tests/platform/cpp_source_function_body.cmake"
    DESTINATION "${MUTATION_ROOT}")

function(run_guard_with_all_inputs LABEL RENDERER_HEADER_TEXT RENDERER_TEXT
        HEADER_TEXT VIEW_TEXT MATERIAL_HEADER_TEXT MATERIAL_TEXT
        EXPECT_SUCCESS GUARD_SCRIPT)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}")
    set(_renderer_header_mutant "${_root}/inventory_renderer.hpp")
    set(_renderer_mutant "${_root}/inventory_renderer.cpp")
    set(_header_mutant "${_root}/inventory_view_math.hpp")
    set(_view_mutant "${_root}/inventory_view_math.cpp")
    set(_material_header_mutant "${_root}/material_bag_renderer.hpp")
    set(_material_mutant "${_root}/material_bag_renderer.cpp")
    file(WRITE "${_renderer_header_mutant}" "${RENDERER_HEADER_TEXT}")
    file(WRITE "${_renderer_mutant}" "${RENDERER_TEXT}")
    file(WRITE "${_header_mutant}" "${HEADER_TEXT}")
    file(WRITE "${_view_mutant}" "${VIEW_TEXT}")
    file(WRITE "${_material_header_mutant}" "${MATERIAL_HEADER_TEXT}")
    file(WRITE "${_material_mutant}" "${MATERIAL_TEXT}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DINVENTORY_HEADER=${_renderer_header_mutant}"
        "-DINVENTORY_SOURCE=${_renderer_mutant}"
        "-DVIEW_HEADER=${_header_mutant}"
        "-DVIEW_SOURCE=${_view_mutant}"
        "-DMATERIAL_HEADER=${_material_header_mutant}"
        "-DMATERIAL_SOURCE=${_material_mutant}"
        -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
        message(FATAL_ERROR "T2 guard unexpectedly rejected ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY
            T2_INVENTORY_RENDERER_MISSED_MUTATIONS "${LABEL}")
    endif()
endfunction()

function(run_guard_with_script LABEL RENDERER_TEXT HEADER_TEXT VIEW_TEXT
        EXPECT_SUCCESS GUARD_SCRIPT)
    run_guard_with_all_inputs("${LABEL}" "${_renderer_header_original}"
        "${RENDERER_TEXT}" "${HEADER_TEXT}" "${VIEW_TEXT}"
        "${_material_header_original}" "${_material_source_original}"
        "${EXPECT_SUCCESS}" "${GUARD_SCRIPT}")
endfunction()

function(run_guard LABEL RENDERER_TEXT HEADER_TEXT VIEW_TEXT EXPECT_SUCCESS)
    run_guard_with_script("${LABEL}" "${RENDERER_TEXT}" "${HEADER_TEXT}"
        "${VIEW_TEXT}" "${EXPECT_SUCCESS}" "${_guard}")
endfunction()

function(expect_renderer_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_renderer_original}")
    if(_mutated STREQUAL _renderer_original)
        message(FATAL_ERROR "T2 renderer mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_mutated}" "${_header_original}"
        "${_view_original}" FALSE)
endfunction()

function(expect_renderer_mutation_accepted_without_dead_check
        LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_renderer_original}")
    if(_mutated STREQUAL _renderer_original)
        message(FATAL_ERROR "T2 dead-branch probe anchor is missing: ${LABEL}")
    endif()
    run_guard_with_script("${LABEL}_without_dead_check" "${_mutated}"
        "${_header_original}" "${_view_original}" TRUE
        "${_guard_without_dead_check}")
endfunction()

function(expect_view_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_view_original}")
    if(_mutated STREQUAL _view_original)
        message(FATAL_ERROR "T2 view mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_renderer_original}" "${_header_original}"
        "${_mutated}" FALSE)
endfunction()

function(expect_renderer_header_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_renderer_header_original}")
    if(_mutated STREQUAL _renderer_header_original)
        message(FATAL_ERROR
            "T2 renderer-header mutation anchor is missing: ${LABEL}")
    endif()
    run_guard_with_all_inputs("${LABEL}" "${_mutated}"
        "${_renderer_original}" "${_header_original}" "${_view_original}"
        "${_material_header_original}" "${_material_source_original}"
        FALSE "${_guard}")
endfunction()

function(expect_material_header_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_material_header_original}")
    if(_mutated STREQUAL _material_header_original)
        message(FATAL_ERROR
            "T2 material-header mutation anchor is missing: ${LABEL}")
    endif()
    run_guard_with_all_inputs("${LABEL}" "${_renderer_header_original}"
        "${_renderer_original}" "${_header_original}" "${_view_original}"
        "${_mutated}" "${_material_source_original}" FALSE "${_guard}")
endfunction()

function(expect_material_source_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_material_source_original}")
    if(_mutated STREQUAL _material_source_original)
        message(FATAL_ERROR
            "T2 material-source mutation anchor is missing: ${LABEL}")
    endif()
    run_guard_with_all_inputs("${LABEL}" "${_renderer_header_original}"
        "${_renderer_original}" "${_header_original}" "${_view_original}"
        "${_material_header_original}" "${_mutated}" FALSE "${_guard}")
endfunction()

run_guard(pristine "${_renderer_original}" "${_header_original}"
    "${_view_original}" TRUE)

expect_view_mutation_rejected(core_skill_display_name
    "ui_skill_name(id)"
    "skills::active_skill_definition(id)->display_name")

expect_renderer_mutation_rejected(core_item_display_name
    "ui_item_base_name(item.base_id)"
    "base->name.data()")

expect_renderer_mutation_rejected(default_font_fallback
    "if (!ready || !IsFontValid(font)) return;"
    "const Font draw_font = ready && IsFontValid(font)\n        ? font : GetFontDefault();")

expect_renderer_mutation_rejected(active_english_panel_title
    "ui_text(UiTextId::equipment_title)"
    "\"EQUIPMENT\"")

set(_dead_constexpr_decoy [=[if constexpr (false) {
            static_cast<void>(tab_labels[index]);
        }
        draw_inventory_page_button(shell.tabs[index], u8"装备",]=])
expect_renderer_mutation_rejected(dead_constexpr_chinese_decoy
    "draw_inventory_page_button(shell.tabs[index], tab_labels[index],"
    "${_dead_constexpr_decoy}")
expect_renderer_mutation_accepted_without_dead_check(
    dead_constexpr_chinese_decoy
    "draw_inventory_page_button(shell.tabs[index], tab_labels[index],"
    "${_dead_constexpr_decoy}")

set(_dead_preprocessor_decoy [=[#if 0
        static_cast<void>(tab_labels[index]);
#endif
        draw_inventory_page_button(shell.tabs[index], u8"装备",]=])
expect_renderer_mutation_rejected(dead_preprocessor_chinese_decoy
    "draw_inventory_page_button(shell.tabs[index], tab_labels[index],"
    "${_dead_preprocessor_decoy}")
expect_renderer_mutation_accepted_without_dead_check(
    dead_preprocessor_decoy
    "draw_inventory_page_button(shell.tabs[index], tab_labels[index],"
    "${_dead_preprocessor_decoy}")

set(_dead_comparison_decoy [=[if (0 == 1) {
            static_cast<void>(tab_labels[index]);
        }
        draw_inventory_page_button(shell.tabs[index], u8"装备",]=])
expect_renderer_mutation_rejected(dead_comparison_chinese_decoy
    "draw_inventory_page_button(shell.tabs[index], tab_labels[index],"
    "${_dead_comparison_decoy}")
expect_renderer_mutation_accepted_without_dead_check(
    dead_comparison_decoy
    "draw_inventory_page_button(shell.tabs[index], tab_labels[index],"
    "${_dead_comparison_decoy}")

expect_renderer_mutation_rejected(grid_metadata_disconnected
    "inventory_grid_item_metadata(item)"
    "std::array<char, 64>{}")

expect_renderer_mutation_rejected(text_width_button_backing
    "DrawRectangleRec(rectangle, fill);"
    "DrawRectangleRec({rectangle.x, rectangle.y,\n        MeasureTextEx(font, label, 18.0F, 0.5F).x, rectangle.height}, fill);")

expect_renderer_mutation_rejected(transparent_text_backing
    "constexpr Color kInventoryTextBacking{5, 9, 16, 255};"
    "constexpr Color kInventoryTextBacking{5, 9, 16, 232};")

expect_renderer_mutation_rejected(missing_actual_button_backing_audit
    "UiTextAuditRole::inventory_status, fill);"
    "UiTextAuditRole::inventory_status);")

expect_view_mutation_rejected(unsupported_runtime_glyph
    "u8\"空中移动\""
    "u8\"空中控制\"")

expect_renderer_mutation_rejected(skill_name_ignores_local_scale
    "std::max(12.0F, ui_typography().kSkillDescriptionFontSize\n                * skill_text_scale), 12.0F,"
    "std::max(12.0F, ui_typography().kSkillDescriptionFontSize), 12.0F,")

expect_renderer_mutation_rejected(hard_coded_inventory_binding_u8
    "inventory_page_title(page_,\n        inventory_binding_label)"
    "inventory_page_title(page_, u8\"I\")")

expect_renderer_mutation_rejected(hard_coded_inventory_binding_plain
    "inventory_page_title(page_,\n        inventory_binding_label)"
    "inventory_page_title(page_, \"I\")")

expect_renderer_mutation_rejected(wrong_inventory_binding_argument
    "inventory_page_title(page_,\n        inventory_binding_label)"
    "inventory_page_title(page_, ui_text(UiTextId::inventory_title))")

expect_renderer_mutation_rejected(resize_restarts_transient_status
    "tooltip_state_ = {};\n    tooltip_measure_font_ = {};"
    "tooltip_state_ = {};\n    transient_status_state_ = {};\n    tooltip_measure_font_ = {};")

expect_renderer_mutation_rejected(resize_restarts_typed_transient_status
    "tooltip_state_ = {};\n    tooltip_measure_font_ = {};"
    "tooltip_state_ = {};\n    transient_status_state_ = InventoryTransientStatusState{};\n    tooltip_measure_font_ = {};")

expect_renderer_mutation_rejected(input_frozen_resolver_uses_selected
    "const InventoryVisibleItemSource frozen_source =\n        inventory_visible_item_source(frame_layout_.shell, page_, state,\n            view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);"
    "const InventoryVisibleItemSource frozen_source =\n        inventory_visible_item_source(frame_layout_.shell, page_, state,\n            view_cache_, selected_item_id_, scroll_rows_);")

expect_renderer_mutation_rejected(input_selected_resolver_uses_frozen
    "const InventoryVisibleItemSource selected_source =\n        inventory_visible_item_source(frame_layout_.shell, page_, state,\n            view_cache_, selected_item_id_, scroll_rows_);"
    "const InventoryVisibleItemSource selected_source =\n        inventory_visible_item_source(frame_layout_.shell, page_, state,\n            view_cache_, tooltip_state_.frozen_item_id, scroll_rows_);")

expect_renderer_mutation_rejected(draw_frozen_resolver_uses_selected
    "const InventoryVisibleItemSource frozen_source =\n            inventory_visible_item_source(shell, page_, state, view_cache_,\n                tooltip_state_.frozen_item_id, scroll_rows_);"
    "const InventoryVisibleItemSource frozen_source =\n            inventory_visible_item_source(shell, page_, state, view_cache_,\n                selected_item_id_, scroll_rows_);")

expect_renderer_mutation_rejected(draw_selected_resolver_uses_frozen
    "const InventoryVisibleItemSource selected_source =\n            inventory_visible_item_source(shell, page_, state, view_cache_,\n                selected_item_id_, scroll_rows_);"
    "const InventoryVisibleItemSource selected_source =\n            inventory_visible_item_source(shell, page_, state, view_cache_,\n                tooltip_state_.frozen_item_id, scroll_rows_);")

set(_draw_contract_lambda_anchor [=[    const auto page_title = inventory_page_title(page_,
        inventory_binding_label);]=])
set(_draw_contract_lambda_replacement [=[    const auto draw_contract_decoy = [&]() {
        const auto page_title = inventory_page_title(page_,
            inventory_binding_label);
        const InventoryVisibleItemSource frozen_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                tooltip_state_.frozen_item_id, scroll_rows_);
        const InventoryVisibleItemSource selected_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                selected_item_id_, scroll_rows_);
        static_cast<void>(page_title);
        static_cast<void>(frozen_source);
        static_cast<void>(selected_source);
    };
    static_cast<void>(draw_contract_decoy);
    const auto page_title = inventory_page_title(page_,
        ui_text(UiTextId::inventory_title));]=])
set(_draw_contract_lambda_mutant "${_renderer_original}")
set(_draw_contract_before_frozen "${_draw_contract_lambda_mutant}")
string(REPLACE
    [=[const InventoryVisibleItemSource frozen_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                tooltip_state_.frozen_item_id, scroll_rows_);]=]
    [=[const InventoryVisibleItemSource frozen_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                selected_item_id_, scroll_rows_);]=]
    _draw_contract_lambda_mutant "${_draw_contract_lambda_mutant}")
if(_draw_contract_lambda_mutant STREQUAL _draw_contract_before_frozen)
    message(FATAL_ERROR "T2 draw-contract frozen resolver anchor is missing")
endif()
set(_draw_contract_before_selected "${_draw_contract_lambda_mutant}")
string(REPLACE
    [=[const InventoryVisibleItemSource selected_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                selected_item_id_, scroll_rows_);]=]
    [=[const InventoryVisibleItemSource selected_source =
            inventory_visible_item_source(shell, page_, state, view_cache_,
                tooltip_state_.frozen_item_id, scroll_rows_);]=]
    _draw_contract_lambda_mutant "${_draw_contract_lambda_mutant}")
if(_draw_contract_lambda_mutant STREQUAL _draw_contract_before_selected)
    message(FATAL_ERROR "T2 draw-contract selected resolver anchor is missing")
endif()
set(_draw_contract_before_decoy "${_draw_contract_lambda_mutant}")
string(REPLACE "${_draw_contract_lambda_anchor}"
    "${_draw_contract_lambda_replacement}" _draw_contract_lambda_mutant
    "${_draw_contract_lambda_mutant}")
if(_draw_contract_lambda_mutant STREQUAL _draw_contract_before_decoy)
    message(FATAL_ERROR "T2 draw-contract lambda mutation anchor is missing")
endif()
run_guard_with_script(draw_contract_hidden_in_uncalled_lambda
    "${_draw_contract_lambda_mutant}" "${_header_original}"
    "${_view_original}" FALSE "${_guard}")
run_guard_with_script(draw_contract_without_lambda_check
    "${_draw_contract_lambda_mutant}" "${_header_original}"
    "${_view_original}" TRUE "${_guard_without_draw_lambda_check}")

expect_renderer_mutation_rejected(selected_tooltip_flickers_after_restore
    "|| tooltip_state_.frozen_item_id == selected_source.item_id))"
    "|| false))")

expect_renderer_mutation_rejected(corridor_steals_source_card_input
    "const bool retained_backing_blocks = tooltip_state_.visible\n        && pointer_inside_retained_tooltip;"
    "const bool retained_backing_blocks = tooltip_state_.visible\n        && (pointer_inside_retained_tooltip\n            || contains(retained_frame.layout.retention_corridor, mouse));")

expect_renderer_mutation_rejected(corridor_widens_pointer_inside_backing
    "const bool pointer_inside_retained_tooltip = retained_frame.valid\n        && contains(retained_frame.layout.backing, mouse);"
    "const bool pointer_inside_retained_tooltip = retained_frame.valid\n        && (contains(retained_frame.layout.backing, mouse)\n            || contains(retained_frame.layout.retention_corridor, mouse));")

expect_renderer_mutation_rejected(equipped_material_target_grid_only
    "&& selected_material.has_value() && direct_hit.item != nullptr;"
    "&& selected_material.has_value() && direct_hit.grid_item;")

expect_renderer_mutation_rejected(equipped_material_visual_disabled
    "output.material_target_mode = material_target.material.has_value();"
    "output.material_target_mode = false;")

expect_renderer_mutation_rejected(equipped_material_rejection_ignored
    "output.material_target_rejected = material_target.rejection_active\n        && material_target.rejected_item_id == item_id;"
    "output.material_target_rejected = false;")

expect_renderer_mutation_rejected(equipped_material_context_disconnected
    "selected_item_id_, recipe_, material_target, hud_font,"
    "selected_item_id_, recipe_, InventoryMaterialTargetContext{}, hud_font,")

expect_renderer_mutation_rejected(material_context_leaks_to_materials_page
    "        material_target.material = selected_material;"
    "    }\n    material_target.material = selected_material;\n    if (true) {")

expect_renderer_mutation_rejected(empty_state_material_context_disconnected
    "view_cache_.filtered_indices.size(), selected_material);"
    "view_cache_.filtered_indices.size(), std::nullopt);")

expect_renderer_mutation_rejected(skill_showcase_entry_auto_selects_slot_zero
    "void InventoryRenderer::show_skill_stones_page() noexcept {"
    "void InventoryRenderer::show_skill_stones_page() noexcept {\n    active_skill_selection_.selected_slot = 0U;")

expect_renderer_mutation_rejected(skill_tab_entry_auto_selects_slot_zero
    "case InventoryPointerLayer::tab:"
    "case InventoryPointerLayer::tab:\n        active_skill_selection_.selected_slot = 0U;")

expect_renderer_mutation_rejected(skill_support_read_only_label_disconnected
    "u8\"辅助技能石（只读）\""
    "u8\"\"")

expect_renderer_mutation_rejected(non_centered_remove_button
    "draw_button(shell.action_bounds, presentation.action.data(),"
    "draw_button(Rectangle{}, presentation.action.data(),")

expect_renderer_mutation_rejected(single_column_detail_rows
    "DrawRectangleRec(shell.grid_viewport, Color{11, 16, 25, 255});"
    "InventoryDetailRowLayout legacy_detail{};\n        DrawRectangleRec(shell.grid_viewport, Color{11, 16, 25, 255});")

expect_renderer_header_mutation_rejected(renderer_header_state_disconnected
    "InventoryTransientStatusState transient_status_state_{};"
    "InventoryTransientStatusState transient_status_state_removed_{};")

expect_material_header_mutation_rejected(material_header_legacy_draw_api
    "class MaterialBagRenderer final {"
    "class MaterialBagRenderer final {\n    void draw(const items::ItemOwnershipState&, const MaterialPack&, Font font, bool font_ready, int width, int height);")

expect_material_source_mutation_rejected(material_source_layout_disconnected
    "if (!layout.contains_all_slots()) return;"
    "if (layout.slots.empty()) return;")

get_property(_missed_mutations GLOBAL PROPERTY
    T2_INVENTORY_RENDERER_MISSED_MUTATIONS)
if(_missed_mutations)
    list(FILTER _missed_mutations EXCLUDE REGEX "^$")
endif()
if(_missed_mutations)
    list(JOIN _missed_mutations ", " _missed_labels)
    message(FATAL_ERROR "T2 guard missed mutations: ${_missed_labels}")
endif()

message(STATUS "T2 inventory renderer source guard self-test accepted")
