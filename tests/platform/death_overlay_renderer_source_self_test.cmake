cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(_guard "${SOURCE_ROOT}/tests/platform/death_overlay_renderer_source_test.cmake")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/death_overlay_renderer.cpp")
set(_renderer_header "${SOURCE_ROOT}/src/platform/raylib/death_overlay_renderer.hpp")
set(_view "${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.cpp")
set(_view_header "${SOURCE_ROOT}/src/platform/raylib/death_overlay_view.hpp")
set(_dungeon "${SOURCE_ROOT}/src/platform/raylib/dungeon_view_math.cpp")
set(_dungeon_header "${SOURCE_ROOT}/src/platform/raylib/dungeon_view_math.hpp")
set(_combat "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
foreach(_path IN ITEMS _guard _renderer _renderer_header _view _view_header
        _dungeon _dungeon_header _combat)
    if(NOT EXISTS "${${_path}}")
        message(FATAL_ERROR "T5 self-test input is missing: ${${_path}}")
    endif()
endforeach()

file(READ "${_guard}" _guard_original)
file(READ "${_renderer}" _renderer_original)
file(READ "${_renderer_header}" _renderer_header_original)
file(READ "${_view}" _view_original)
file(READ "${_view_header}" _view_header_original)
file(READ "${_dungeon}" _dungeon_original)
file(READ "${_dungeon_header}" _dungeon_header_original)
file(READ "${_combat}" _combat_original)
string(REPLACE "\r\n" "\n" _guard_original "${_guard_original}")

file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
set_property(GLOBAL PROPERTY T5_DEATH_OVERLAY_MISSED_MUTATIONS "")

set(_dead_guard_function [=[function(reject_dead_branch CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES
            "if[ ]*(constexpr[ ]*)?\\([ ]*(false|0|0[ ]*==[ ]*1|1[ ]*==[ ]*0)[ ]*\\)"
            OR "${CODE}" MATCHES "#[ ]*if[ ]+0([^0-9]|$)"
            OR "${CODE}" MATCHES
                "if[ \\t\\r\\n]*\\([^\\)]*(false[ \\t]*&&|![ \\t]*true|0[ \\t]*!=[ \\t]*0|1[ \\t]*!=[ \\t]*1)[^\\)]*\\)"
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
    message(FATAL_ERROR "T5 dead-branch guard probe anchor is missing")
endif()
set(_guard_without_dead_check
    "${MUTATION_ROOT}/death_overlay_renderer_source_test_without_dead_check.cmake")
file(WRITE "${_guard_without_dead_check}" "${_guard_without_dead_check_text}")
file(COPY "${SOURCE_ROOT}/tests/platform/cpp_source_function_body.cmake"
    DESTINATION "${MUTATION_ROOT}")

function(run_guard_with_script LABEL RENDERER_TEXT RENDERER_HEADER_TEXT
        VIEW_TEXT VIEW_HEADER_TEXT DUNGEON_TEXT DUNGEON_HEADER_TEXT COMBAT_TEXT
        EXPECT_SUCCESS GUARD_SCRIPT)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}")
    set(_renderer_mutant "${_root}/death_overlay_renderer.cpp")
    set(_renderer_header_mutant "${_root}/death_overlay_renderer.hpp")
    set(_view_mutant "${_root}/death_overlay_view.cpp")
    set(_view_header_mutant "${_root}/death_overlay_view.hpp")
    set(_dungeon_mutant "${_root}/dungeon_view_math.cpp")
    set(_dungeon_header_mutant "${_root}/dungeon_view_math.hpp")
    set(_combat_mutant "${_root}/combat_renderer.cpp")
    file(WRITE "${_renderer_mutant}" "${RENDERER_TEXT}")
    file(WRITE "${_renderer_header_mutant}" "${RENDERER_HEADER_TEXT}")
    file(WRITE "${_view_mutant}" "${VIEW_TEXT}")
    file(WRITE "${_view_header_mutant}" "${VIEW_HEADER_TEXT}")
    file(WRITE "${_dungeon_mutant}" "${DUNGEON_TEXT}")
    file(WRITE "${_dungeon_header_mutant}" "${DUNGEON_HEADER_TEXT}")
    file(WRITE "${_combat_mutant}" "${COMBAT_TEXT}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DRENDERER_SOURCE=${_renderer_mutant}"
        "-DRENDERER_HEADER=${_renderer_header_mutant}"
        "-DVIEW_SOURCE=${_view_mutant}"
        "-DVIEW_HEADER=${_view_header_mutant}"
        "-DDUNGEON_SOURCE=${_dungeon_mutant}"
        "-DDUNGEON_HEADER=${_dungeon_header_mutant}"
        "-DCOMBAT_RENDERER_SOURCE=${_combat_mutant}"
        -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
        message(FATAL_ERROR "T5 guard unexpectedly rejected ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY
            T5_DEATH_OVERLAY_MISSED_MUTATIONS "${LABEL}")
    endif()
endfunction()

function(run_guard LABEL RENDERER_TEXT RENDERER_HEADER_TEXT VIEW_TEXT
        VIEW_HEADER_TEXT DUNGEON_TEXT DUNGEON_HEADER_TEXT COMBAT_TEXT
        EXPECT_SUCCESS)
    run_guard_with_script("${LABEL}" "${RENDERER_TEXT}"
        "${RENDERER_HEADER_TEXT}" "${VIEW_TEXT}" "${VIEW_HEADER_TEXT}"
        "${DUNGEON_TEXT}" "${DUNGEON_HEADER_TEXT}" "${COMBAT_TEXT}"
        "${EXPECT_SUCCESS}" "${_guard}")
endfunction()

function(expect_renderer_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated
        "${_renderer_original}")
    if(_mutated STREQUAL _renderer_original)
        message(FATAL_ERROR "T5 renderer mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_mutated}" "${_renderer_header_original}"
        "${_view_original}" "${_view_header_original}" "${_dungeon_original}"
        "${_dungeon_header_original}" "${_combat_original}" FALSE)
endfunction()

function(expect_header_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated
        "${_renderer_header_original}")
    if(_mutated STREQUAL _renderer_header_original)
        message(FATAL_ERROR "T5 header mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_renderer_original}" "${_mutated}"
        "${_view_original}" "${_view_header_original}" "${_dungeon_original}"
        "${_dungeon_header_original}" "${_combat_original}" FALSE)
endfunction()

function(expect_view_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_view_original}")
    if(_mutated STREQUAL _view_original)
        message(FATAL_ERROR "T5 view mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_renderer_original}"
        "${_renderer_header_original}" "${_mutated}" "${_view_header_original}"
        "${_dungeon_original}" "${_dungeon_header_original}"
        "${_combat_original}" FALSE)
endfunction()

function(expect_combat_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_combat_original}")
    if(_mutated STREQUAL _combat_original)
        message(FATAL_ERROR "T5 combat mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_renderer_original}"
        "${_renderer_header_original}" "${_view_original}"
        "${_view_header_original}" "${_dungeon_original}"
        "${_dungeon_header_original}" "${_mutated}" FALSE)
endfunction()

run_guard(pristine "${_renderer_original}" "${_renderer_header_original}"
    "${_view_original}" "${_view_header_original}" "${_dungeon_original}"
    "${_dungeon_header_original}" "${_combat_original}" TRUE)

set(_material_plan_block [=[    const DeathOverlayMaterialPlan plan =
        death_overlay_material_plan(view.visible);]=])
set(_material_plan_with_lambda_string [=[    const DeathOverlayMaterialPlan plan =
        death_overlay_material_plan(view.visible);
    const char* lambda_like_text = "[&]() noexcept { fake(); }";
    static_cast<void>(lambda_like_text);]=])
string(REPLACE "${_material_plan_block}"
    "${_material_plan_with_lambda_string}" _lambda_string_mutant
    "${_renderer_original}")
if(_lambda_string_mutant STREQUAL _renderer_original)
    message(FATAL_ERROR "T5 lambda-string control anchor is missing")
endif()
run_guard(lambda_syntax_in_string "${_lambda_string_mutant}"
    "${_renderer_header_original}" "${_view_original}"
    "${_view_header_original}" "${_dungeon_original}"
    "${_dungeon_header_original}" "${_combat_original}" TRUE)

set(_material_plan_with_lambda_comment [=[    const DeathOverlayMaterialPlan plan =
        death_overlay_material_plan(view.visible);
    /* [&]() noexcept { fake(); } */]=])
string(REPLACE "${_material_plan_block}"
    "${_material_plan_with_lambda_comment}" _lambda_comment_mutant
    "${_renderer_original}")
if(_lambda_comment_mutant STREQUAL _renderer_original)
    message(FATAL_ERROR "T5 lambda-comment control anchor is missing")
endif()
run_guard(lambda_syntax_in_comment "${_lambda_comment_mutant}"
    "${_renderer_header_original}" "${_view_original}"
    "${_view_header_original}" "${_dungeon_original}"
    "${_dungeon_header_original}" "${_combat_original}" TRUE)

string(REPLACE
    "    draw_text(font_, view.title.data(), layout.title,"
    "    const auto uncalled_draw = [&]() noexcept {\n        draw_text(font_, view.title.data(), layout.title,"
    _uncalled_lambda_opened "${_renderer_original}")
if(_uncalled_lambda_opened STREQUAL _renderer_original)
    message(FATAL_ERROR "T5 uncalled-lambda mutation opening anchor is missing")
endif()
set(_prompt_draw [=[    draw_text(font_, view.prompt.data(), layout.prompt,
        layout.prompt_font_size,
        snapshot.death->continue_failed ? style.error : style.warning, true);]=])
set(_uncalled_prompt_draw [=[        draw_text(font_, view.prompt.data(), layout.prompt,
            layout.prompt_font_size,
            snapshot.death->continue_failed ? style.error : style.warning, true);
    };
    static_cast<void>(uncalled_draw);]=])
string(REPLACE "${_prompt_draw}" "${_uncalled_prompt_draw}"
    _uncalled_lambda_mutant "${_uncalled_lambda_opened}")
if(_uncalled_lambda_mutant STREQUAL _uncalled_lambda_opened)
    message(FATAL_ERROR "T5 uncalled-lambda mutation closing anchor is missing")
endif()
run_guard(uncalled_lambda_dead_draw "${_uncalled_lambda_mutant}"
    "${_renderer_header_original}" "${_view_original}"
    "${_view_header_original}" "${_dungeon_original}"
    "${_dungeon_header_original}" "${_combat_original}" FALSE)
get_property(_uncalled_lambda_misses GLOBAL PROPERTY
    T5_DEATH_OVERLAY_MISSED_MUTATIONS)
if("uncalled_lambda_dead_draw" IN_LIST _uncalled_lambda_misses)
    message(FATAL_ERROR "T5 guard missed an uncalled lambda containing fake draw tokens")
endif()

expect_renderer_mutation_rejected(ascii_formal_reflow
    "build_death_overlay_view(snapshot, continue_binding_label)"
    "build_death_overlay_ascii_view(snapshot)")

expect_renderer_mutation_rejected(default_font_fallback
    "const DeathOverlayView view ="
    "const Font fallback = GetFontDefault();\n    static_cast<void>(fallback);\n    const DeathOverlayView view =")

set(_fail_closed_block [=[if (death_overlay_text_draw_mode(font_ready_)
            != DeathOverlayTextDrawMode::shared_ui_font
            || !IsFontValid(font_)) {
        return;
    }]=])
set(_missing_fail_closed_return [=[if (death_overlay_text_draw_mode(font_ready_)
            != DeathOverlayTextDrawMode::shared_ui_font
            || !IsFontValid(font_)) {
        static_cast<void>(0);
    }]=])
expect_renderer_mutation_rejected(missing_fail_closed_return
    "${_fail_closed_block}" "${_missing_fail_closed_return}")

set(_gate_then_view "${_fail_closed_block}\n    const DeathOverlayView view =")
set(_gate_then_early_return
    "${_fail_closed_block}\n    return;\n    const DeathOverlayView view =")
expect_renderer_mutation_rejected(unconditional_return_after_font_gate
    "${_gate_then_view}" "${_gate_then_early_return}")
get_property(_early_return_misses GLOBAL PROPERTY
    T5_DEATH_OVERLAY_MISSED_MUTATIONS)
if("unconditional_return_after_font_gate" IN_LIST _early_return_misses)
    message(FATAL_ERROR
        "T5 guard missed unconditional return after the valid font gate")
endif()

expect_renderer_mutation_rejected(unconditional_return_after_view
    "build_death_overlay_view(snapshot, continue_binding_label);\n    const DeathOverlayMaterialPlan plan ="
    "build_death_overlay_view(snapshot, continue_binding_label);\n    return;\n    const DeathOverlayMaterialPlan plan =")
expect_renderer_mutation_rejected(unconditional_return_after_visible_gate
    "if (!plan.visible) return;\n    const DeathOverlayLayout layout ="
    "if (!plan.visible) return;\n    return;\n    const DeathOverlayLayout layout =")

expect_view_mutation_rejected(active_english_title
    "u8\"死亡回顾\"" "\"DEATH RECAP\"")
expect_view_mutation_rejected(active_english_source
    "u8\"怪物攻击\"" "\"MONSTER ATTACK\"")
expect_view_mutation_rejected(active_english_damage
    "u8\"火焰伤害\"" "\"FIRE DAMAGE\"")

expect_view_mutation_rejected(copied_monster_rule_table
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "constexpr std::array<const char*, 1U> copied{{u8\"复制怪物\"}};\n    return copied[0];")
expect_view_mutation_rejected(copied_affix_rule_table
    "return ui_monster_affix_name(static_cast<combat::MonsterAffixId>(id));"
    "constexpr std::array<const char*, 1U> copied{{u8\"复制词缀\"}};\n    return copied[0];")
expect_view_mutation_rejected(copied_abyss_rule_table
    "return abyss_rule_label(static_cast<abyss::AbyssRuleId>(id));"
    "constexpr std::array<const char*, 1U> copied{{u8\"复制规则\"}};\n    return copied[0];")

expect_view_mutation_rejected(monster_switch_table_fallback
    "return ui_monster_name(static_cast<combat::MonsterId>(id));"
    "switch (id) { case 0U: return u8\"复制怪物\"; default: break; }\n    return ui_monster_name(static_cast<combat::MonsterId>(id));")
expect_view_mutation_rejected(affix_switch_table_fallback
    "return ui_monster_affix_name(static_cast<combat::MonsterAffixId>(id));"
    "switch (id) { case 0U: return u8\"复制词缀\"; default: break; }\n    return ui_monster_affix_name(static_cast<combat::MonsterAffixId>(id));")
expect_view_mutation_rejected(abyss_switch_table_fallback
    "return abyss_rule_label(static_cast<abyss::AbyssRuleId>(id));"
    "switch (id) { case 0U: return u8\"复制规则\"; default: break; }\n    return abyss_rule_label(static_cast<abyss::AbyssRuleId>(id));")

expect_header_mutation_rejected(transparent_text_backing
    "Color backing{5, 9, 16, 255};"
    "Color backing{5, 9, 16, 232};")
expect_header_mutation_rejected(low_contrast_primary
    "Color primary{248, 246, 238, 255};"
    "Color primary{9, 12, 18, 255};")

expect_renderer_mutation_rejected(text_width_backing
    "DrawRectangleRec(rectangle(bounds), style.backing);"
    "DrawRectangleRec({bounds.x, bounds.y, MeasureTextEx(font, text, 16.0F, 1.0F).x, bounds.height}, style.backing);")

expect_renderer_mutation_rejected(non_centered_title
    "style.danger, true);"
    "style.danger, false);")
expect_renderer_mutation_rejected(non_centered_continue
    "snapshot.death->continue_failed ? style.error : style.warning, true);"
    "snapshot.death->continue_failed ? style.error : style.warning, false);")
expect_renderer_mutation_rejected(missing_body_line_draw
    "index < view.line_count; ++index)"
    "index < 0U; ++index)")

expect_renderer_mutation_rejected(hardcoded_continue_binding
    "build_death_overlay_view(snapshot, continue_binding_label)"
    "build_death_overlay_view(snapshot, \"E\")")
expect_combat_mutation_rejected(missing_dynamic_binding_forward
    "death_overlay_.draw(current_hud, material_pack_, interact_binding_label);"
    "death_overlay_.draw(current_hud, material_pack_);")

foreach(_dead_condition IN ITEMS "false" "false && font_ready_" "!true" "0 != 0")
    string(MAKE_C_IDENTIFIER "${_dead_condition}" _dead_label)
    set(_live_view [=[const DeathOverlayView view =
        build_death_overlay_view(snapshot, continue_binding_label);]=])
    set(_dead_view_decoy "${_live_view}\n    if (${_dead_condition}) {\n        const DeathOverlayView dead_view =\n            build_death_overlay_view(snapshot, continue_binding_label);\n        static_cast<void>(dead_view);\n    }")
    string(REPLACE
        "${_live_view}"
        "${_dead_view_decoy}" _dead_mutant "${_renderer_original}")
    if(_dead_mutant STREQUAL _renderer_original)
        message(FATAL_ERROR "T5 renderer mutation anchor is missing: dead ${_dead_condition}")
    endif()
    run_guard("dead_formal_view_${_dead_label}" "${_dead_mutant}"
        "${_renderer_header_original}" "${_view_original}"
        "${_view_header_original}" "${_dungeon_original}"
        "${_dungeon_header_original}" "${_combat_original}" FALSE)
    if(_dead_condition STREQUAL "false")
        run_guard_with_script(dead_formal_view_without_dead_check
            "${_dead_mutant}" "${_renderer_header_original}"
            "${_view_original}" "${_view_header_original}"
            "${_dungeon_original}" "${_dungeon_header_original}"
            "${_combat_original}" TRUE "${_guard_without_dead_check}")
    endif()
endforeach()

get_property(_missed_mutations GLOBAL PROPERTY
    T5_DEATH_OVERLAY_MISSED_MUTATIONS)
if(_missed_mutations)
    list(FILTER _missed_mutations EXCLUDE REGEX "^$")
endif()
if(_missed_mutations)
    list(JOIN _missed_mutations ", " _missed_labels)
    message(FATAL_ERROR "T5 guard missed mutations: ${_missed_labels}")
endif()

message(STATUS "T5 death-overlay renderer source guard self-test accepted")
