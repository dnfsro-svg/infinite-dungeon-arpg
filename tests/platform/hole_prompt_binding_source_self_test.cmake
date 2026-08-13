cmake_minimum_required(VERSION 3.25)

foreach(REQUIRED_INPUT IN ITEMS RAYLIB_SOURCE_DIR GUARD_SCRIPT MUTATION_ROOT)
    if(NOT DEFINED ${REQUIRED_INPUT} OR "${${REQUIRED_INPUT}}" STREQUAL "")
        message(FATAL_ERROR "hole prompt guard self-test input missing: ${REQUIRED_INPUT}")
    endif()
endforeach()

file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
file(READ "${RAYLIB_SOURCE_DIR}/room_renderer.cpp" ROOM_ORIGINAL)
file(READ "${RAYLIB_SOURCE_DIR}/combat_renderer.cpp" COMBAT_ORIGINAL)
set(MISSED_MUTATIONS)

function(run_guard LABEL ROOM_TEXT COMBAT_TEXT EXPECT_SUCCESS)
    set(ROOT "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${ROOT}")
    set(ROOM_MUTANT "${ROOT}/room_renderer.cpp")
    set(COMBAT_MUTANT "${ROOT}/combat_renderer.cpp")
    file(WRITE "${ROOM_MUTANT}" "${ROOM_TEXT}")
    file(WRITE "${COMBAT_MUTANT}" "${COMBAT_TEXT}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DRAYLIB_SOURCE_DIR=${RAYLIB_SOURCE_DIR}"
        "-DROOM_RENDERER_SOURCE=${ROOM_MUTANT}"
        "-DCOMBAT_RENDERER_SOURCE=${COMBAT_MUTANT}"
        -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE RESULT OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
        message(FATAL_ERROR "hole prompt guard rejected clean source: ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
        list(APPEND MISSED_MUTATIONS "${LABEL}")
        set(MISSED_MUTATIONS "${MISSED_MUTATIONS}" PARENT_SCOPE)
    endif()
endfunction()

run_guard(clean "${ROOM_ORIGINAL}" "${COMBAT_ORIGINAL}" TRUE)

function(replace_required INPUT NEEDLE REPLACEMENT OUTPUT LABEL)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" MUTATED "${INPUT}")
    if(MUTATED STREQUAL INPUT)
        message(FATAL_ERROR "${LABEL}: mutation anchor is missing")
    endif()
    set(${OUTPUT} "${MUTATED}" PARENT_SCOPE)
endfunction()

macro(run_room_mutant LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" ROOM_MUTATED "${ROOM_ORIGINAL}")
    if(ROOM_MUTATED STREQUAL ROOM_ORIGINAL)
        message(FATAL_ERROR "${LABEL}: mutation anchor is missing")
    endif()
    run_guard("${LABEL}" "${ROOM_MUTATED}" "${COMBAT_ORIGINAL}" FALSE)
endmacro()

run_room_mutant(hardcoded_interact_key
    "interact_binding_label, HoleInteractionAction::descend"
    "\"E\", HoleInteractionAction::descend")
run_room_mutant(missing_backing
    "DrawRectangleRec(layout.backing, raylib_color(style.backing));"
    "static_cast<void>(layout.backing);")
run_room_mutant(missing_contrast_correction
    "color = raylib_color(world_prompt_text_color(\n        {color.r, color.g, color.b, color.a}));"
    "color.a = 255U;")
run_room_mutant(default_text_draw
    "draw_crisp_ui_text(font, text, layout.text_position,\n        font_size * layout.text_scale,\n        1.0F * layout.text_scale, color);"
    "DrawTextEx(font, text, layout.text_position, font_size * layout.text_scale, 1.0F * layout.text_scale, color);")
run_room_mutant(legacy_text_measure
    "MeasureTextEx(font, text, font_size, 1.0F)"
    "Vector2{static_cast<float>(MeasureText(text, static_cast<int>(font_size))), font_size}")

string(REPLACE "world_ui_font_ = shared_ui_font.font();"
    "world_ui_font_ = GetFontDefault();" COMBAT_MUTATED "${COMBAT_ORIGINAL}")
if(COMBAT_MUTATED STREQUAL COMBAT_ORIGINAL)
    message(FATAL_ERROR "shared font mutation anchor is missing")
endif()
run_guard(default_font_owner "${ROOM_ORIGINAL}" "${COMBAT_MUTATED}" FALSE)

run_room_mutant(dead_cjk_decoy
    "draw_crisp_ui_text(font, text, layout.text_position,\n        font_size * layout.text_scale,\n        1.0F * layout.text_scale, color);"
    "if (false) { draw_crisp_ui_text(font, text, layout.text_position, font_size * layout.text_scale, 1.0F * layout.text_scale, color); }")

run_room_mutant(abyss_overlay_default_font
    "draw_abyss(current, static_cast<float>(GetTime()),\n        world_ui_font_, world_ui_font_ready_);"
    "draw_abyss(current, static_cast<float>(GetTime()),\n        GetFontDefault(), true);")
run_room_mutant(abyss_overlay_dead_shared_font_decoy
    "draw_abyss(current, static_cast<float>(GetTime()),\n        world_ui_font_, world_ui_font_ready_);"
    "if (false) { draw_abyss(current, static_cast<float>(GetTime()), world_ui_font_, world_ui_font_ready_); }\n    draw_abyss(current, static_cast<float>(GetTime()), GetFontDefault(), true);")

run_room_mutant(material_success_uses_fallback_frame
    "        ? material_destination : frame;"
    "        ? frame : frame;")
run_room_mutant(slot_only_blocker
    "active_skill_layout.visual_bounds"
    "active_skill_layout.bounds")

replace_required("${COMBAT_ORIGINAL}"
    "            draw_room(world, render_plan.ground_loot,\n                render_plan.material_loot, camera, interact_binding_label,\n                {camera_offset.x, camera_offset.y});"
    "            draw_room(world, render_plan.ground_loot,\n                render_plan.material_loot, camera, interact_binding_label,\n                {});"
    COMBAT_MUTATED camera_offset_dropped)
run_guard(camera_offset_dropped "${ROOM_ORIGINAL}" "${COMBAT_MUTATED}" FALSE)

run_room_mutant(door_layout_bypassed
    "    const WorldPromptLayout layout = door_bounds != nullptr\n        ? door_world_prompt_layout(anchor, measured, *door_bounds,\n            hud_blocker, screen_offset, screen_width, screen_height)\n        : world_prompt_layout(\n            anchor, measured, screen_width, screen_height);"
    "    const WorldPromptLayout layout = world_prompt_layout(\n        anchor, measured, screen_width, screen_height);")

set(ROOM_MUTATED "${ROOM_ORIGINAL}")
replace_required("${ROOM_MUTATED}"
    "    const WorldPromptLayout layout = door_bounds != nullptr\n        ? door_world_prompt_layout(anchor, measured, *door_bounds,\n            hud_blocker, screen_offset, screen_width, screen_height)\n        : world_prompt_layout(\n            anchor, measured, screen_width, screen_height);"
    "    WorldPromptLayout layout{};\n    if constexpr (false) {\n        layout = door_bounds != nullptr\n            ? door_world_prompt_layout(anchor, measured, *door_bounds,\n                hud_blocker, screen_offset, screen_width, screen_height)\n            : world_prompt_layout(\n                anchor, measured, screen_width, screen_height);\n    }\n    layout = world_prompt_layout(\n        anchor, measured, screen_width, screen_height);"
    ROOM_MUTATED door_requirements_in_constexpr_false_world_prompt)
replace_required("${ROOM_MUTATED}"
    "    const Rectangle hud_blocker = active_skill_layout.valid\n        ? active_skill_layout.visual_bounds : Rectangle{};"
    "    const Rectangle hud_blocker = active_skill_layout.valid\n        ? active_skill_layout.bounds : Rectangle{};\n    if constexpr (false) {\n        const Rectangle intended_hud_blocker = active_skill_layout.valid\n            ? active_skill_layout.visual_bounds : Rectangle{};\n        static_cast<void>(intended_hud_blocker);\n    }"
    ROOM_MUTATED door_requirements_in_constexpr_false_blocker)
replace_required("${ROOM_MUTATED}"
    "        const Rectangle door_bounds = material_drawn\n            && material_destination.width > 0.0F\n            && material_destination.height > 0.0F\n            ? material_destination : frame;"
    "        const Rectangle door_bounds = frame;\n        if constexpr (false) {\n            const Rectangle intended_door_bounds = material_drawn\n                && material_destination.width > 0.0F\n                && material_destination.height > 0.0F\n                ? material_destination : frame;\n            static_cast<void>(intended_door_bounds);\n        }"
    ROOM_MUTATED door_requirements_in_constexpr_false_bounds)
replace_required("${ROOM_MUTATED}"
    "        draw_world_prompt(visual.label,\n            {projected.x, frame.y - label_size - 14.0F},\n            label_size, text_color, font, font_ready, width, height,\n            &door_bounds, hud_blocker, screen_offset);"
    "        if constexpr (false) {\n            draw_world_prompt(visual.label,\n                {projected.x, frame.y - label_size - 14.0F},\n                label_size, text_color, font, font_ready, width, height,\n                &door_bounds, hud_blocker, screen_offset);\n        }\n        draw_world_prompt(visual.label,\n            {projected.x, frame.y - label_size - 14.0F},\n            label_size, text_color, font, font_ready, width, height);"
    ROOM_MUTATED door_requirements_in_constexpr_false_prompt)
replace_required("${ROOM_MUTATED}"
    "    draw_doors(current, camera, width, height, material_pack_,\n        world_ui_font_, world_ui_font_ready_, screen_offset);"
    "    if constexpr (false) {\n        draw_doors(current, camera, width, height, material_pack_,\n            world_ui_font_, world_ui_font_ready_, screen_offset);\n    }\n    draw_doors(current, camera, width, height, material_pack_,\n        world_ui_font_, world_ui_font_ready_, {});"
    ROOM_MUTATED door_requirements_in_constexpr_false_room)
set(COMBAT_MUTATED "${COMBAT_ORIGINAL}")
replace_required("${COMBAT_MUTATED}"
    "            draw_room(world, render_plan.ground_loot,\n                render_plan.material_loot, camera, interact_binding_label,\n                {camera_offset.x, camera_offset.y});"
    "            if constexpr (false) {\n                draw_room(world, render_plan.ground_loot,\n                    render_plan.material_loot, camera, interact_binding_label,\n                    {camera_offset.x, camera_offset.y});\n            }\n            draw_room(world, render_plan.ground_loot,\n                render_plan.material_loot, camera, interact_binding_label,\n                {});"
    COMBAT_MUTATED door_requirements_in_constexpr_false_combat)
run_guard(door_requirements_in_constexpr_false
    "${ROOM_MUTATED}" "${COMBAT_MUTATED}" FALSE)

set(ROOM_MUTATED "${ROOM_ORIGINAL}")
replace_required("${ROOM_MUTATED}"
    "    const WorldPromptLayout layout = door_bounds != nullptr\n        ? door_world_prompt_layout(anchor, measured, *door_bounds,\n            hud_blocker, screen_offset, screen_width, screen_height)\n        : world_prompt_layout(\n            anchor, measured, screen_width, screen_height);"
    "    WorldPromptLayout layout{};\n    auto&& door_layout_requirement = [&]() noexcept {\n        layout = door_bounds != nullptr\n            ? door_world_prompt_layout(anchor, measured, *door_bounds,\n                hud_blocker, screen_offset, screen_width, screen_height)\n            : world_prompt_layout(\n                anchor, measured, screen_width, screen_height);\n    };\n    static_cast<void>(door_layout_requirement);\n    layout = world_prompt_layout(\n        anchor, measured, screen_width, screen_height);"
    ROOM_MUTATED door_requirements_in_uncalled_lambda_world_prompt)
replace_required("${ROOM_MUTATED}"
    "    const Rectangle hud_blocker = active_skill_layout.valid\n        ? active_skill_layout.visual_bounds : Rectangle{};"
    "    const Rectangle hud_blocker = active_skill_layout.valid\n        ? active_skill_layout.bounds : Rectangle{};\n    auto&& blocker_requirement = [&]() noexcept {\n        const Rectangle intended_hud_blocker = active_skill_layout.valid\n            ? active_skill_layout.visual_bounds : Rectangle{};\n        static_cast<void>(intended_hud_blocker);\n    };\n    static_cast<void>(blocker_requirement);"
    ROOM_MUTATED door_requirements_in_uncalled_lambda_blocker)
replace_required("${ROOM_MUTATED}"
    "        const Rectangle door_bounds = material_drawn\n            && material_destination.width > 0.0F\n            && material_destination.height > 0.0F\n            ? material_destination : frame;"
    "        const Rectangle door_bounds = frame;\n        auto&& bounds_requirement = [&]() noexcept {\n            const Rectangle intended_door_bounds = material_drawn\n                && material_destination.width > 0.0F\n                && material_destination.height > 0.0F\n                ? material_destination : frame;\n            static_cast<void>(intended_door_bounds);\n        };\n        static_cast<void>(bounds_requirement);"
    ROOM_MUTATED door_requirements_in_uncalled_lambda_bounds)
replace_required("${ROOM_MUTATED}"
    "        draw_world_prompt(visual.label,\n            {projected.x, frame.y - label_size - 14.0F},\n            label_size, text_color, font, font_ready, width, height,\n            &door_bounds, hud_blocker, screen_offset);"
    "        auto&& prompt_requirement = [&]() noexcept {\n            draw_world_prompt(visual.label,\n                {projected.x, frame.y - label_size - 14.0F},\n                label_size, text_color, font, font_ready, width, height,\n                &door_bounds, hud_blocker, screen_offset);\n        };\n        static_cast<void>(prompt_requirement);\n        draw_world_prompt(visual.label,\n            {projected.x, frame.y - label_size - 14.0F},\n            label_size, text_color, font, font_ready, width, height);"
    ROOM_MUTATED door_requirements_in_uncalled_lambda_prompt)
replace_required("${ROOM_MUTATED}"
    "    draw_doors(current, camera, width, height, material_pack_,\n        world_ui_font_, world_ui_font_ready_, screen_offset);"
    "    auto&& door_requirement = [&]() noexcept {\n        draw_doors(current, camera, width, height, material_pack_,\n            world_ui_font_, world_ui_font_ready_, screen_offset);\n    };\n    static_cast<void>(door_requirement);\n    draw_doors(current, camera, width, height, material_pack_,\n        world_ui_font_, world_ui_font_ready_, {});"
    ROOM_MUTATED door_requirements_in_uncalled_lambda_room)
set(COMBAT_MUTATED "${COMBAT_ORIGINAL}")
replace_required("${COMBAT_MUTATED}"
    "            draw_room(world, render_plan.ground_loot,\n                render_plan.material_loot, camera, interact_binding_label,\n                {camera_offset.x, camera_offset.y});"
    "            auto&& camera_requirement = [&]() noexcept {\n                draw_room(world, render_plan.ground_loot,\n                    render_plan.material_loot, camera, interact_binding_label,\n                    {camera_offset.x, camera_offset.y});\n            };\n            static_cast<void>(camera_requirement);\n            draw_room(world, render_plan.ground_loot,\n                render_plan.material_loot, camera, interact_binding_label,\n                {});"
    COMBAT_MUTATED door_requirements_in_uncalled_lambda_combat)
run_guard(door_requirements_in_uncalled_lambda
    "${ROOM_MUTATED}" "${COMBAT_MUTATED}" FALSE)

if(MISSED_MUTATIONS)
    list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
    message(FATAL_ERROR "hole prompt guard missed mutations: ${MISSED_LABELS}")
endif()
