cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(_guard "${SOURCE_ROOT}/tests/platform/ground_loot_renderer_source_test.cmake")
set(_ground "${SOURCE_ROOT}/src/platform/raylib/ground_loot_view.cpp")
set(_pickup "${SOURCE_ROOT}/src/platform/raylib/loot_pickup_feedback.cpp")
set(_material "${SOURCE_ROOT}/src/platform/raylib/material_loot_view.cpp")
set(_bag "${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.cpp")
set(_inventory "${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp")
set(_view_header "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.hpp")
set(_view "${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.cpp")
set(_hud "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp")
set(_catalog "${SOURCE_ROOT}/src/platform/raylib/ui_text_catalog.cpp")
foreach(_path IN ITEMS _guard _ground _pickup _material _bag _inventory
        _view_header _view _hud _catalog)
    if(NOT EXISTS "${${_path}}")
        message(FATAL_ERROR "T3 self-test input is missing: ${${_path}}")
    endif()
endforeach()

file(READ "${_ground}" _ground_original)
file(READ "${_pickup}" _pickup_original)
file(READ "${_material}" _material_original)
file(READ "${_bag}" _bag_original)
file(READ "${_inventory}" _inventory_original)
file(READ "${_view_header}" _view_header_original)
file(READ "${_view}" _view_original)
file(READ "${_hud}" _hud_original)
file(READ "${_catalog}" _catalog_original)
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
set_property(GLOBAL PROPERTY T3_GROUND_LOOT_RENDERER_MISSED_MUTATIONS "")

function(run_guard LABEL GROUND_TEXT PICKUP_TEXT MATERIAL_TEXT BAG_TEXT
        INVENTORY_TEXT VIEW_HEADER_TEXT VIEW_TEXT HUD_TEXT CATALOG_TEXT
        EXPECT_SUCCESS)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}")
    file(WRITE "${_root}/ground_loot_view.cpp" "${GROUND_TEXT}")
    file(WRITE "${_root}/loot_pickup_feedback.cpp" "${PICKUP_TEXT}")
    file(WRITE "${_root}/material_loot_view.cpp" "${MATERIAL_TEXT}")
    file(WRITE "${_root}/material_bag_renderer.cpp" "${BAG_TEXT}")
    file(WRITE "${_root}/inventory_renderer.cpp" "${INVENTORY_TEXT}")
    file(WRITE "${_root}/inventory_view_math.hpp" "${VIEW_HEADER_TEXT}")
    file(WRITE "${_root}/inventory_view_math.cpp" "${VIEW_TEXT}")
    file(WRITE "${_root}/hud_renderer.cpp" "${HUD_TEXT}")
    file(WRITE "${_root}/ui_text_catalog.cpp" "${CATALOG_TEXT}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DGROUND_SOURCE=${_root}/ground_loot_view.cpp"
        "-DPICKUP_SOURCE=${_root}/loot_pickup_feedback.cpp"
        "-DMATERIAL_SOURCE=${_root}/material_loot_view.cpp"
        "-DBAG_SOURCE=${_root}/material_bag_renderer.cpp"
        "-DINVENTORY_SOURCE=${_root}/inventory_renderer.cpp"
        "-DVIEW_HEADER=${_root}/inventory_view_math.hpp"
        "-DVIEW_SOURCE=${_root}/inventory_view_math.cpp"
        "-DHUD_SOURCE=${_root}/hud_renderer.cpp"
        "-DCATALOG_SOURCE=${_root}/ui_text_catalog.cpp"
        -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
        message(FATAL_ERROR "T3 guard unexpectedly rejected ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY
            T3_GROUND_LOOT_RENDERER_MISSED_MUTATIONS "${LABEL}")
    endif()
endfunction()

function(expect_mutation_rejected LABEL TARGET NEEDLE REPLACEMENT)
    set(_ground_text "${_ground_original}")
    set(_pickup_text "${_pickup_original}")
    set(_material_text "${_material_original}")
    set(_bag_text "${_bag_original}")
    set(_inventory_text "${_inventory_original}")
    set(_view_header_text "${_view_header_original}")
    set(_view_text "${_view_original}")
    set(_hud_text "${_hud_original}")
    set(_catalog_text "${_catalog_original}")
    string(TOLOWER "${TARGET}" _target_lower)
    set(_variable "_${_target_lower}_text")
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${${_variable}}")
    if(_mutated STREQUAL "${${_variable}}")
        message(FATAL_ERROR "T3 mutation anchor is missing: ${LABEL}")
    endif()
    set(${_variable} "${_mutated}")
    run_guard("${LABEL}" "${_ground_text}" "${_pickup_text}"
        "${_material_text}" "${_bag_text}" "${_inventory_text}"
        "${_view_header_text}" "${_view_text}" "${_hud_text}"
        "${_catalog_text}" FALSE)
endfunction()

function(expect_mutation_accepted LABEL TARGET NEEDLE REPLACEMENT)
    set(_ground_text "${_ground_original}")
    set(_pickup_text "${_pickup_original}")
    set(_material_text "${_material_original}")
    set(_bag_text "${_bag_original}")
    set(_inventory_text "${_inventory_original}")
    set(_view_header_text "${_view_header_original}")
    set(_view_text "${_view_original}")
    set(_hud_text "${_hud_original}")
    set(_catalog_text "${_catalog_original}")
    string(TOLOWER "${TARGET}" _target_lower)
    set(_variable "_${_target_lower}_text")
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${${_variable}}")
    if(_mutated STREQUAL "${${_variable}}")
        message(FATAL_ERROR "T3 mutation anchor is missing: ${LABEL}")
    endif()
    set(${_variable} "${_mutated}")
    run_guard("${LABEL}" "${_ground_text}" "${_pickup_text}"
        "${_material_text}" "${_bag_text}" "${_inventory_text}"
        "${_view_header_text}" "${_view_text}" "${_hud_text}"
        "${_catalog_text}" TRUE)
endfunction()

run_guard(pristine "${_ground_original}" "${_pickup_original}"
    "${_material_original}" "${_bag_original}" "${_inventory_original}"
    "${_view_header_original}" "${_view_original}" "${_hud_original}"
    "${_catalog_original}" TRUE)

expect_mutation_rejected(core_ground_item_name GROUND
    "ui_item_base_name(item.base_id)" "base->name.data()")
expect_mutation_rejected(core_pickup_item_name PICKUP
    "ui_item_base_name(receipt.base_id)" "base->name.data()")
expect_mutation_rejected(legacy_material_label MATERIAL
    "return ui_material_name(id);" "return u8\"蜕变石\";")
expect_mutation_rejected(partial_material_token MATERIAL
    "format_material_pickup_text(" "format_partial_material_pickup_text(")
expect_mutation_rejected(english_material_bag_title BAG
    "u8\"材料袋\"" "\"MATERIAL BAG\"")
expect_mutation_rejected(default_material_bag_font BAG
    "if (!ready || !IsFontValid(font)) return;"
    "const Font draw_font = ready ? font : GetFontDefault();")
expect_mutation_rejected(noncentered_material_bag_text BAG
    "fit_centered_ui_text(" "UiCenteredTextLayout{")
expect_mutation_rejected(transparent_material_bag_backing BAG
    "constexpr Color kMaterialTextBacking{5, 9, 16, 255};"
    "constexpr Color kMaterialTextBacking{5, 9, 16, 232};")
expect_mutation_rejected(low_contrast_confirm_backing BAG
    "constexpr Color kConfirmBacking{74, 24, 27, 255};"
    "constexpr Color kConfirmBacking{248, 246, 238, 255};")
expect_mutation_rejected(noncentered_confirmation_question BAG
    "u8\"确认强化石？\", question_bounds,"
    "u8\"确认强化石？\", layout.panel,")
expect_mutation_rejected(noncentered_confirmation_button BAG
    "u8\"确认\", layout.confirm,"
    "u8\"确认\", layout.panel,")
expect_mutation_rejected(missing_receipt_generation_dedup INVENTORY
    "transient_input.receipt_commit_generation =\n        snapshot.reinforcement_receipt.commit_generation;"
    "transient_input.receipt_commit_generation = 0U;")
expect_mutation_rejected(wrong_receipt_generation_with_live_decoy INVENTORY
    "transient_input.receipt_commit_generation =\n        snapshot.reinforcement_receipt.commit_generation;"
    "transient_input.receipt_commit_generation =\n        snapshot.commit_generation;\n    static_cast<void>(snapshot.reinforcement_receipt.commit_generation);")
expect_mutation_rejected(wrong_receipt_destroyed_presentation_binding_with_decoy
    INVENTORY
    "transient_input.receipt_destroyed =\n        snapshot.reinforcement_receipt.destroyed;"
    "transient_input.receipt_destroyed =\n        snapshot.reinforcement_receipt.success;\n    static_cast<void>(snapshot.reinforcement_receipt.destroyed);")
set(_destroyed_string_decoy
    "static_assert(true, \"transient_input.receipt_destroyed = snapshot.reinforcement_receipt.destroyed;\");")
expect_mutation_accepted(receipt_destroyed_string_decoy_causal_control INVENTORY
    "transient_input.receipt_destroyed =\n        snapshot.reinforcement_receipt.destroyed;"
    "transient_input.receipt_destroyed =\n        snapshot.reinforcement_receipt.destroyed;\n    ${_destroyed_string_decoy}")
expect_mutation_rejected(wrong_receipt_destroyed_live_binding_with_string_decoy
    INVENTORY
    "transient_input.receipt_destroyed =\n        snapshot.reinforcement_receipt.destroyed;"
    "transient_input.receipt_destroyed =\n        snapshot.reinforcement_receipt.success;\n    ${_destroyed_string_decoy}")
expect_mutation_rejected(transient_frame_count_header_disconnected VIEW_HEADER
    "kInventoryTransientStatusFrameCount = 180U"
    "kInventoryTransientStatusFrameCount = 179U")
expect_mutation_rejected(transient_frame_lifetime_shortened VIEW
    "state.remaining_frames = kInventoryTransientStatusFrameCount;"
    "state.remaining_frames = kInventoryTransientStatusFrameCount - 1U;")
expect_mutation_rejected(second_reinforcement_status INVENTORY
    "draw_inventory_status(shell, status_presentation, material_pack,\n        hud_font, current_font_ready);"
    "draw_inventory_status(shell, status_presentation, material_pack,\n        hud_font, current_font_ready);\n    draw_inventory_status(shell, status_presentation, material_pack,\n        hud_font, current_font_ready);")
expect_mutation_rejected(transparent_inventory_status_backing INVENTORY
    "constexpr Color kInventoryTextBacking{5, 9, 16, 255};"
    "constexpr Color kInventoryTextBacking{5, 9, 16, 232};")
expect_mutation_rejected(default_ground_loot_font HUD
    "if (!IsWindowReady() || !font_ready_ || !IsFontValid(font_)) return;"
    "if (!IsWindowReady()) return;\n    const Font draw_font = GetFontDefault();")
expect_mutation_rejected(transparent_loot_backing HUD
    "const Color backing{5, 9, 16, 255};"
    "const Color backing{5, 9, 16, 232};")
expect_mutation_rejected(faded_loot_backing HUD
    "DrawRectangleRounded(bounds, 0.18F, 6, backing);"
    "DrawRectangleRounded(bounds, 0.18F, 6, Fade(backing, 0.5F));")
expect_mutation_rejected(noncentered_loot_text HUD
    "draw_crisp_ui_text(font_, layout.text, layout.position"
    "draw_crisp_ui_text(font_, layout.text, {bounds.x + 8.0F, bounds.y + 3.0F}")
expect_mutation_rejected(dead_shared_font_decoy HUD
    "void HudRenderer::draw_ground_loot(\n    const GroundLootView& view) const noexcept {"
    "void HudRenderer::draw_ground_loot(\n    const GroundLootView& view) const noexcept {\n    if (false) {}")
expect_mutation_rejected(legacy_catalog_glyph CATALOG
    "u8\"材料袋\"" "u8\"材料蜕\"")

get_property(_missed_mutations GLOBAL PROPERTY
    T3_GROUND_LOOT_RENDERER_MISSED_MUTATIONS)
if(_missed_mutations)
    list(FILTER _missed_mutations EXCLUDE REGEX "^$")
endif()
if(_missed_mutations)
    list(JOIN _missed_mutations ", " _missed_labels)
    message(FATAL_ERROR "T3 guard missed mutations: ${_missed_labels}")
endif()

message(STATUS "T3 ground/material loot renderer source guard self-test accepted")
