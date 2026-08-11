cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(_guard
    "${SOURCE_ROOT}/tests/platform/inventory_renderer_source_test.cmake")
set(_renderer
    "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp")
set(_view_header
    "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.hpp")
set(_view_source
    "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.cpp")
foreach(_path IN ITEMS _guard _renderer _view_header _view_source)
    if(NOT EXISTS "${${_path}}")
        message(FATAL_ERROR "T2 self-test input is missing: ${${_path}}")
    endif()
endforeach()

file(READ "${_renderer}" _renderer_original)
file(READ "${_view_header}" _header_original)
file(READ "${_view_source}" _view_original)
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
file(COPY "${SOURCE_ROOT}/tests/platform/cpp_source_function_body.cmake"
    DESTINATION "${MUTATION_ROOT}")

function(run_guard_with_script LABEL RENDERER_TEXT HEADER_TEXT VIEW_TEXT
        EXPECT_SUCCESS GUARD_SCRIPT)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}")
    set(_renderer_mutant "${_root}/inventory_renderer.cpp")
    set(_header_mutant "${_root}/inventory_view_math.hpp")
    set(_view_mutant "${_root}/inventory_view_math.cpp")
    file(WRITE "${_renderer_mutant}" "${RENDERER_TEXT}")
    file(WRITE "${_header_mutant}" "${HEADER_TEXT}")
    file(WRITE "${_view_mutant}" "${VIEW_TEXT}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DINVENTORY_SOURCE=${_renderer_mutant}"
        "-DVIEW_HEADER=${_header_mutant}"
        "-DVIEW_SOURCE=${_view_mutant}"
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
        draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),
            material_pack, UiMaterialElement::inventory_panel_equipment,
            hud_font, hud_font_ready, text_layout.equipment_panel_title);
    }
    draw_panel(layout.equipment, u8"装备",]=])
expect_renderer_mutation_rejected(dead_constexpr_chinese_decoy
    "draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),"
    "${_dead_constexpr_decoy}")
expect_renderer_mutation_accepted_without_dead_check(
    dead_constexpr_chinese_decoy
    "draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),"
    "${_dead_constexpr_decoy}")

set(_dead_preprocessor_decoy [=[#if 0
        draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),
            material_pack, UiMaterialElement::inventory_panel_equipment,
            hud_font, hud_font_ready, text_layout.equipment_panel_title);
#endif
    draw_panel(layout.equipment, u8"装备",]=])
expect_renderer_mutation_rejected(dead_preprocessor_chinese_decoy
    "draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),"
    "${_dead_preprocessor_decoy}")
expect_renderer_mutation_accepted_without_dead_check(
    dead_preprocessor_decoy
    "draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),"
    "${_dead_preprocessor_decoy}")

set(_dead_comparison_decoy [=[if (0 == 1) {
        draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),
            material_pack, UiMaterialElement::inventory_panel_equipment,
            hud_font, hud_font_ready, text_layout.equipment_panel_title);
    }
    draw_panel(layout.equipment, u8"装备",]=])
expect_renderer_mutation_rejected(dead_comparison_chinese_decoy
    "draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),"
    "${_dead_comparison_decoy}")
expect_renderer_mutation_accepted_without_dead_check(
    dead_comparison_decoy
    "draw_panel(layout.equipment, ui_text(UiTextId::equipment_title),"
    "${_dead_comparison_decoy}")

expect_renderer_mutation_rejected(default_core_rarity_name
    "ui_item_rarity_name(item.rarity)"
    "rarity_name(item.rarity)")

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
    "ui_typography().kSkillDescriptionFontSize * skill_text_scale,\n            9.0F,"
    "ui_typography().kSkillDescriptionFontSize,\n            9.0F,")

expect_renderer_mutation_rejected(non_centered_remove_button
    "draw_centered_hud_font_text(font, font_ready, u8\"取出\",\n        layout.remove_button,"
    "draw_hud_font_text(font, font_ready, u8\"取出\",\n        layout.remove_button.x, layout.remove_button.y,")

expect_renderer_mutation_rejected(single_column_detail_rows
    "const InventoryDetailRowLayout row = inventory_detail_row_layout("
    "const InventoryDetailRowLayout row{}; static_cast<void>(detail_line_rectangle(")

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
