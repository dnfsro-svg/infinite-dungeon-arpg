if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()

set(_guard
    "${SOURCE_ROOT}/tests/platform/stage11d_renderer_integration_guard_test.cmake")
file(READ "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.hpp" _header)
file(READ "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp" _combat)
file(READ "${SOURCE_ROOT}/src/platform/raylib/room_renderer.cpp" _room)
file(READ "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp" _hud)
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

function(stage11d_replace_required OUT_VAR SOURCE BEFORE AFTER LABEL)
    string(FIND "${SOURCE}" "${BEFORE}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D guard mutation replacement failed closed: ${LABEL}")
    endif()
    string(REPLACE "${BEFORE}" "${AFTER}" _mutated "${SOURCE}")
    if("${_mutated}" STREQUAL "${SOURCE}")
        message(FATAL_ERROR
            "Stage11D guard mutation did not change source: ${LABEL}")
    endif()
    set(${OUT_VAR} "${_mutated}" PARENT_SCOPE)
endfunction()

function(stage11d_run_guard_case LABEL COMBAT ROOM HUD EXPECT_PASS EXPECT_REASON)
    set(_root "${GUARD_TEST_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}/src/platform/raylib")
    file(WRITE "${_root}/src/platform/raylib/combat_renderer.hpp" "${_header}")
    file(WRITE "${_root}/src/platform/raylib/combat_renderer.cpp" "${COMBAT}")
    file(WRITE "${_root}/src/platform/raylib/room_renderer.cpp" "${ROOM}")
    file(WRITE "${_root}/src/platform/raylib/hud_renderer.cpp" "${HUD}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${_root}" -P "${_guard}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(_output "${_stdout}${_stderr}")
    if(EXPECT_PASS)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard rejected equivalent ${LABEL} variant: ${_output}")
        endif()
    else()
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard accepted bad ${LABEL} mutation")
        endif()
        if(NOT _output MATCHES "${EXPECT_REASON}")
            message(FATAL_ERROR
                "Stage11D ${LABEL} mutation failed for wrong reason; expected ${EXPECT_REASON}: ${_output}")
        endif()
    endif()
endfunction()

set(_camera_line "    const CameraOffset camera_offset = feedback.camera_offset();")
set(_duplicate_factory
    "    static_cast<void>(make_combat_render_plan(current, loot_filter_mode_, 1.0F, 1.0F));\n${_camera_line}")
stage11d_replace_required(_combat_duplicate "${_combat}"
    "${_camera_line}" "${_duplicate_factory}" duplicate_factory)
stage11d_run_guard_case(duplicate_factory "${_combat_duplicate}" "${_room}"
    "${_hud}" FALSE "factory.*exactly once")

set(_outer_close "}  // namespace arpg::platform")
set(_room_rebuild
    "void stage11d_room_rebuild_mutation(const dungeon::DungeonSnapshot& snapshot) {\n    static_cast<void>(build_ground_loot_view(snapshot, settings::LootFilterMode::show_all, 1.0F, 1.0F));\n}\n\n${_outer_close}")
stage11d_replace_required(_room_duplicate "${_room}"
    "${_outer_close}" "${_room_rebuild}" room_rebuild)
stage11d_run_guard_case(room_rebuild "${_combat}" "${_room_duplicate}"
    "${_hud}" FALSE "room renderer.*rebuild")

set(_hud_rebuild
    "void stage11d_hud_rebuild_mutation(const dungeon::DungeonSnapshot& snapshot) {\n    static_cast<void>(build_ground_loot_view(snapshot, settings::LootFilterMode::show_all, 1.0F, 1.0F));\n}\n\n${_outer_close}")
stage11d_replace_required(_hud_duplicate "${_hud}"
    "${_outer_close}" "${_hud_rebuild}" hud_rebuild)
stage11d_run_guard_case(hud_rebuild "${_combat}" "${_room}"
    "${_hud_duplicate}" FALSE "HUD renderer.*rebuild")

set(_hud_direct
    "            hud_renderer_.draw_ground_loot(render_plan.ground_loot);")
set(_hud_split
    "            const GroundLootView split_ground_loot = render_plan.ground_loot;\n            hud_renderer_.draw_ground_loot(split_ground_loot);")
stage11d_replace_required(_combat_split "${_combat}"
    "${_hud_direct}" "${_hud_split}" consumer_split)
stage11d_run_guard_case(consumer_split "${_combat_split}" "${_room}"
    "${_hud}" FALSE "same GroundLootView|consumer.*diverg")

stage11d_replace_required(_combat_renamed_declaration "${_combat}"
    "CombatRenderPlan render_plan =" "CombatRenderPlan frame_plan ="
    local_plan_declaration_rename)
stage11d_replace_required(_combat_renamed "${_combat_renamed_declaration}"
    "render_plan." "frame_plan." local_plan_use_rename)
set(_renamed_room
    "            draw_room(current, frame_plan.ground_loot, frame_plan.material_loot);")
set(_formatted_room
    "            draw_room(\n                current,\n                frame_plan.ground_loot,\n                frame_plan.material_loot); ")
stage11d_replace_required(_combat_formatted "${_combat_renamed}"
    "${_renamed_room}" "${_formatted_room}" harmless_formatting)
stage11d_run_guard_case(rename_and_format "${_combat_formatted}" "${_room}"
    "${_hud}" TRUE "")

set(_renamed_camera
    "    const CameraOffset camera_offset = feedback.camera_offset();")
set(_alias_declaration
    "    const GroundLootView& shared_ground_loot = frame_plan.ground_loot;\n${_renamed_camera}")
stage11d_replace_required(_combat_alias "${_combat_renamed}"
    "${_renamed_camera}" "${_alias_declaration}" const_reference_alias)
stage11d_replace_required(_combat_alias_room "${_combat_alias}"
    "frame_plan.ground_loot, frame_plan.material_loot);"
    "shared_ground_loot, frame_plan.material_loot);" alias_room_consumer)
stage11d_replace_required(_combat_alias_consumers "${_combat_alias_room}"
    "frame_plan.ground_loot);" "shared_ground_loot);" alias_hud_consumer)
stage11d_run_guard_case(const_reference_alias "${_combat_alias_consumers}" "${_room}"
    "${_hud}" TRUE "")

message(STATUS
    "[stage11d-renderer-guard-self-test] bad_mutations=4 equivalent_variants=2")
