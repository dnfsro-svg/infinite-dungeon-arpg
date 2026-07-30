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
file(READ "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp"
    _runtime)
file(READ "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp"
    _report)
string(REPLACE "\r\n" "\n" _host "${_host}")
string(REPLACE "\r\n" "\n" _runtime "${_runtime}")
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

function(stage11d_run_runtime_guard_case LABEL RUNTIME EXPECT_PASS EXPECT_REASON)
    set(_path "${GUARD_TEST_ROOT}/${LABEL}-runtime.cpp")
    file(WRITE "${_path}" "${RUNTIME}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHOST_VALIDATION_RUNTIME_OVERRIDE=${_path}" -P "${_guard}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set(_output "${_stdout}${_stderr}")
    if(EXPECT_PASS)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard rejected equivalent ${LABEL} runtime variant: ${_output}")
        endif()
    else()
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D guard accepted bad ${LABEL} runtime mutation")
        endif()
        if(NOT _output MATCHES "${EXPECT_REASON}")
            message(FATAL_ERROR
                "Stage11D ${LABEL} runtime mutation failed for wrong reason; expected ${EXPECT_REASON}: ${_output}")
        endif()
    endif()
endfunction()

set(_stage11d_semantic_call
    "validation_runtime->observe_ground_loot(\n                current, pause_menu, runtime.render_status(),\n                presented_loot_filter, ground_loot_view,\n                renderer.hud_notice_view(), runtime.item_state(),\n                GetScreenWidth(), GetScreenHeight());")
string(REPLACE "validation_runtime->observe_ground_loot("
    "validation_runtime.get()->observe_ground_loot("
    _stage11d_alternate_semantic_call "${_stage11d_semantic_call}")
function(stage11d_expect_dead_ground_decoy LABEL WRONG_ORDER_ANCHOR
        MOVE_AFTER REASON)
    set(_dead_decoy
        "if (false)\n                ${_stage11d_semantic_call}")
    stage11d_replace_required(_with_dead_decoy "${_host}"
        "${_stage11d_semantic_call}" "${_dead_decoy}"
        "${LABEL}_dead_decoy")
    if(MOVE_AFTER)
        set(_wrong_order
            "${WRONG_ORDER_ANCHOR}\n            ${_stage11d_alternate_semantic_call}")
    else()
        set(_wrong_order
            "${_stage11d_alternate_semantic_call}\n            ${WRONG_ORDER_ANCHOR}")
    endif()
    stage11d_replace_required(_mutated "${_with_dead_decoy}"
        "${WRONG_ORDER_ANCHOR}" "${_wrong_order}"
        "${LABEL}_wrong_order")
    stage11d_run_host_guard_case("${LABEL}" "${_mutated}" FALSE "${REASON}")
endfunction()

set(_stage17_draw_observer_call
    "validation_runtime->observe_active_skill_draw(\n                presented_snapshot, renderer.active_skill_draw_status());")
function(stage11d_expect_stage17_lambda_decoy LABEL)
    set(_lambda_decoy
        "const auto task7c_stage17_draw_decoy = [&]() noexcept {\n                ${_stage17_draw_observer_call}\n            };")
    stage11d_replace_required(_with_lambda_decoy "${_host}"
        "${_stage17_draw_observer_call}" "${_lambda_decoy}"
        "${LABEL}_lambda_decoy")
    stage11d_replace_required(_mutated "${_with_lambda_decoy}"
        "${_stage11d_semantic_call}"
        "${_stage11d_semantic_call}\n            ${_stage17_draw_observer_call}"
        "${LABEL}_wrong_order")
    stage11d_run_host_guard_case("${LABEL}" "${_mutated}" FALSE "T7C-M01")
endfunction()

set(_stage12_status_anchor
    "if (config.stage12_material_runtime_status != nullptr) {\n                const MonsterMaterialDrawRuntimeStatus shooter_draw =")
string(FIND "${_host}" "${_stage12_status_anchor}" _stage12_status_begin)
string(FIND "${_host}" "${_stage11d_semantic_call}" _stage12_status_end)
if(_stage12_status_begin EQUAL -1 OR _stage12_status_end EQUAL -1
        OR NOT _stage12_status_begin LESS _stage12_status_end)
    message(FATAL_ERROR
        "Stage11D M02 mutation cannot locate the Stage12 status block")
endif()
math(EXPR _stage12_status_length
    "${_stage12_status_end} - ${_stage12_status_begin}")
string(SUBSTRING "${_host}" ${_stage12_status_begin}
    ${_stage12_status_length} _stage12_status_block)
function(stage11d_expect_stage12_lambda_decoy LABEL)
    set(_lambda_decoy
        "const auto task7c_stage12_status_decoy = [&]() noexcept {\n                ${_stage12_status_block}\n            };")
    stage11d_replace_required(_with_lambda_decoy "${_host}"
        "${_stage12_status_block}" "${_lambda_decoy}"
        "${LABEL}_lambda_decoy")
    stage11d_replace_required(_mutated "${_with_lambda_decoy}"
        "${_stage11d_semantic_call}"
        "${_stage11d_semantic_call}\n            ${_stage12_status_block}"
        "${LABEL}_wrong_order")
    stage11d_run_host_guard_case("${LABEL}" "${_mutated}" FALSE "T7C-M02")
endfunction()

set(_passive_overlay_block
    "if (!config.stage12_material_background_only\n                && !config.stage12_material_icons_only\n                && passive_overlay_open) {\n                draw_passive_tree_overlay(current, runtime.render_status());\n            }")
function(stage11d_run_task7c_scope_mutations)
    stage11d_expect_dead_ground_decoy(task7c_m01_dead_decoy_before_stage17
        "validation_runtime->observe_active_skill_draw(" FALSE "T7C-M01")
    stage11d_expect_dead_ground_decoy(task7c_m02_dead_decoy_before_stage12
        "${_stage12_status_anchor}" FALSE "T7C-M02")
    stage11d_expect_dead_ground_decoy(task7c_m03_dead_decoy_after_passive
        "${_passive_overlay_block}" TRUE "T7C-M03-passive")
    stage11d_expect_stage17_lambda_decoy(task7c_m01_lambda_decoy)
    stage11d_expect_stage12_lambda_decoy(task7c_m02_lambda_decoy)
    stage11d_replace_required(_alternate_owner "${_host}"
        "${_stage11d_semantic_call}" "${_stage11d_alternate_semantic_call}"
        task7c_equivalent_alternate_owner)
    stage11d_run_host_guard_case(task7c_equivalent_alternate_owner
        "${_alternate_owner}" TRUE "")
endfunction()
if(DEFINED TASK7C_M01_SCOPE_RED_ONLY AND TASK7C_M01_SCOPE_RED_ONLY)
    stage11d_expect_dead_ground_decoy(task7c_m01_dead_decoy_before_stage17
        "validation_runtime->observe_active_skill_draw(" FALSE "T7C-M01")
    message(STATUS "[stage11d-renderer-task7c-m01-scope-red] named_mutations=1")
    return()
endif()
if(DEFINED TASK7C_ORDER_SCOPE_ONLY AND TASK7C_ORDER_SCOPE_ONLY)
    stage11d_run_task7c_scope_mutations()
    message(STATUS
        "[stage11d-renderer-task7c-order-scope] named_mutations=5 equivalent_variants=1")
    return()
endif()
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
        "validation_runtime->observe_capture_result("
        "${_stage11d_semantic_call}\n            validation_runtime->observe_capture_result("
        host_semantic_after_capture_insert)
    stage11d_run_host_guard_case(host_semantic_after_capture_order_decoy
        "${_host_semantic_after_capture}" FALSE "ground-loot observer after")
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
    "${_host_semantic_lambda}" FALSE "requires one ground-loot facade observer")

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

stage11d_replace_required(_host_without_semantic_call "${_host}"
    "${_stage11d_semantic_call}" "" host_semantic_after_capture_remove)
stage11d_replace_required(_host_semantic_after_capture
    "${_host_without_semantic_call}"
    "validation_runtime->observe_capture_result("
    "${_stage11d_semantic_call}\n            validation_runtime->observe_capture_result("
    host_semantic_after_capture_insert)
stage11d_run_host_guard_case(host_semantic_after_capture_order_decoy
    "${_host_semantic_after_capture}" FALSE "T7C-M03-pause")

function(stage11d_expect_moved_ground_observer LABEL ANCHOR MOVE_AFTER REASON)
    stage11d_replace_required(_without_observer "${_host}"
        "${_stage11d_semantic_call}" "" "${LABEL}_remove")
    if(MOVE_AFTER)
        set(_replacement "${ANCHOR}\n${_stage11d_semantic_call}")
    else()
        set(_replacement "${_stage11d_semantic_call}\n            ${ANCHOR}")
    endif()
    stage11d_replace_required(_mutated "${_without_observer}"
        "${ANCHOR}" "${_replacement}" "${LABEL}_insert")
    stage11d_run_host_guard_case("${LABEL}" "${_mutated}" FALSE "${REASON}")
endfunction()

function(stage11d_expect_ground_observer_after_block LABEL BLOCK REASON)
    stage11d_replace_required(_without_observer "${_host}"
        "${_stage11d_semantic_call}" "" "${LABEL}_remove")
    stage11d_replace_required(_mutated "${_without_observer}"
        "${BLOCK}" "${BLOCK}\n            ${_stage11d_semantic_call}"
        "${LABEL}_insert")
    stage11d_run_host_guard_case("${LABEL}" "${_mutated}" FALSE "${REASON}")
endfunction()

stage11d_run_task7c_scope_mutations()

stage11d_expect_moved_ground_observer(task7c_m01_before_stage17
    "validation_runtime->observe_active_skill_draw(" FALSE
    "T7C-M01")
stage11d_expect_moved_ground_observer(task7c_m02_before_stage12_status
    "if (config.stage12_material_runtime_status != nullptr) {\n                const MonsterMaterialDrawRuntimeStatus shooter_draw =" FALSE
    "T7C-M02")
set(_inventory_overlay_block
    "if (!config.stage12_material_background_only\n                && !config.stage12_material_icons_only\n                && inventory.is_open()) {\n                inventory.draw(*session, current, runtime.render_status(),\n                    renderer.material_pack(), renderer.hud_font(),\n                    renderer.hud_font_ready());\n            }")
set(_pause_overlay_block
    "if (!config.stage12_material_background_only\n                && !config.stage12_material_icons_only\n                && pause_menu.screen != PauseScreen::closed) {\n                pause_menu_renderer.draw(pause_menu, renderer.material_pack());\n                pause_cjk_ready = pause_menu_renderer.has_cjk_font();\n            }")
stage11d_expect_ground_observer_after_block(task7c_m03_after_passive_overlay
    "${_passive_overlay_block}"
    "T7C-M03-passive")
stage11d_expect_ground_observer_after_block(task7c_m03_after_inventory_overlay
    "${_inventory_overlay_block}"
    "T7C-M03-inventory")
stage11d_expect_ground_observer_after_block(task7c_m03_after_pause_overlay
    "${_pause_overlay_block}"
    "T7C-M03-pause")

stage11d_replace_required(_m04_second_view "${_host}"
    "const GroundLootView ground_loot_view = [&]() noexcept {"
    "const auto second_ground_loot_view = build_ground_loot_view(\n                presented_snapshot, presented_loot_filter, 1.0F, 1.0F);\n            const GroundLootView ground_loot_view = [&]() noexcept {"
    task7c_m04_rebuild_second_view)
stage11d_run_host_guard_case(task7c_m04_rebuild_second_view
    "${_m04_second_view}" FALSE "T7C-M04-builder")
stage11d_replace_required(_m04_declared_second "${_host}"
    "            }();\n            validation_runtime->observe_active_skill_draw("
    "            }();\n            const auto second_ground_loot_view = ground_loot_view;\n            validation_runtime->observe_active_skill_draw("
    task7c_m04_declare_second_view)
stage11d_replace_required(_m04_pass_second "${_m04_declared_second}"
    "ground_loot_view,\n                renderer.hud_notice_view()"
    "second_ground_loot_view,\n                renderer.hud_notice_view()"
    task7c_m04_pass_second_view)
stage11d_run_host_guard_case(task7c_m04_pass_second_view
    "${_m04_pass_second}" FALSE "T7C-M04")

foreach(_notice_variant IN ITEMS cached_hud_notices previous_hud_notices)
    stage11d_replace_required(_mutated "${_host}"
        "renderer.hud_notice_view(), runtime.item_state(),"
        "${_notice_variant}, runtime.item_state(),"
        "task7c_m05_${_notice_variant}")
    stage11d_run_host_guard_case("task7c_m05_${_notice_variant}"
        "${_mutated}" FALSE "T7C-M05")
endforeach()
set(_m06_dimension_tail
    "renderer.hud_notice_view(), runtime.item_state(),\n                GetScreenWidth(), GetScreenHeight());")
stage11d_replace_required(_m06_constants "${_host}"
    "${_m06_dimension_tail}"
    "renderer.hud_notice_view(), runtime.item_state(),\n                1280, 720);"
    task7c_m06_constant_dimensions)
stage11d_run_host_guard_case(task7c_m06_constant_dimensions
    "${_m06_constants}" FALSE "T7C-M06")
stage11d_replace_required(_m06_swapped "${_host}"
    "${_m06_dimension_tail}"
    "renderer.hud_notice_view(), runtime.item_state(),\n                GetScreenHeight(), GetScreenWidth());"
    task7c_m06_swapped_dimensions)
stage11d_run_host_guard_case(task7c_m06_swapped_dimensions
    "${_m06_swapped}" FALSE "T7C-M06")
stage11d_replace_required(_m06_globals "${_report}"
    "make_hud_layout(\n            screen_width, screen_height, false)"
    "make_hud_layout(\n            GetScreenWidth(), GetScreenHeight(), false)"
    task7c_m06_global_dimensions)
stage11d_run_report_guard_case(task7c_m06_global_dimensions
    "${_m06_globals}" FALSE "T7C-M06-global")

stage11d_replace_required(_ground_without_visible_gate "${_runtime}"
    "if (impl_->states.stage11d.target_visible\n            && !impl_->states.stage11d.captured) {"
    "if (!impl_->states.stage11d.captured) {"
    task7c_ground_record_without_visible_gate)
stage11d_run_runtime_guard_case(task7c_ground_record_without_visible_gate
    "${_ground_without_visible_gate}" FALSE
    "T7C-ground-condition")
stage11d_replace_required(_ground_without_uncaptured_gate "${_runtime}"
    "if (impl_->states.stage11d.target_visible\n            && !impl_->states.stage11d.captured) {"
    "if (impl_->states.stage11d.target_visible) {"
    task7c_ground_record_without_uncaptured_gate)
stage11d_run_runtime_guard_case(task7c_ground_record_without_uncaptured_gate
    "${_ground_without_uncaptured_gate}" FALSE
    "T7C-ground-condition")
set(_ground_record_block
    "if (impl_->states.stage11d.target_visible\n            && !impl_->states.stage11d.captured) {\n        host_validation::stage11d_record_semantics(\n            impl_->states.stage11d, snapshot, ownership,\n            ground_loot_view, notices);\n    }")
foreach(_ground_scope IN ITEMS lambda dead)
    if(_ground_scope STREQUAL "lambda")
        set(_ground_scope_replacement
            "const auto ground_record_decoy = [&] { ${_ground_record_block} };")
    else()
        set(_ground_scope_replacement
            "if (false) { ${_ground_record_block} }")
    endif()
    stage11d_replace_required(_ground_scoped_runtime "${_runtime}"
        "${_ground_record_block}" "${_ground_scope_replacement}"
        "task7c_ground_${_ground_scope}_scope")
    stage11d_run_runtime_guard_case("task7c_ground_${_ground_scope}_scope"
        "${_ground_scoped_runtime}" FALSE
        "T7C-ground-condition")
endforeach()

set(_inactive_host_decoy
    "#if 0\nvalidation_runtime->observe_ground_loot(current, pause_menu, runtime.render_status(), presented_loot_filter, ground_loot_view, renderer.hud_notice_view(), runtime.item_state(), GetScreenWidth(), GetScreenHeight());\n#endif\n            const GroundLootView ground_loot_view = [&]() noexcept {")
stage11d_replace_required(_host_with_inactive_decoy "${_host}"
    "const GroundLootView ground_loot_view = [&]() noexcept {"
    "${_inactive_host_decoy}" inactive_host_facade_decoy)
stage11d_run_host_guard_case(inactive_host_facade_decoy
    "${_host_with_inactive_decoy}" TRUE "")
set(_inactive_runtime_decoy
    "#if 0\nvoid HostValidationRuntime::observe_ground_loot() {}\n#endif\nvoid HostValidationRuntime::observe_ground_loot(")
stage11d_replace_required(_runtime_with_inactive_decoy "${_runtime}"
    "void HostValidationRuntime::observe_ground_loot("
    "${_inactive_runtime_decoy}" inactive_runtime_facade_decoy)
stage11d_run_runtime_guard_case(inactive_runtime_facade_decoy
    "${_runtime_with_inactive_decoy}" TRUE "")

stage11d_replace_required(_m19_no_callback "${_host}"
    "validation_runtime->observe_capture_result("
    "validation_runtime->observe_capture_result_removed("
    task7c_m19_omit_capture_callback)
stage11d_run_host_guard_case(task7c_m19_omit_capture_callback
    "${_m19_no_callback}" FALSE "T7C-M19")

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
    "[stage11d-renderer-guard-self-test] task7c_mutations=22 equivalent_variants=6")
