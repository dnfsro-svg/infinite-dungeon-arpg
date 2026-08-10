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
    "draw_crisp_ui_text(font, text, layout.text_position,\n        font_size, 1.0F, color);"
    "DrawTextEx(font, text, layout.text_position, font_size, 1.0F, color);")
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
    "draw_crisp_ui_text(font, text, layout.text_position,\n        font_size, 1.0F, color);"
    "if (false) { draw_crisp_ui_text(font, text, layout.text_position, font_size, 1.0F, color); }")

if(MISSED_MUTATIONS)
    list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
    message(FATAL_ERROR "hole prompt guard missed mutations: ${MISSED_LABELS}")
endif()
