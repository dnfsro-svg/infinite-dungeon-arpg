cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(_guard "${SOURCE_ROOT}/tests/platform/task7_debug_overlay_source_test.cmake")
set(_source_dir "${SOURCE_ROOT}/src/platform/raylib")
set(_names OVERLAY_HEADER OVERLAY_SOURCE DEBUG_SOURCE ACTOR_SOURCE
    COMBAT_SOURCE VIEW_MATH_HEADER VIEW_MATH_SOURCE)
set(_files debug_overlay_renderer.hpp debug_overlay_renderer.cpp
    debug_renderer.cpp actor_renderer.cpp combat_renderer.cpp
    combat_view_math.hpp combat_view_math.cpp)
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")

set(_index 0)
foreach(_name IN LISTS _names)
    list(GET _files ${_index} _file)
    file(READ "${_source_dir}/${_file}" _text)
    set(_original_${_name} "${_text}")
    math(EXPR _index "${_index} + 1")
endforeach()

function(run_guard LABEL EXPECT_SUCCESS)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}")
    set(_args "-DSOURCE_ROOT=${SOURCE_ROOT}")
    set(_index 0)
    foreach(_name IN LISTS _names)
        list(GET _files ${_index} _file)
        if(DEFINED _mutated_${_name})
            set(_text "${_mutated_${_name}}")
        else()
            set(_text "${_original_${_name}}")
        endif()
        set(_path "${_root}/${_file}")
        file(WRITE "${_path}" "${_text}")
        list(APPEND _args "-D${_name}=${_path}")
        math(EXPR _index "${_index} + 1")
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${_args} -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
        message(FATAL_ERROR "Task 7 guard rejected pristine source")
    endif()
    if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
        message(FATAL_ERROR "Task 7 guard accepted mutation: ${LABEL}")
    endif()
endfunction()

run_guard(pristine TRUE)

function(expect_mutation_rejected LABEL SOURCE_NAME NEEDLE REPLACEMENT)
    set(_source "${_original_${SOURCE_NAME}}")
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}" _mutated "${_source}")
    if(_mutated STREQUAL _source)
        message(FATAL_ERROR "Task 7 mutation anchor is missing: ${LABEL}")
    endif()
    set(_mutated_${SOURCE_NAME} "${_mutated}")
    run_guard("${LABEL}" FALSE)
endfunction()

expect_mutation_rejected(missing_world_label_backing DEBUG_SOURCE
    "DrawRectangleRec(layout.backing, style.backing_color);"
    "static_cast<void>(layout.backing);")
expect_mutation_rejected(default_world_label_font DEBUG_SOURCE
    "draw_crisp_ui_text(shared_ui_font, label, layout.text_position,
            12.0F, 0.5F, style.text_color, 1);"
    "DrawTextEx(GetFontDefault(), label, layout.text_position,
            12.0F, 0.5F, style.text_color);")
expect_mutation_rejected(dead_world_label_backing DEBUG_SOURCE
    "DrawRectangleRec(layout.backing, style.backing_color);"
    "if (shared_ui_font_ready && false) {
            DrawRectangleRec(layout.backing, style.backing_color);
        }")
expect_mutation_rejected(unclamped_world_label_layout DEBUG_SOURCE
    "const SceneLabelLayout layout = scene_label_layout(
            {corners[4].x + 4.0F, corners[4].y - 14.0F},
            measured, width, height, style);"
    "const SceneLabelLayout layout{
            {corners[4].x + 4.0F, corners[4].y - 14.0F,
                measured.x, measured.y},
            {corners[4].x + 4.0F, corners[4].y - 14.0F}};")
expect_mutation_rejected(monster_label_text_before_backing ACTOR_SOURCE
    "DrawRectangleRec(layout.backing, style.backing_color);
    draw_crisp_ui_text(font, text, layout.text_position,
        static_cast<float>(font_size), 1.0F, style.text_color);"
    "draw_crisp_ui_text(font, text, layout.text_position,
        static_cast<float>(font_size), 1.0F, style.text_color);
    DrawRectangleRec(layout.backing, style.backing_color);")
expect_mutation_rejected(default_monster_label_font ACTOR_SOURCE
    "draw_crisp_ui_text(font, text, layout.text_position,
        static_cast<float>(font_size), 1.0F, style.text_color);"
    "DrawTextEx(GetFontDefault(), text, layout.text_position,
        static_cast<float>(font_size), 1.0F, style.text_color);")
expect_mutation_rejected(dead_monster_label_backing ACTOR_SOURCE
    "DrawRectangleRec(layout.backing, style.backing_color);"
    "if (IsFontValid(font) && false) {
        DrawRectangleRec(layout.backing, style.backing_color);
    }")

expect_mutation_rejected(stack_capacity_expansion VIEW_MATH_HEADER
    "inline constexpr std::size_t kMonsterSceneLabelCapacity = 5U;"
    "inline constexpr std::size_t kMonsterSceneLabelCapacity = 15U;")
expect_mutation_rejected(normal_mode_includes_debug_labels ACTOR_SOURCE
    "monster_scene_label_sequence(draw_debug, affix_count);"
    "monster_scene_label_sequence(true, affix_count);")
expect_mutation_rejected(missing_visible_resource_bar_exclusion ACTOR_SOURCE
    "visible_resource_bars[visible_resource_bar_count++] = bar_bounds;"
    "static_cast<void>(bar_bounds);")
expect_mutation_rejected(missing_actor_exclusion ACTOR_SOURCE
    "monster_actor_label_exclusion(projected),
            presentation.debug_label_lane, scale, width, height);"
    "Rectangle{},
            presentation.debug_label_lane, scale, width, height);")
expect_mutation_rejected(hardcoded_stack_lane ACTOR_SOURCE
    "presentation.debug_label_lane, scale, width, height);"
    "0U, scale, width, height);")
expect_mutation_rejected(missing_real_text_measurement ACTOR_SOURCE
    "MeasureTextEx(hud_font, label_texts[index],
                static_cast<float>(label_font_sizes[index]), 1.0F),"
    "Vector2{0.0F, 0.0F},")
expect_mutation_rejected(independent_plate_viewport_clamp VIEW_MATH_SOURCE
    "            plate_y += plate_heights[index] + kMinimumPlateGap;"
    "            layout.plates[index] = scene_label_layout(
                {plate_x, plate_y}, entry.text_size,
                viewport_width, viewport_height, entry.style);
            plate_y += plate_heights[index] + kMinimumPlateGap;")
expect_mutation_rejected(reordered_stack_candidates VIEW_MATH_SOURCE
    "        {exclusion_center_x - stack_width * 0.5F,
            exclusion.y - kMinimumPlateGap - stack_height - lane_offset},
        {exclusion_center_x - stack_width * 0.5F,
            exclusion.y + exclusion.height + kMinimumPlateGap + lane_offset},"
    "        {exclusion_center_x - stack_width * 0.5F,
            exclusion.y + exclusion.height + kMinimumPlateGap + lane_offset},
        {exclusion_center_x - stack_width * 0.5F,
            exclusion.y - kMinimumPlateGap - stack_height - lane_offset},")
expect_mutation_rejected(no_safe_candidate_fails_open VIEW_MATH_SOURCE
    "        layout.visible = true;
        return layout;
    }
    return layout;
}"
    "        layout.visible = true;
        return layout;
    }
    layout.visible = true;
    return layout;
}")

expect_mutation_rejected(unsafe_affix_textformat ACTOR_SOURCE
    "            static_cast<void>(ui_utf8_format(
                affix_text.data(), affix_text.size(),
                u8\"%s %s\", badge.short_name, badge.tier_text));
            label_texts[index] = affix_text.data();"
    "            label_texts[index] = TextFormat(
                \"%s %s\", badge.short_name, badge.tier_text);")

expect_mutation_rejected(line_capacity OVERLAY_HEADER
    "std::array<DebugOverlayLine, 12U> lines{};"
    "std::array<DebugOverlayLine, 11U> lines{};")
expect_mutation_rejected(dynamic_format OVERLAY_SOURCE
    "ui_utf8_format(view.lines[0].text.data(),"
    "std::snprintf(view.lines[0].text.data(),")
expect_mutation_rejected(wrong_breakpoint OVERLAY_SOURCE
    "screen_width < 1280 ? 1U : 2U"
    "screen_width < 1279 ? 1U : 2U")
expect_mutation_rejected(translucent_backing OVERLAY_SOURCE
    "layout.backing_color = {0x0B, 0x10, 0x19, 255};"
    "layout.backing_color = {0x0B, 0x10, 0x19, 200};")
expect_mutation_rejected(missing_column_backing OVERLAY_SOURCE
    "DrawRectangleRec(layout.column_backings[column], layout.backing_color);"
    "static_cast<void>(layout.column_backings[column]);")
expect_mutation_rejected(default_overlay_text OVERLAY_SOURCE
    "draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data(),
            {bounds.x, bounds.y + 2.0F}, layout.font_size,
            layout.spacing, view.lines[index].color, 1);"
    "DrawTextEx(shared_ui_font, view.lines[index].text.data(),
            {bounds.x, bounds.y + 2.0F}, layout.font_size,
            layout.spacing, view.lines[index].color);")
expect_mutation_rejected(dead_shared_font_decoy OVERLAY_SOURCE
    "draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data(),
            {bounds.x, bounds.y + 2.0F}, layout.font_size,
            layout.spacing, view.lines[index].color, 1);"
    "if (false) {
            draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data(),
                {bounds.x, bounds.y + 2.0F}, layout.font_size,
                layout.spacing, view.lines[index].color, 1);
        }")
expect_mutation_rejected(compound_false_shared_font_decoy OVERLAY_SOURCE
    "draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data(),
            {bounds.x, bounds.y + 2.0F}, layout.font_size,
            layout.spacing, view.lines[index].color, 1);"
    "if (shared_ui_font_ready && false) {
            draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data(),
                {bounds.x, bounds.y + 2.0F}, layout.font_size,
                layout.spacing, view.lines[index].color, 1);
        }")
expect_mutation_rejected(unshared_overlay_call COMBAT_SOURCE
    "debug_overlay_.draw(world_ui_font_, world_ui_font_ready_,"
    "debug_overlay_.draw(Font{}, true,")
expect_mutation_rejected(hardcoded_world_label DEBUG_SOURCE
    "debug_world_volume_label(DebugWorldVolumeKind::assist)"
    "\"Assist\"")
expect_mutation_rejected(hardcoded_monster_role VIEW_MATH_SOURCE
    "MonsterShapeId::bomber, ui_monster_name(id), MonsterWarningMode::none"
    "MonsterShapeId::bomber, \"BOMBER\", MonsterWarningMode::none")
expect_mutation_rejected(english_monster_phase VIEW_MATH_SOURCE
    "case combat::MonsterAiPhase::idle: return u8\"待机\";"
    "case combat::MonsterAiPhase::idle: return \"Idle\";")
expect_mutation_rejected(english_affix_tier VIEW_MATH_SOURCE
    "case combat::MonsterAffixTier::m1: return u8\"词缀Ⅰ\";"
    "case combat::MonsterAffixTier::m1: return \"M1\";")
expect_mutation_rejected(english_defeat_marker ACTOR_SOURCE
    "constexpr const char* kLabel = u8\"已击败\";"
    "constexpr const char* kLabel = \"DEFEATED\";")

message(STATUS "Task 7 debug overlay source guard self-test rejected all mutations")
