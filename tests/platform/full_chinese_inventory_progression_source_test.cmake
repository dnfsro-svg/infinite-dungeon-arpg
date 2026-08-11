cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(_files
    inventory_view_math.cpp inventory_renderer.cpp ground_loot_view.cpp
    loot_pickup_feedback.cpp material_loot_view.cpp material_bag_renderer.cpp
    hud_renderer.cpp passive_tree_view_math.cpp passive_tree_renderer.cpp
    death_overlay_view.cpp death_overlay_renderer.cpp dungeon_view_math.cpp
    combat_renderer.cpp ui_text_catalog.cpp ui_text_catalog.hpp ui_text_layout.cpp
    ui_text_layout.hpp)
foreach(_file IN LISTS _files)
    set(_path "${SOURCE_ROOT}/src/platform/raylib/${_file}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "T6 required production file is missing: ${_path}")
    endif()
    file(READ "${_path}" _raw_${_file})
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")

function(require_token TEXT TOKEN CONTEXT)
    string(FIND "${TEXT}" "${TOKEN}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR "T6 ${CONTEXT} is missing required token: ${TOKEN}")
    endif()
endfunction()

function(reject_token TEXT TOKEN CONTEXT)
    string(FIND "${TEXT}" "${TOKEN}" _index)
    if(NOT _index EQUAL -1)
        message(FATAL_ERROR "T6 ${CONTEXT} contains forbidden token: ${TOKEN}")
    endif()
endfunction()

function(reject_dead_branch CODE CONTEXT)
    if("${CODE}" MATCHES "#[ \t]*if[ \t]+0([^0-9]|$)"
            OR "${CODE}" MATCHES "if[ \t]*(constexpr[ \t]*)?\\([ \t]*(false|0|0[ \t]*==[ \t]*1|1[ \t]*==[ \t]*0)[ \t]*\\)"
            OR "${CODE}" MATCHES "if[ \t]*\\([^\\)]*(false[ \t]*&&|![ \t]*true|0[ \t]*!=[ \t]*0|1[ \t]*!=[ \t]*1)[^\\)]*\\)")
        message(FATAL_ERROR "T6 ${CONTEXT} required behavior is hidden in a dead branch")
    endif()
endfunction()

function(reject_lambda CODE CONTEXT)
    # CODE has already been stripped of comments and string literals.  Scan
    # capture lists instead of relying on a regex: init captures may nest []
    # and lambda declarators may carry mutable/noexcept/trailing return types.
    string(LENGTH "${CODE}" _length)
    set(_index 0)
    while(_index LESS _length)
        string(SUBSTRING "${CODE}" ${_index} 1 _character)
        if(NOT _character STREQUAL "[")
            math(EXPR _index "${_index} + 1")
            continue()
        endif()
        math(EXPR _following_index "${_index} + 1")
        if(_following_index LESS _length)
            string(SUBSTRING "${CODE}" ${_following_index} 1 _following)
            if(_following STREQUAL "[")
                # C++ attributes ([[maybe_unused]]) are not lambdas.
                math(EXPR _index "${_index} + 2")
                continue()
            endif()
        endif()
        if(_index GREATER 0)
            math(EXPR _previous_index "${_index} - 1")
            while(_previous_index GREATER_EQUAL 0)
                string(SUBSTRING "${CODE}" ${_previous_index} 1 _previous)
                if(NOT _previous MATCHES "[ \t\r\n]")
                    break()
                endif()
                math(EXPR _previous_index "${_previous_index} - 1")
            endwhile()
            # An identifier/index before the capture is an array access even
            # with whitespace (view.name [0]); these C++17 expression
            # keywords/operators may legally precede a lambda.
            if(_previous STREQUAL "]" OR _previous STREQUAL ")")
                math(EXPR _index "${_index} + 1")
                continue()
            endif()
            if("${_previous}" MATCHES "^[A-Za-z0-9_]$")
                set(_word_end ${_previous_index})
                while(_previous_index GREATER 0)
                    math(EXPR _word_start "${_previous_index} - 1")
                    string(SUBSTRING "${CODE}" ${_word_start} 1 _word_character)
                    if(NOT _word_character MATCHES "^[A-Za-z0-9_]$")
                        break()
                    endif()
                    set(_previous_index ${_word_start})
                endwhile()
                math(EXPR _word_length "${_word_end} - ${_previous_index} + 1")
                string(SUBSTRING "${CODE}" ${_previous_index} ${_word_length} _previous_word)
                if(NOT _previous_word STREQUAL "return" AND NOT _previous_word STREQUAL "throw"
                        AND NOT _previous_word STREQUAL "not" AND NOT _previous_word STREQUAL "and"
                        AND NOT _previous_word STREQUAL "or" AND NOT _previous_word STREQUAL "not_eq")
                    math(EXPR _index "${_index} + 1")
                    continue()
                endif()
            endif()
        endif()
        set(_capture_depth 1)
        math(EXPR _cursor "${_index} + 1")
        while(_cursor LESS _length AND _capture_depth GREATER 0)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _capture_character)
            if(_capture_character STREQUAL "[")
                math(EXPR _capture_depth "${_capture_depth} + 1")
            elseif(_capture_character STREQUAL "]")
                math(EXPR _capture_depth "${_capture_depth} - 1")
            endif()
            math(EXPR _cursor "${_cursor} + 1")
        endwhile()
        if(NOT _capture_depth EQUAL 0)
            math(EXPR _index "${_index} + 1")
            continue()
        endif()
        while(_cursor LESS _length)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _space)
            if(NOT _space MATCHES "[ \t\r\n]")
                break()
            endif()
            math(EXPR _cursor "${_cursor} + 1")
        endwhile()
        if(_cursor LESS _length)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _next)
            if(_next STREQUAL "(")
                set(_parameter_depth 1)
                math(EXPR _cursor "${_cursor} + 1")
                while(_cursor LESS _length AND _parameter_depth GREATER 0)
                    string(SUBSTRING "${CODE}" ${_cursor} 1 _parameter_character)
                    if(_parameter_character STREQUAL "(")
                        math(EXPR _parameter_depth "${_parameter_depth} + 1")
                    elseif(_parameter_character STREQUAL ")")
                        math(EXPR _parameter_depth "${_parameter_depth} - 1")
                    endif()
                    math(EXPR _cursor "${_cursor} + 1")
                endwhile()
            endif()
        endif()
        # A valid lambda body is the first brace after its optional declarator.
        # We stop at a statement separator so unrelated array expressions never
        # borrow a later block as their body.
        while(_cursor LESS _length)
            string(SUBSTRING "${CODE}" ${_cursor} 1 _suffix_character)
            if(_suffix_character STREQUAL "{")
                message(FATAL_ERROR "T6 ${CONTEXT} required behavior is hidden in a lambda")
            endif()
            if(_suffix_character STREQUAL ";")
                break()
            endif()
            math(EXPR _cursor "${_cursor} + 1")
        endwhile()
        math(EXPR _index "${_index} + 1")
    endwhile()
endfunction()

function(extract_live SOURCE MARKER OUT_BODY CONTEXT)
    file(READ "${SOURCE}" _source_text)
    extract_cpp_function_body("${_source_text}" "${MARKER}" _body _code)
    reject_dead_branch("${_code}" "${CONTEXT}")
    set(${OUT_BODY} "${_body}" PARENT_SCOPE)
endfunction()

foreach(_file IN LISTS _files)
    # Function-level checks below use the parser.  The remaining whole-file
    # checks deliberately operate on the real source and have exact tokens.
    set(_normal_${_file} "${_raw_${_file}}")
    set(_code_${_file} "${_raw_${_file}}")
endforeach()

# T0: a single catalog plus UTF-8-safe centered layout remains the common path.
foreach(_token IN ITEMS "ui_item_base_name" "ui_material_name" "ui_item_slot_name"
        "ui_item_rarity_name" "ui_item_affix_name" "ui_passive_node_name"
        "ui_monster_name" "ui_monster_affix_name" "ui_skill_name")
    require_token("${_normal_ui_text_catalog.hpp}" "${_token}" "catalog public API")
endforeach()
foreach(_token IN ITEMS "fit_centered_ui_text(" "ui_label_value_row_layout(")
    require_token("${_normal_ui_text_layout.cpp}" "${_token}" "UTF-8 layout implementation")
endforeach()

# T2 inventory and skill stones.
extract_live("${SOURCE_ROOT}/src/platform/raylib/inventory_view_math.cpp"
    "void copy_skill_name(" _skill "inventory catalog skill consumer")
require_token("${_skill}" "ui_skill_name(id)" "inventory catalog skill consumer")
extract_live("${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp"
    "void draw_button(" _button "inventory button")
reject_lambda("${_button}" "inventory button")
foreach(_token IN ITEMS "DrawRectangleRec(rectangle, fill)" "draw_centered_hud_font_text(" "rectangle")
    require_token("${_button}" "${_token}" "inventory button")
endforeach()
reject_token("${_button}" "MeasureTextEx(" "inventory button backing")
extract_live("${SOURCE_ROOT}/src/platform/raylib/inventory_renderer.cpp"
    "void InventoryRenderer::draw(" _inventory_draw "inventory live renderer")
foreach(_token IN ITEMS "ui_item_slot_name(slot)" "ui_item_rarity_name(item.rarity)"
        "ui_item_base_name(item.base_id)" "ui_item_affix_name(roll.affix_id)"
        "ui_material_name(*selected_material)" "draw_detail_row(")
    require_token("${_inventory_draw}" "${_token}" "inventory live renderer")
endforeach()
foreach(_bad IN ITEMS "->display_name" ".display_name" "base->name" "definition->name")
    reject_token("${_inventory_draw}" "${_bad}" "inventory live renderer")
endforeach()

# T3 loot, materials and only the two authorized HUD methods.
extract_live("${SOURCE_ROOT}/src/platform/raylib/ground_loot_view.cpp"
    "void format_label_text(" _ground_label "ground loot catalog label")
reject_lambda("${_ground_label}" "ground loot catalog label")
foreach(_token IN ITEMS "ui_item_rarity_name(item.rarity)" "ui_item_base_name(item.base_id)" "物品等级 %u")
    require_token("${_ground_label}" "${_token}" "ground loot catalog label")
endforeach()
reject_token("${_ground_label}" "base->name" "ground loot catalog label")
extract_live("${SOURCE_ROOT}/src/platform/raylib/loot_pickup_feedback.cpp"
    "LootPickupFeedback format_feedback(" _pickup_feedback "pickup catalog feedback")
reject_lambda("${_pickup_feedback}" "pickup catalog feedback")
foreach(_token IN ITEMS "ui_item_rarity_name(receipt.rarity)" "ui_item_base_name(receipt.base_id)" "物品等级 %u")
    require_token("${_pickup_feedback}" "${_token}" "pickup catalog feedback")
endforeach()
reject_token("${_pickup_feedback}" "base->name" "pickup catalog feedback")
extract_live("${SOURCE_ROOT}/src/platform/raylib/material_loot_view.cpp"
    "std::string_view material_label(" _material_label "material catalog label")
reject_lambda("${_material_label}" "material catalog label")
require_token("${_material_label}" "return ui_material_name(id);" "material catalog label")
extract_live("${SOURCE_ROOT}/src/platform/raylib/material_loot_view.cpp"
    "bool format_material_pickup_text(" _material_format "material UTF-8 aggregation")
reject_lambda("${_material_format}" "material UTF-8 aggregation")
foreach(_token IN ITEMS "ui_utf8_format(" "、等%zu种" "bytes.back() = '\\0'")
    require_token("${_material_format}" "${_token}" "material UTF-8 aggregation")
endforeach()
extract_live("${SOURCE_ROOT}/src/platform/raylib/material_loot_view.cpp"
    "bool append_complete_utf8(" _material_append "material UTF-8 append")
reject_lambda("${_material_append}" "material UTF-8 append")
require_token("${_material_append}" "ui_utf8_copy(" "material UTF-8 append")
reject_token("${_material_format}" "std::snprintf(destination.bytes.data()"
    "material UTF-8 aggregation")
extract_live("${SOURCE_ROOT}/src/platform/raylib/material_bag_renderer.cpp"
    "void draw_material_text(" _material_text "material bag centered text")
foreach(_token IN ITEMS "if (!ready || !IsFontValid(font)) return" "opaque_backing.a != 255U"
        "fit_centered_ui_text(" "ui_luma_contrast_ratio(color, opaque_backing) < 4.5F")
    require_token("${_material_text}" "${_token}" "material bag centered text")
endforeach()
foreach(_hud_marker IN ITEMS "void HudRenderer::draw_ground_loot(" "void HudRenderer::draw_material_loot(")
    extract_live("${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp" "${_hud_marker}" _hud "authorized loot HUD renderer")
    foreach(_token IN ITEMS "font_ready_" "IsFontValid(font_)" "{5, 9, 16, 255}"
            "fit_centered_ui_text(" "ui_luma_contrast_ratio(text, backing)")
        require_token("${_hud}" "${_token}" "authorized loot HUD renderer")
    endforeach()
    foreach(_bad IN ITEMS "GetFontDefault(" "Fade(" "DrawText(" "bounds.x + 8.0F")
        reject_token("${_hud}" "${_bad}" "authorized loot HUD renderer")
    endforeach()
endforeach()

# T4 passive tree.
extract_live("${SOURCE_ROOT}/src/platform/raylib/passive_tree_view_math.cpp"
    "PassiveNodeTextView passive_node_text_view(" _passive_view "passive catalog view")
reject_lambda("${_passive_view}" "passive catalog view")
foreach(_token IN ITEMS "ui_passive_node_name(node)" "ui_utf8_format(" "copy_complete(view.benefit")
    require_token("${_passive_view}" "${_token}" "passive catalog view")
endforeach()
extract_live("${SOURCE_ROOT}/src/platform/raylib/passive_tree_renderer.cpp"
    "void draw_centered_passive_tree_text(" _passive_text "passive centered text")
reject_lambda("${_passive_text}" "passive centered text")
foreach(_token IN ITEMS "opaque_backing.a != 255U" "ui_luma_contrast_ratio(color, opaque_backing) < 4.5F"
        "fit_centered_ui_text(" "layout.position")
    require_token("${_passive_text}" "${_token}" "passive centered text")
endforeach()
extract_live("${SOURCE_ROOT}/src/platform/raylib/passive_tree_renderer.cpp"
    "void draw_passive_tree_overlay(" _passive_draw "passive live renderer")
reject_lambda("${_passive_draw}" "passive live renderer")
foreach(_token IN ITEMS "passive_node_text_view(" "passive_node_name_bounds(" "ui_text(UiTextId::passive_title)" "layout.title")
    require_token("${_passive_draw}" "${_token}" "passive live renderer")
endforeach()
foreach(_bad IN ITEMS "source.name" "node.name" "GetFontDefault(" "DrawText(")
    reject_token("${_passive_view}${_passive_text}${_passive_draw}" "${_bad}" "passive player path")
endforeach()

# T5 death recap, including the only real combat call and the shared abyss table.
extract_live("${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.cpp"
    "const char* death_monster_name(" _death_monster "death monster catalog")
reject_lambda("${_death_monster}" "death monster catalog")
require_token("${_death_monster}" "ui_monster_name(" "death monster catalog")
extract_live("${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.cpp"
    "const char* death_affix_name(" _death_affix "death affix catalog")
reject_lambda("${_death_affix}" "death affix catalog")
require_token("${_death_affix}" "ui_monster_affix_name(" "death affix catalog")
extract_live("${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.cpp"
    "const char* death_abyss_rule_name(" _death_abyss "death abyss catalog")
reject_lambda("${_death_abyss}" "death abyss catalog")
require_token("${_death_abyss}" "abyss_rule_label(" "death abyss catalog")
extract_live("${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.cpp"
    "DeathOverlayView build_death_overlay_view(" _death_view "death formal view")
reject_lambda("${_death_view}" "death formal view")
extract_live("${SOURCE_ROOT}/src/platform/raylib/death_overlay_renderer.cpp"
    "void DeathOverlayRenderer::draw(" _death_draw "death formal renderer")
reject_lambda("${_death_draw}" "death formal renderer")
foreach(_token IN ITEMS "DeathOverlayTextDrawMode::shared_ui_font" "!IsFontValid(font_)"
        "build_death_overlay_view(snapshot, continue_binding_label)" "draw_text(font_, view.title.data()")
    require_token("${_death_draw}" "${_token}" "death formal renderer")
endforeach()
foreach(_bad IN ITEMS "build_death_overlay_ascii_view(" "GetFontDefault(" "DrawText(")
    reject_token("${_death_draw}" "${_bad}" "death formal renderer")
endforeach()
extract_live("${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp"
    "GroundLootView CombatRenderer::draw(" _combat_draw "combat death consumer")
reject_lambda("${_combat_draw}" "combat death consumer")
require_token("${_combat_draw}" "death_overlay_.draw(current_hud, material_pack_, interact_binding_label)" "combat death consumer")
extract_live("${SOURCE_ROOT}/src/platform/raylib/dungeon_view_math.cpp"
    "const char* abyss_rule_label(" _abyss_map "public abyss mapping")
reject_lambda("${_abyss_map}" "public abyss mapping")
require_token("${_abyss_map}" "u8\"雷暴\"" "public abyss mapping")

foreach(_file IN ITEMS material_loot_view.cpp material_bag_renderer.cpp ui_text_catalog.cpp)
    reject_token("${_code_${_file}}" "蜕变石" "legacy material spelling")
endforeach()
foreach(_text IN ITEMS "${_inventory_draw}" "${_ground_label}" "${_pickup_feedback}"
        "${_material_text}" "${_passive_view}" "${_passive_draw}" "${_death_view}")
    foreach(_english IN ITEMS "EQUIPMENT INVENTORY" "MATERIAL BAG" "PASSIVE STAR CHART" "DEATH RECAP" "Last hit" "Save failed")
        reject_token("${_text}" "${_english}" "active player text")
    endforeach()
endforeach()

message(STATUS "T6 full Chinese inventory/progression source guard accepted")
