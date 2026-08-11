cmake_minimum_required(VERSION 3.25)

foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
        UI_TEXT_RENDERER_SOURCE UI_TEXT_BOUNDS_AUDIT_SOURCE
        GUARD_SCRIPT MUTATION_ROOT)
    if(NOT DEFINED ${REQUIRED_INPUT}
            OR ("${${REQUIRED_INPUT}}" STREQUAL "")
            OR (NOT REQUIRED_INPUT STREQUAL "MUTATION_ROOT"
                AND NOT EXISTS "${${REQUIRED_INPUT}}"))
        message(FATAL_ERROR "HUD renderer guard self-test input missing: ${REQUIRED_INPUT}")
    endif()
endforeach()

file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
file(READ "${HUD_RENDERER_SOURCE}" HUD_ORIGINAL)
file(READ "${ACTIVE_SKILL_RENDERER_SOURCE}" ACTIVE_ORIGINAL)
file(READ "${UI_TEXT_RENDERER_SOURCE}" TEXT_RENDERER_ORIGINAL)
file(READ "${UI_TEXT_BOUNDS_AUDIT_SOURCE}" TEXT_AUDIT_ORIGINAL)
set(MISSED_MUTATIONS)

set(DUNGEON_FIXTURE_TEXT
    "void fixture(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); renderer.observe_hud(previous, current, status, hints, 0.0F, false); }\n")
set(LOOT_FIXTURE_TEXT
    "platform::ControlHints control_hints(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); return hints; }\nvoid fixture(){ renderer.observe_hud(previous, current, status, control_hints(), 0.0F, false); }\n")
set(STAGE12_FIXTURE_TEXT
    "void fixture(){ platform::ControlHints control_hints{}; platform::refresh_control_hints(control_hints, arpg::settings::default_settings()); renderer.observe_hud(previous, current, status, control_hints, 0.0F, false); }\n")
set(STAGE16_FIXTURE_TEXT
    "${DUNGEON_FIXTURE_TEXT}${DUNGEON_FIXTURE_TEXT}")

function(run_guard LABEL HUD_TEXT ACTIVE_TEXT EXPECT_SUCCESS)
    set(ROOT "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${ROOT}")
    set(HUD_MUTANT "${ROOT}/hud_renderer.cpp")
    set(ACTIVE_MUTANT "${ROOT}/active_skill_renderer.cpp")
    set(TEXT_RENDERER_MUTANT "${ROOT}/ui_text_renderer.cpp")
    set(TEXT_AUDIT_MUTANT "${ROOT}/ui_text_bounds_audit.cpp")
    set(DUNGEON_FIXTURE "${ROOT}/dungeon.cpp")
    set(LOOT_FIXTURE "${ROOT}/loot.cpp")
    set(STAGE12_FIXTURE "${ROOT}/stage12.cpp")
    set(STAGE16_FIXTURE "${ROOT}/stage16.cpp")
    file(WRITE "${HUD_MUTANT}" "${HUD_TEXT}")
    file(WRITE "${ACTIVE_MUTANT}" "${ACTIVE_TEXT}")
    set(TEXT_RENDERER_TEXT "${TEXT_RENDERER_ORIGINAL}")
    set(TEXT_AUDIT_TEXT "${TEXT_AUDIT_ORIGINAL}")
    if(ARGC GREATER 4)
        set(TEXT_RENDERER_TEXT "${ARGV4}")
    endif()
    if(ARGC GREATER 5)
        set(TEXT_AUDIT_TEXT "${ARGV5}")
    endif()
    file(WRITE "${TEXT_RENDERER_MUTANT}" "${TEXT_RENDERER_TEXT}")
    file(WRITE "${TEXT_AUDIT_MUTANT}" "${TEXT_AUDIT_TEXT}")
    set(CURRENT_DUNGEON_FIXTURE_TEXT "${DUNGEON_FIXTURE_TEXT}")
    set(CURRENT_LOOT_FIXTURE_TEXT "${LOOT_FIXTURE_TEXT}")
    set(CURRENT_STAGE12_FIXTURE_TEXT "${STAGE12_FIXTURE_TEXT}")
    set(CURRENT_STAGE16_FIXTURE_TEXT "${STAGE16_FIXTURE_TEXT}")
    if(DEFINED FIXTURE_OVERRIDE)
        set(CURRENT_DUNGEON_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
        set(CURRENT_LOOT_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
        set(CURRENT_STAGE12_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
    endif()
    if(DEFINED DUNGEON_FIXTURE_OVERRIDE)
        set(CURRENT_DUNGEON_FIXTURE_TEXT "${DUNGEON_FIXTURE_OVERRIDE}")
    endif()
    if(DEFINED LOOT_FIXTURE_OVERRIDE)
        set(CURRENT_LOOT_FIXTURE_TEXT "${LOOT_FIXTURE_OVERRIDE}")
    endif()
    if(DEFINED STAGE12_FIXTURE_OVERRIDE)
        set(CURRENT_STAGE12_FIXTURE_TEXT "${STAGE12_FIXTURE_OVERRIDE}")
    endif()
    if(DEFINED STAGE16_FIXTURE_OVERRIDE)
        set(CURRENT_STAGE16_FIXTURE_TEXT "${STAGE16_FIXTURE_OVERRIDE}")
    endif()
    file(WRITE "${DUNGEON_FIXTURE}" "${CURRENT_DUNGEON_FIXTURE_TEXT}")
    file(WRITE "${LOOT_FIXTURE}" "${CURRENT_LOOT_FIXTURE_TEXT}")
    file(WRITE "${STAGE12_FIXTURE}" "${CURRENT_STAGE12_FIXTURE_TEXT}")
    file(WRITE "${STAGE16_FIXTURE}" "${CURRENT_STAGE16_FIXTURE_TEXT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHUD_RENDERER_SOURCE=${HUD_MUTANT}"
            "-DACTIVE_SKILL_RENDERER_SOURCE=${ACTIVE_MUTANT}"
            "-DUI_TEXT_RENDERER_SOURCE=${TEXT_RENDERER_MUTANT}"
            "-DUI_TEXT_BOUNDS_AUDIT_SOURCE=${TEXT_AUDIT_MUTANT}"
            "-DDUNGEON_RUNTIME_SOURCE=${DUNGEON_FIXTURE}"
            "-DLOOT_SUCTION_SOURCE=${LOOT_FIXTURE}"
            "-DSTAGE12_SOURCE=${STAGE12_FIXTURE}"
            "-DSTAGE16_SOURCE=${STAGE16_FIXTURE}"
            -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE RESULT
        OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
        message(FATAL_ERROR "HUD renderer guard rejected clean sources: ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
        list(APPEND MISSED_MUTATIONS "${LABEL}")
        set(MISSED_MUTATIONS "${MISSED_MUTATIONS}" PARENT_SCOPE)
    endif()
endfunction()

function(replace_required INPUT MATCH REPLACEMENT OUTPUT LABEL)
    string(FIND "${INPUT}" "${MATCH}" MATCH_INDEX)
    if(MATCH_INDEX EQUAL -1)
        message(FATAL_ERROR "Self-test mutation did not match: ${LABEL}")
    endif()
    string(REPLACE "${MATCH}" "${REPLACEMENT}" RESULT "${INPUT}")
    if(RESULT STREQUAL INPUT)
        message(FATAL_ERROR "Self-test mutation did not change source: ${LABEL}")
    endif()
    set(${OUTPUT} "${RESULT}" PARENT_SCOPE)
endfunction()

run_guard(clean "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" TRUE)

replace_required("${HUD_ORIGINAL}" "bar.label.bytes.data()"
    "legacy_bar_label(bar)" HUD_MUTATED "legacy HUD label replacement")
set(HUD_COMPLETE_DECOY
    "if (false) {\n                record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font,\n                    bar.label.bytes.data(), label_position,\n                    style.player_bar_font_size * layout.scale,\n                    1.0F, label_container,\n                    style.player_bar_font_size * layout.scale, &blocker, 1U);\n                draw_hud_text(draw_font, bar.label.bytes.data(), label_position,\n                    style.player_bar_font_size * layout.scale,\n                    palette.text, style.outline_pixels);\n            }\n            ")
replace_required("${HUD_MUTATED}"
    "record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font,"
    "${HUD_COMPLETE_DECOY}record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font,"
    HUD_MUTATED "HUD dead-branch insertion")
run_guard(hud_complete_dead_calls_with_live_legacy_path
    "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)


macro(run_fixture_reset_mutant LABEL RESET_HINTS RESET_CONTROL_HINTS)
    set(DUNGEON_FIXTURE_OVERRIDE
        "void fixture(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); ${RESET_HINTS} renderer.observe_hud(previous, current, status, hints, 0.0F, false); }")
    set(LOOT_FIXTURE_OVERRIDE
        "platform::ControlHints control_hints(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); ${RESET_HINTS} return hints; } void fixture(){ renderer.observe_hud(previous, current, status, control_hints(), 0.0F, false); }")
    set(STAGE12_FIXTURE_OVERRIDE
        "void fixture(){ platform::ControlHints control_hints{}; platform::refresh_control_hints(control_hints, arpg::settings::default_settings()); ${RESET_CONTROL_HINTS} renderer.observe_hud(previous, current, status, control_hints, 0.0F, false); }")
    set(STAGE16_FIXTURE_OVERRIDE
        "${DUNGEON_FIXTURE_OVERRIDE}${DUNGEON_FIXTURE_OVERRIDE}")
    run_guard("${LABEL}" "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE)
    unset(DUNGEON_FIXTURE_OVERRIDE)
    unset(LOOT_FIXTURE_OVERRIDE)
    unset(STAGE12_FIXTURE_OVERRIDE)
    unset(STAGE16_FIXTURE_OVERRIDE)
endmacro()

run_fixture_reset_mutant(refresh_then_brace_assignment
    "hints = {};" "control_hints = {};")
run_fixture_reset_mutant(refresh_then_typed_reassignment
    "hints = platform::ControlHints{};"
    "control_hints = platform::ControlHints{};")
run_fixture_reset_mutant(refresh_then_memset_clear
    "std::memset(&hints, 0, sizeof(hints));"
    "std::memset(&control_hints, 0, sizeof(control_hints));")

replace_required("${HUD_ORIGINAL}" "bar.label.bytes.data()" "u8\"生命\""
    HUD_MUTATED "HUD hardcoded label replacement")
set(HUD_DECOY
    "if (false) {\n        static_cast<void>(bar.label.bytes.data());\n        static_cast<void>(bar.label.bytes.data());\n    }\n    ")
replace_required("${HUD_MUTATED}"
    "const HudLayout& layout, const MaterialPack& assets) const noexcept {\n    if (!IsWindowReady()) return;"
    "const HudLayout& layout, const MaterialPack& assets) const noexcept {\n    ${HUD_DECOY}if (!IsWindowReady()) return;"
    HUD_MUTATED "HUD dead-code decoy insertion")
run_guard(hud_dead_correct_decoy_with_real_hardcode
    "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)


set(FIXTURE_OVERRIDE
    "void fixture(){ platform::ControlHints initialized{}; platform::refresh_control_hints(initialized, arpg::settings::default_settings()); platform::ControlHints hints{}; renderer.observe_hud(previous, current, status, hints, 0.0F, false); }\n")
set(STAGE16_FIXTURE_OVERRIDE "${FIXTURE_OVERRIDE}${FIXTURE_OVERRIDE}")
run_guard(refreshes_unused_hints_but_passes_uninitialized
    "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE)
unset(FIXTURE_OVERRIDE)
unset(STAGE16_FIXTURE_OVERRIDE)

replace_required("${HUD_ORIGINAL}" "bar.label.bytes.data()" "u8\"生命\""
    HUD_MUTATED "HUD ignored-label replacement")
set(HUD_MUTATED
    "${HUD_MUTATED}\nvoid decoy_bar_label_use(){ bar.label.bytes.data(); }\n")
run_guard(hud_ignores_bar_label "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)


replace_required("${ACTIVE_ORIGINAL}" "if (!layout.valid) return;" ""
    ACTIVE_MUTATED "remove early invalid-layout guard")
replace_required("${ACTIVE_MUTATED}" "if (!material_drawn) {"
    "if (!layout.valid) return;\n        if (!material_drawn) {"
    ACTIVE_MUTATED "move invalid-layout guard after material draw")
run_guard(invalid_layout_guard_moved_after_first_material_draw
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

foreach(PLATE_NAME IN ITEMS key cooldown name)
    replace_required("${ACTIVE_ORIGINAL}"
        "draw_active_skill_text_plate(hud_font, text_plan.${PLATE_NAME});"
        "static_cast<void>(text_plan.${PLATE_NAME});"
        ACTIVE_MUTATED "remove active-skill ${PLATE_NAME} plate draw")
    run_guard(active_skill_${PLATE_NAME}_plate_removed
        "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
endforeach()

replace_required("${ACTIVE_ORIGINAL}" "plan.position" "Vector2{0.0F, 0.0F}"
    ACTIVE_MUTATED "replace plan position with fixed offset")
run_guard(active_skill_plate_uses_fixed_text_offset
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

replace_required("${ACTIVE_ORIGINAL}" "nullptr, 0U, 0);" "nullptr, 0U, 2);"
    ACTIVE_MUTATED "replace audit zero shadow")
run_guard(active_skill_plate_audit_uses_shadow
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

replace_required("${ACTIVE_ORIGINAL}" "ui_text_contrast_style().primary, 0);"
    "ui_text_contrast_style().primary, 2);" ACTIVE_MUTATED
    "replace draw zero shadow")
run_guard(active_skill_plate_draw_uses_shadow
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

replace_required("${TEXT_RENDERER_ORIGINAL}" "if (shadow_pixels > 0) {"
    "if (true) {" TEXT_RENDERER_MUTATED "make shadow draw unconditional")
run_guard(unconditional_crisp_text_shadow
    "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE
    "${TEXT_RENDERER_MUTATED}")

replace_required("${TEXT_AUDIT_ORIGINAL}"
    "style.outline_pixels, effective_shadow"
    "style.outline_pixels, style.shadow_pixels" TEXT_AUDIT_MUTATED
    "ignore explicit audit shadow")
run_guard(audit_ignores_explicit_shadow
    "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE
    "${TEXT_RENDERER_ORIGINAL}" "${TEXT_AUDIT_MUTATED}")

replace_required("${ACTIVE_ORIGINAL}"
    "draw_active_skill_text_plate(hud_font, text_plan.key);"
    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
    "replace live active-skill key plate draw")
set(ACTIVE_LAMBDA_DECOY
    "auto hidden = [&]() { draw_active_skill_text_plate(hud_font, text_plan.key); };\n        ")
replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
    "${ACTIVE_LAMBDA_DECOY}legacy_draw(text_plan.key);"
    ACTIVE_MUTATED "insert uncalled active-skill lambda decoy")
run_guard(active_skill_calls_hidden_in_uncalled_lambda
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

replace_required("${ACTIVE_ORIGINAL}"
    "draw_active_skill_text_plate(hud_font, text_plan.key);"
    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
    "replace live key plate draw for dead-code decoy")
set(ACTIVE_FALSE_DECOY
    "if (false) { draw_active_skill_text_plate(hud_font, text_plan.key); }\n        ")
replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
    "${ACTIVE_FALSE_DECOY}legacy_draw(text_plan.key);" ACTIVE_MUTATED
    "insert constant-false active-skill key plate decoy")
run_guard(active_skill_key_call_hidden_in_constant_false
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

replace_required("${ACTIVE_ORIGINAL}"
    "draw_active_skill_text_plate(hud_font, text_plan.key);"
    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
    "replace live key plate draw for if constexpr decoy")
set(ACTIVE_CONSTEXPR_FALSE_DECOY
    "if constexpr (false) { draw_active_skill_text_plate(hud_font, text_plan.key); }\n        ")
replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
    "${ACTIVE_CONSTEXPR_FALSE_DECOY}legacy_draw(text_plan.key);"
    ACTIVE_MUTATED "insert if constexpr false active-skill key plate decoy")
run_guard(active_skill_key_call_hidden_in_if_constexpr_false
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

replace_required("${ACTIVE_ORIGINAL}"
    "draw_active_skill_text_plate(hud_font, text_plan.key);"
    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
    "replace live key plate draw for auto reference lambda decoy")
set(ACTIVE_AUTO_REF_LAMBDA_DECOY
    "auto&& hidden = [&]() { draw_active_skill_text_plate(hud_font, text_plan.key); };\n        ")
replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
    "${ACTIVE_AUTO_REF_LAMBDA_DECOY}legacy_draw(text_plan.key);"
    ACTIVE_MUTATED "insert auto reference uncalled active-skill lambda decoy")
run_guard(active_skill_key_call_hidden_in_auto_ref_lambda
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

set(TEXT_PLAN_DRAW_BLOCK
    "        const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan(\n            slot, layout.text_regions[index], layout.scale,\n            hud_font_ready && IsFontValid(hud_font),\n            &measure_active_skill_text, &measure_context);\n        draw_active_skill_text_plate(hud_font, text_plan.key);\n        draw_active_skill_text_plate(hud_font, text_plan.cooldown);\n        draw_active_skill_text_plate(hud_font, text_plan.name);\n")
replace_required("${ACTIVE_ORIGINAL}" "${TEXT_PLAN_DRAW_BLOCK}" ""
    ACTIVE_MUTATED "remove live text plan and plate draw block")
replace_required("${ACTIVE_MUTATED}"
    "        if (!slot.empty) {\n            const float inset ="
    "${TEXT_PLAN_DRAW_BLOCK}        if (!slot.empty) {\n            const float inset ="
    ACTIVE_MUTATED "move text plan and plates before icon and cooldown artwork")
run_guard(active_skill_plan_before_icon_and_cooldown_artwork
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

if(MISSED_MUTATIONS)
    list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
    message(FATAL_ERROR
        "HUD renderer guard missed mutations: ${MISSED_LABELS}")
endif()
