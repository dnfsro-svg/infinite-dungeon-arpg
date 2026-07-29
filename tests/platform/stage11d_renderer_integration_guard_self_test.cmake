if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()

set(_guard
    "${SOURCE_ROOT}/tests/platform/stage11d_renderer_integration_guard_test.cmake")
file(READ "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.hpp" _header)
file(READ "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp" _combat)
file(READ "${SOURCE_ROOT}/src/platform/raylib/room_renderer.cpp" _room)
file(READ "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp" _hud)
file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" _host)
file(READ "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp"
    _report)
string(REPLACE "\r\n" "\n" _host "${_host}")
string(REPLACE "\r\n" "\n" _report "${_report}")
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
    file(MAKE_DIRECTORY "${_root}")
    set(_header_path "${_root}/combat_renderer.hpp")
    set(_combat_path "${_root}/combat_renderer.cpp")
    set(_room_path "${_root}/room_renderer.cpp")
    set(_hud_path "${_root}/hud_renderer.cpp")
    file(WRITE "${_header_path}" "${_header}")
    file(WRITE "${_combat_path}" "${COMBAT}")
    file(WRITE "${_room_path}" "${ROOM}")
    file(WRITE "${_hud_path}" "${HUD}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DCOMBAT_HEADER_OVERRIDE=${_header_path}"
            "-DCOMBAT_SOURCE_OVERRIDE=${_combat_path}"
            "-DROOM_SOURCE_OVERRIDE=${_room_path}"
            "-DHUD_SOURCE_OVERRIDE=${_hud_path}"
            -DTASK5B_RENDERER_ONLY=ON -P "${_guard}"
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

function(stage11d_run_report_guard_case LABEL REPORT EXPECT_PASS EXPECT_REASON)
    set(_path "${GUARD_TEST_ROOT}/${LABEL}-report.cpp")
    file(WRITE "${_path}" "${REPORT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DREPORT_OVERRIDE=${_path}" -DTASK5B_REPORT_ONLY=ON
            -P "${_guard}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(_output "${_stdout}${_stderr}")
    if(EXPECT_PASS)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard rejected equivalent ${LABEL} report variant: ${_output}")
        endif()
    else()
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard accepted bad ${LABEL} report mutation")
        endif()
        if(NOT _output MATCHES "${EXPECT_REASON}")
            message(FATAL_ERROR
                "Stage11D ${LABEL} report mutation failed for wrong reason; expected ${EXPECT_REASON}: ${_output}")
        endif()
    endif()
endfunction()

function(stage11d_run_host_guard_case LABEL HOST EXPECT_PASS EXPECT_REASON)
    set(_path "${GUARD_TEST_ROOT}/${LABEL}-host.cpp")
    file(WRITE "${_path}" "${HOST}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_OVERRIDE=${_path}" -P "${_guard}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(_output "${_stdout}${_stderr}")
    if(EXPECT_PASS)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard rejected equivalent ${LABEL} host variant: ${_output}")
        endif()
    else()
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard accepted bad ${LABEL} host mutation")
        endif()
        if(NOT _output MATCHES "${EXPECT_REASON}")
            message(FATAL_ERROR
                "Stage11D ${LABEL} host mutation failed for wrong reason; expected ${EXPECT_REASON}: ${_output}")
        endif()
    endif()
endfunction()

set(_stage11d_semantic_call
    "stage11d_record_semantics(stage11d_validation_state, current,\n                    runtime.item_state(), ground_loot_view,\n                    renderer.hud_notice_view());")
function(stage11d_run_summary_binding_mutations)
    stage11d_replace_required(_report_affix_producer_constant "${_report}"
        "combat::monster_affix_danger_score(monster.affixes)" "0U"
        report_monster_affix_danger_producer_constant)
    stage11d_run_report_guard_case(
        report_monster_affix_danger_producer_constant
        "${_report_affix_producer_constant}" FALSE
        "monster_affix_danger producer binding")

    stage11d_replace_required(_report_ai_producer_constant "${_report}"
        "static_cast<std::uint8_t>(\n                monster.ai_phase)" "0U"
        report_monster_ai_phase_producer_constant)
    stage11d_run_report_guard_case(
        report_monster_ai_phase_producer_constant
        "${_report_ai_producer_constant}" FALSE
        "monster_ai_phase producer binding")

    stage11d_replace_required(_report_affix_output_constant "${_report}"
        "state.monster_affix_danger[0]" "0U"
        report_monster_affix_danger_output_constant)
    stage11d_run_report_guard_case(
        report_monster_affix_danger_output_constant
        "${_report_affix_output_constant}" FALSE
        "monster_affix_danger output binding")

    stage11d_replace_required(_report_ai_output_constant "${_report}"
        "static_cast<unsigned>(\n                state.monster_ai_phase[0])" "0U"
        report_monster_ai_phase_output_constant)
    stage11d_run_report_guard_case(
        report_monster_ai_phase_output_constant
        "${_report_ai_output_constant}" FALSE
        "monster_ai_phase output binding")

    stage11d_replace_required(_report_defeat_hp_output_constant "${_report}"
        "state.defeat_player_hp[0]" "0"
        report_defeat_player_hp_output_constant)
    stage11d_run_report_guard_case(
        report_defeat_player_hp_output_constant
        "${_report_defeat_hp_output_constant}" FALSE
        "defeat_player_hp output binding")

    stage11d_replace_required(_report_target_ordinal_output_constant "${_report}"
        "state.target_ordinal" "0U"
        report_target_ordinal_output_constant)
    stage11d_run_report_guard_case(
        report_target_ordinal_output_constant
        "${_report_target_ordinal_output_constant}" FALSE
        "target_ordinal output binding")

    set(_report_result_formula
        "(state.captured\n            && (config.stage11d_loot_validation\n                    != Stage11DLootValidationScenario::rare_only_abyss\n                || state.abyss_claimed) ? \"pass\" : \"fail\")")
    stage11d_replace_required(_report_result_constant "${_report}"
        "${_report_result_formula}" "\"pass\""
        report_result_constant_pass)
    stage11d_run_report_guard_case(report_result_constant_pass
        "${_report_result_constant}" FALSE "result output binding")

endfunction()
if(DEFINED TASK5B_SUMMARY_BINDINGS_ONLY AND TASK5B_SUMMARY_BINDINGS_ONLY)
    stage11d_run_summary_binding_mutations()
    message(STATUS
        "[stage11d-renderer-task5b-summary-bindings] named_mutations=7")
    return()
endif()
if(DEFINED TASK5B_ORDER_ONLY AND TASK5B_ORDER_ONLY)
    stage11d_replace_required(_host_without_semantic_call "${_host}"
        "${_stage11d_semantic_call}" "" host_semantic_after_capture_remove)
    stage11d_replace_required(_host_semantic_after_capture
        "${_host_without_semantic_call}"
        "stage11d_validation_state.captured = true;"
        "stage11d_validation_state.captured = true;\n                ${_stage11d_semantic_call}"
        host_semantic_after_capture_insert)
    stage11d_run_host_guard_case(host_semantic_after_capture_order_decoy
        "${_host_semantic_after_capture}" FALSE "production run-chain order")
    message(STATUS
        "[stage11d-renderer-task5b-order] named_mutations=1")
    return()
endif()
if(NOT DEFINED TASK5B_REPORT_CASES_ONLY OR NOT TASK5B_REPORT_CASES_ONLY)
set(_stage11d_semantic_lambda
    "const auto task5b_semantic_decoy = [&]() noexcept {\n                    ${_stage11d_semantic_call}\n                };")
stage11d_replace_required(_host_semantic_lambda "${_host}"
    "${_stage11d_semantic_call}" "${_stage11d_semantic_lambda}"
    host_semantic_uncalled_lambda)
stage11d_run_host_guard_case(host_semantic_uncalled_lambda
    "${_host_semantic_lambda}" FALSE "production run-chain token scope")

if(DEFINED TASK5B_RED_ONLY AND TASK5B_RED_ONLY)
    message(STATUS "[stage11d-renderer-task5b-red] named_mutations=1")
    return()
endif()
endif()

stage11d_replace_required(_report_record_comment "${_report}"
    "void stage11d_record_semantics("
    "// void stage11d_record_semantics(\nvoid task5b_record_removed("
    report_record_comment_decoy)
stage11d_run_report_guard_case(report_record_comment_decoy
    "${_report_record_comment}" FALSE "semantic recorder.*definition")

stage11d_replace_required(_report_target_string "${_report}"
    "bool stage11d_target_visible("
    "const char* task5b_target_string = \"bool stage11d_target_visible(\";\nbool task5b_target_removed("
    report_target_string_decoy)
stage11d_run_report_guard_case(report_target_string_decoy
    "${_report_target_string}" FALSE "target-visible evaluator.*definition")

stage11d_replace_required(_report_summary_forward "${_report}"
    "void write_stage11d_loot_validation_summary("
    "void write_stage11d_loot_validation_summary();\nvoid task5b_summary_removed("
    report_summary_forward_declaration_decoy)
stage11d_run_report_guard_case(report_summary_forward_declaration_decoy
    "${_report_summary_forward}" FALSE "summary writer forward declaration")

set(_report_affix_payload
    "            state.monster_affix_danger[ordinal] =\n                combat::monster_affix_danger_score(monster.affixes);")
stage11d_replace_required(_report_without_affix_payload "${_report}"
    "${_report_affix_payload}" "            static_cast<void>(ordinal);"
    report_semantic_payload_removal)
set(_report_outer_close "}  // namespace arpg::platform::host_validation")
set(_report_cross_function
    "void task5b_report_payload_decoy() {\n    ${_report_affix_payload}\n}\n\n${_report_outer_close}")
stage11d_replace_required(_report_payload_cross_function
    "${_report_without_affix_payload}" "${_report_outer_close}"
    "${_report_cross_function}" report_semantic_payload_cross_function_decoy)
stage11d_run_report_guard_case(report_semantic_payload_cross_function_decoy
    "${_report_payload_cross_function}" FALSE
    "target evaluator is missing.*monster_affix_danger")
stage11d_run_summary_binding_mutations()

if(DEFINED TASK5B_REPORT_CASES_ONLY AND TASK5B_REPORT_CASES_ONLY)
    message(STATUS
        "[stage11d-renderer-task5b-report] named_mutations=11")
    return()
endif()

stage11d_replace_required(_host_outer_lambda_begin "${_host}"
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN presented_semantics"
    "const auto task5b_outer_semantic_decoy = [&]() noexcept {\n// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN presented_semantics"
    host_presented_outer_uncalled_lambda_begin)
stage11d_replace_required(_host_outer_lambda "${_host_outer_lambda_begin}"
    "// STAGE11D_LOOT_VALIDATION_SEAM_END presented_semantics"
    "// STAGE11D_LOOT_VALIDATION_SEAM_END presented_semantics\n            };"
    host_presented_outer_uncalled_lambda_end)
stage11d_run_host_guard_case(host_presented_outer_uncalled_lambda
    "${_host_outer_lambda}" FALSE "production run-chain token scope")

stage11d_replace_required(_host_without_semantic_call "${_host}"
    "${_stage11d_semantic_call}" "" host_semantic_after_capture_remove)
stage11d_replace_required(_host_semantic_after_capture
    "${_host_without_semantic_call}"
    "stage11d_validation_state.captured = true;"
    "stage11d_validation_state.captured = true;\n                ${_stage11d_semantic_call}"
    host_semantic_after_capture_insert)
stage11d_run_host_guard_case(host_semantic_after_capture_order_decoy
    "${_host_semantic_after_capture}" FALSE "production run-chain order")

stage11d_run_guard_case(pristine "${_combat}" "${_room}" "${_hud}" TRUE "")

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
    "${_hud}" FALSE "room.*builder.*exactly once|icons-only builder")

set(_canonical_predicate
    "        if (!ground_loot_visible(item, mode)) continue;")
stage11d_replace_required(_room_predicate_bypass "${_room}"
    "${_canonical_predicate}" "        if (false) continue;"
    canonical_predicate_bypass)
stage11d_run_guard_case(canonical_predicate_bypass "${_combat}"
    "${_room_predicate_bypass}" "${_hud}" FALSE
    "canonical predicate.*exactly once")

stage11d_replace_required(_room_predicate_ignored "${_room}"
    "${_canonical_predicate}"
    "        static_cast<void>(ground_loot_visible(item, mode));"
    canonical_predicate_result_ignored)
stage11d_run_guard_case(canonical_predicate_result_ignored "${_combat}"
    "${_room_predicate_ignored}" "${_hud}" FALSE
    "canonical predicate.*exactly once")

set(_production_room_draw
    "    draw_ground_items(current, loot_filter_mode_, material_pack_, width, height);")
set(_production_mode_bypass
    "    draw_ground_items(current, settings::LootFilterMode::show_all, material_pack_, width, height);")
stage11d_replace_required(_room_mode_bypass "${_room}"
    "${_production_room_draw}" "${_production_mode_bypass}"
    production_mode_bypass)
stage11d_run_guard_case(production_mode_bypass "${_combat}"
    "${_room_mode_bypass}" "${_hud}" FALSE "production.*filter mode")

set(_icons_only_builder
    "    const GroundLootView ground_loot = build_ground_loot_view(\n        snapshot, loot_filter_mode_, width, height);")
stage11d_replace_required(_room_without_icons_builder "${_room}"
    "${_icons_only_builder}" "    const GroundLootView ground_loot{};"
    icons_only_builder_removal)
set(_builder_moved_to_room
    "    static_cast<void>(build_ground_loot_view(\n        current, loot_filter_mode_, width, height));\n${_production_room_draw}")
stage11d_replace_required(_room_builder_scope "${_room_without_icons_builder}"
    "${_production_room_draw}" "${_builder_moved_to_room}"
    builder_scope_move)
stage11d_run_guard_case(builder_scope_move "${_combat}"
    "${_room_builder_scope}" "${_hud}" FALSE "icons-only builder.*scope")

set(_duplicate_predicate
    "${_canonical_predicate}\n        static_cast<void>(ground_loot_visible(item, mode));")
stage11d_replace_required(_room_duplicate_predicate "${_room}"
    "${_canonical_predicate}" "${_duplicate_predicate}"
    duplicate_canonical_predicate)
stage11d_run_guard_case(duplicate_canonical_predicate "${_combat}"
    "${_room_duplicate_predicate}" "${_hud}" FALSE
    "canonical predicate.*exactly once")

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
    "[stage11d-renderer-guard-self-test] bad_mutations=23 equivalent_variants=3")
