if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

set(_combat_header_path "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.hpp")
set(_combat_source_path "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_room_source_path "${SOURCE_ROOT}/src/platform/raylib/room_renderer.cpp")
set(_hud_source_path "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp")
set(_host_source_path "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_report_source_path
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
if(DEFINED COMBAT_HEADER_OVERRIDE)
    set(_combat_header_path "${COMBAT_HEADER_OVERRIDE}")
endif()
if(DEFINED COMBAT_SOURCE_OVERRIDE)
    set(_combat_source_path "${COMBAT_SOURCE_OVERRIDE}")
endif()
if(DEFINED ROOM_SOURCE_OVERRIDE)
    set(_room_source_path "${ROOM_SOURCE_OVERRIDE}")
endif()
if(DEFINED HUD_SOURCE_OVERRIDE)
    set(_hud_source_path "${HUD_SOURCE_OVERRIDE}")
endif()
if(DEFINED HOST_OVERRIDE)
    set(_host_source_path "${HOST_OVERRIDE}")
endif()
if(DEFINED REPORT_OVERRIDE)
    set(_report_source_path "${REPORT_OVERRIDE}")
endif()

foreach(_required_path IN ITEMS
        "${_combat_header_path}"
        "${_combat_source_path}"
        "${_room_source_path}"
        "${_hud_source_path}"
        "${_host_source_path}"
        "${_report_source_path}")
    if(NOT EXISTS "${_required_path}")
        message(FATAL_ERROR "Stage11D renderer integration guard input missing: ${_required_path}")
    endif()
endforeach()

file(READ "${_combat_header_path}" _combat_header)
file(READ "${_combat_source_path}" _combat_source)
file(READ "${_room_source_path}" _room_source)
file(READ "${_hud_source_path}" _hud_source)
file(READ "${_host_source_path}" _host_source)
file(READ "${_report_source_path}" _report_source)

function(stage11d_count_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${SOURCE}" _source_length)
    string(LENGTH "${TOKEN}" _token_length)
    string(REPLACE "${TOKEN}" "" _without "${SOURCE}")
    string(LENGTH "${_without}" _without_length)
    math(EXPR _removed "${_source_length} - ${_without_length}")
    math(EXPR _count "${_removed} / ${_token_length}")
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(stage11d_brace_depth SURFACE POSITION OUT_DEPTH)
    if(POSITION EQUAL 0)
        set(${OUT_DEPTH} 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    string(REGEX REPLACE "[^{}]" "" _braces "${_prefix}")
    string(LENGTH "${_braces}" _brace_length)
    set(_depth 0)
    if(_brace_length GREATER 0)
        math(EXPR _last "${_brace_length} - 1")
        foreach(_index RANGE 0 ${_last})
            string(SUBSTRING "${_braces}" ${_index} 1 _brace)
            if(_brace STREQUAL "{")
                math(EXPR _depth "${_depth} + 1")
            else()
                math(EXPR _depth "${_depth} - 1")
            endif()
        endforeach()
    endif()
    set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
endfunction()

function(stage11d_require_definition LABEL SURFACE SIGNATURE EXPECTED_DEPTH
        OUT_FUNCTION)
    stage11d_count_token("${SURFACE}" "${SIGNATURE}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D report requires one ${LABEL} definition; found ${_count}")
    endif()
    string(FIND "${SURFACE}" "${SIGNATURE}" _position)
    stage11d_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Stage11D report rejected ${LABEL} definition scope")
    endif()
    string(SUBSTRING "${SURFACE}" ${_position} -1 _tail)
    string(FIND "${_tail}" "{" _brace)
    string(FIND "${_tail}" ";" _semicolon)
    if(_brace EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _brace))
        message(FATAL_ERROR
            "Stage11D report rejected ${LABEL} forward declaration")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${SURFACE}" "${SIGNATURE}"
        _begin _open _end)
    math(EXPR _length "${_end} - ${_begin} + 1")
    string(SUBSTRING "${SURFACE}" ${_begin} ${_length} _function)
    set(${OUT_FUNCTION} "${_function}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED TASK5B_RENDERER_ONLY OR NOT TASK5B_RENDERER_ONLY)
set(_report_marked "${_report_source}")
foreach(_edge IN ITEMS BEGIN END)
    set(_marker
        "// STAGE11D_LOOT_VALIDATION_SEAM_${_edge} evidence_semantics")
    string(REPLACE "${_marker}" "TASK5B_REPORT_${_edge}"
        _report_marked "${_report_marked}")
endforeach()
evidence_sanitize_cpp_for_scan("${_report_marked}" _report_code)
foreach(_edge IN ITEMS BEGIN END)
    stage11d_count_token("${_report_code}" "TASK5B_REPORT_${_edge}"
        _marker_count)
    if(NOT _marker_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D report rejected evidence_semantics marker comment/string decoy")
    endif()
endforeach()
stage11d_require_definition("rarity-view helper" "${_report_code}"
    "bool stage11d_view_has_rarity(" 2 _report_view_function)
stage11d_require_definition("semantic recorder" "${_report_code}"
    "void stage11d_record_semantics(" 1 _report_record_function)
stage11d_require_definition("target-visible evaluator" "${_report_code}"
    "bool stage11d_target_visible(" 1 _report_target_function)
stage11d_require_definition("scenario-name helper" "${_report_code}"
    "const char* stage11d_scenario_name(" 2 _report_name_function)
stage11d_require_definition("summary writer" "${_report_code}"
    "void write_stage11d_loot_validation_summary(" 1 _report_summary_function)

foreach(_required IN ITEMS
        "state.view = view;" "state.notices = notices;"
        "state.snapshot_item_count = snapshot.ground_item_count;"
        "state.snapshot_item_ids[index] = item.item_id;"
        "state.inventory_item_ids[state.inventory_item_count++] = item.id;")
    string(FIND "${_report_record_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D report semantic recorder is missing: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS
        "state.pickup_commit_generation = status.loot_pickup.commit_generation;"
        "state.monster_affix_danger[ordinal] ="
        "state.monster_ai_phase[ordinal] ="
        "state.defeat_player_hp[ordinal] = snapshot.combat->player.hp;"
        "state.min_remaining_targets"
        "stage11d_has_three_ordinary_rarities(snapshot)"
        "stage11d_view_has_rarity(")
    string(FIND "${_report_target_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D report target evaluator is missing: ${_required}")
    endif()
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _report_target_normalized
    "${_report_target_function}")
foreach(_producer_binding IN ITEMS
        "state.monster_affix_danger[ordinal]=combat::monster_affix_danger_score(monster.affixes);"
        "state.monster_ai_phase[ordinal]=static_cast<std::uint8_t>(monster.ai_phase);")
    string(FIND "${_report_target_normalized}" "${_producer_binding}"
        _producer_binding_found)
    if(_producer_binding_found EQUAL -1)
        if(_producer_binding MATCHES "monster_affix_danger")
            set(_producer_label "monster_affix_danger")
        else()
            set(_producer_label "monster_ai_phase")
        endif()
        message(FATAL_ERROR
            "Stage11D report rejected ${_producer_label} producer binding")
    endif()
endforeach()
evidence_find_cpp_function_bounds_in_sanitized("${_report_code}"
    "void write_stage11d_loot_validation_summary("
    _report_summary_begin _report_summary_open _report_summary_end)
math(EXPR _report_summary_length
    "${_report_summary_end} - ${_report_summary_begin} + 1")
string(SUBSTRING "${_report_marked}" ${_report_summary_begin}
    ${_report_summary_length} _report_summary_raw)
set(_summary_fields
    scenario committed_mode draft_mode snapshot_count visible_count
    inventory_count normal_item_id magic_item_id rare_item_id abyss_item_id
    abyss_rarity preview_visible_count restored_visible_count abyss_claimed
    progress_phase progress_remaining progress_inventory progress_hp
    progress_rarities max_ground_rarities max_ground_count min_remaining
    max_inventory observed_normal observed_magic observed_rare
    monster_seen_bits monster_defeated_bits monster_last_hp monster_min_hp
    monster_affix_danger monster_ai_phase defeat_distance_milli
    defeat_player_hp target_ordinal pickup_item_id pickup_commit_generation
    pickup_notice_rect notice_text snapshot_ids inventory_ids label_rects result)
set(_summary_fields_marked "${_report_summary_raw}")
foreach(_field IN LISTS _summary_fields)
    set(_literal "\"${_field}=\"")
    string(REPLACE "${_literal}" "TASK5B_SUMMARY_FIELD_${_field}"
        _summary_fields_marked "${_summary_fields_marked}")
endforeach()
string(REPLACE "','" "TASK5B_SUMMARY_COMMA"
    _summary_fields_marked "${_summary_fields_marked}")
string(REPLACE "'\\n'" "TASK5B_SUMMARY_NEWLINE"
    _summary_fields_marked "${_summary_fields_marked}")
string(REPLACE "\"pass\"" "TASK5B_RESULT_PASS"
    _summary_fields_marked "${_summary_fields_marked}")
string(REPLACE "\"fail\"" "TASK5B_RESULT_FAIL"
    _summary_fields_marked "${_summary_fields_marked}")
evidence_sanitize_cpp_for_scan("${_summary_fields_marked}"
    _summary_fields_code)
set(_previous_field_position -1)
foreach(_field IN LISTS _summary_fields)
    set(_marker "TASK5B_SUMMARY_FIELD_${_field}")
    stage11d_count_token("${_summary_fields_code}" "${_marker}" _count)
    string(FIND "${_summary_fields_code}" "${_marker}" _position)
    if(NOT _count EQUAL 1 OR _position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D report summary is missing or duplicates field: ${_field}")
    endif()
    if(NOT _previous_field_position EQUAL -1
            AND NOT _position GREATER _previous_field_position)
        message(FATAL_ERROR
            "Stage11D report summary field order changed at: ${_field}")
    endif()
    set(_previous_field_position ${_position})
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _summary_bindings_normalized
    "${_summary_fields_code}")
function(stage11d_require_summary_binding LABEL BINDING)
    string(FIND "${_summary_bindings_normalized}" "${BINDING}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D report rejected ${LABEL} output binding")
    endif()
endfunction()
stage11d_require_summary_binding(monster_affix_danger
    "TASK5B_SUMMARY_FIELD_monster_affix_danger<<state.monster_affix_danger[0]<<TASK5B_SUMMARY_COMMA<<state.monster_affix_danger[1]<<TASK5B_SUMMARY_COMMA<<state.monster_affix_danger[2]<<TASK5B_SUMMARY_NEWLINE")
stage11d_require_summary_binding(monster_ai_phase
    "TASK5B_SUMMARY_FIELD_monster_ai_phase<<static_cast<unsigned>(state.monster_ai_phase[0])<<TASK5B_SUMMARY_COMMA<<static_cast<unsigned>(state.monster_ai_phase[1])<<TASK5B_SUMMARY_COMMA<<static_cast<unsigned>(state.monster_ai_phase[2])<<TASK5B_SUMMARY_NEWLINE")
stage11d_require_summary_binding(defeat_player_hp
    "TASK5B_SUMMARY_FIELD_defeat_player_hp<<state.defeat_player_hp[0]<<TASK5B_SUMMARY_COMMA<<state.defeat_player_hp[1]<<TASK5B_SUMMARY_COMMA<<state.defeat_player_hp[2]<<TASK5B_SUMMARY_NEWLINE")
stage11d_require_summary_binding(target_ordinal
    "TASK5B_SUMMARY_FIELD_target_ordinal<<state.target_ordinal<<TASK5B_SUMMARY_NEWLINE")
stage11d_require_summary_binding(result
    "TASK5B_SUMMARY_FIELD_result<<(state.captured&&(config.stage11d_loot_validation!=Stage11DLootValidationScenario::rare_only_abyss||state.abyss_claimed)?TASK5B_RESULT_PASS:TASK5B_RESULT_FAIL)<<TASK5B_SUMMARY_NEWLINE")

if(NOT DEFINED TASK5B_REPORT_ONLY OR NOT TASK5B_REPORT_ONLY)
function(stage11d_require_token_depth LABEL SURFACE TOKEN EXPECTED_DEPTH)
    stage11d_count_token("${SURFACE}" "${TOKEN}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D host requires one ${LABEL}; found ${_count}")
    endif()
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    stage11d_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR "Stage11D host rejected ${LABEL} scope")
    endif()
    set(stage11d_last_token_position ${_position} PARENT_SCOPE)
endfunction()

function(stage11d_extract_host_seam LABEL OUT_CODE)
    set(_begin "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN ${LABEL}")
    set(_end "// STAGE11D_LOOT_VALIDATION_SEAM_END ${LABEL}")
    stage11d_count_token("${_host_source}" "${_begin}" _begin_count)
    stage11d_count_token("${_host_source}" "${_end}" _end_count)
    if(NOT _begin_count EQUAL 1 OR NOT _end_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D host cannot bind ${LABEL} seam markers")
    endif()
    string(FIND "${_host_source}" "${_begin}" _begin_at)
    string(FIND "${_host_source}" "${_end}" _end_at)
    if(NOT _begin_at LESS _end_at)
        message(FATAL_ERROR "Stage11D host rejected ${LABEL} seam order")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${_host_source}" ${_begin_at} ${_length} _raw)
    string(REPLACE "${_begin}" "TASK5B_${LABEL}_BEGIN" _marked "${_raw}")
    string(REPLACE "${_end}" "TASK5B_${LABEL}_END" _marked "${_marked}")
    evidence_sanitize_cpp_for_scan("${_marked}" _code)
    foreach(_marker IN ITEMS "TASK5B_${LABEL}_BEGIN" "TASK5B_${LABEL}_END")
        stage11d_count_token("${_code}" "${_marker}" _marker_count)
        if(NOT _marker_count EQUAL 1)
            message(FATAL_ERROR
                "Stage11D host rejected ${LABEL} marker comment/string decoy")
        endif()
    endforeach()
    set(${OUT_CODE} "${_code}" PARENT_SCOPE)
endfunction()

set(_run_signature "HostExitCode run_raylib_host(")
evidence_find_cpp_code_token("${_host_source}" "${_run_signature}"
    _run_raw_begin)
if(_run_raw_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D host cannot bind the production run_raylib_host definition")
endif()
string(SUBSTRING "${_host_source}" ${_run_raw_begin} -1 _run_raw_tail)
evidence_sanitize_cpp_for_scan("${_run_raw_tail}" _run_tail_code)
stage11d_count_token("${_run_tail_code}" "${_run_signature}"
    _run_signature_count)
if(NOT _run_signature_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D host requires one production run_raylib_host definition")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_run_tail_code}"
    "${_run_signature}" _run_begin _run_open _run_end)
math(EXPR _run_length "${_run_end} - ${_run_begin} + 1")
string(SUBSTRING "${_run_tail_code}" ${_run_begin} ${_run_length}
    _run_function_code)
evidence_find_cpp_function_bounds_in_sanitized("${_run_function_code}"
    "while (!exit_requested)" _loop_begin _loop_open _loop_end)

set(_run_loop_chain_tokens
    "const settings::LootFilterMode presented_loot_filter ="
    "renderer.set_loot_filter_mode("
    "BeginDrawing()"
    "const GroundLootView ground_loot_view = [&]() noexcept {"
    "return renderer.draw("
    "stage11d_target_visible("
    "stage11d_record_semantics("
    "const bool stage11d_reached ="
    "const bool loot_validation_visible_capture ="
    "captured_stage10_target = true"
    "const bool capture_succeeded ="
    "const bool captured_stage10_frame ="
    "stage11d_validation_state.captured = true")
set(_run_loop_chain_depths 3 3 3 3 4 3 4 3 3 4 3 3 4)
set(_run_previous_position -1)
list(LENGTH _run_loop_chain_tokens _run_loop_chain_count)
math(EXPR _run_loop_chain_last "${_run_loop_chain_count} - 1")
foreach(_index RANGE 0 ${_run_loop_chain_last})
    list(GET _run_loop_chain_tokens ${_index} _token)
    list(GET _run_loop_chain_depths ${_index} _depth)
    stage11d_require_token_depth("production run-chain token"
        "${_run_function_code}" "${_token}" ${_depth})
    set(_position ${stage11d_last_token_position})
    if(NOT _position GREATER _loop_open OR NOT _position LESS _loop_end)
        message(FATAL_ERROR
            "Stage11D host rejected production run-chain loop scope")
    endif()
    if(NOT _run_previous_position EQUAL -1
            AND NOT _position GREATER _run_previous_position)
        message(FATAL_ERROR
            "Stage11D host rejected production run-chain order")
    endif()
    set(_run_previous_position ${_position})
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" "" _run_function_normalized
    "${_run_function_code}")
foreach(_binding IN ITEMS
        "renderer.set_loot_filter_mode(presented_loot_filter);BeginDrawing();"
        "validation_reached||loot_validation_visible_capture"
        "capture_path=config.validation_capture_file->string();captured_stage10_target=true;"
        "constboolcaptured_stage10_frame=captured_stage10_target&&capture_succeeded;"
        "if(captured_stage10_frame&&stage11d_validation_state.target_visible){stage11d_validation_state.captured=true;}")
    string(FIND "${_run_function_normalized}" "${_binding}" _binding_found)
    if(_binding_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D host rejected production run-chain binding")
    endif()
endforeach()

set(_run_post_loop_chain_tokens
    "stage17_validation_state->clean_shutdown_exact_ready ="
    "host_validation::write_stage11b_validation_summary("
    "host_validation::write_stage11c_hud_validation_summary("
    "write_stage11d_loot_validation_summary("
    "write_stage17_validation_summary("
    "audio.shutdown()"
    "renderer.shutdown_resources()"
    "pause_menu_renderer.shutdown()"
    "return HostExitCode::success")
set(_run_previous_position ${_loop_end})
foreach(_token IN LISTS _run_post_loop_chain_tokens)
    stage11d_require_token_depth("production post-loop token"
        "${_run_function_code}" "${_token}" 2)
    set(_position ${stage11d_last_token_position})
    if(NOT _position GREATER _run_previous_position)
        message(FATAL_ERROR
            "Stage11D host rejected production post-loop summary/shutdown order")
    endif()
    set(_run_previous_position ${_position})
endforeach()
string(FIND "${_run_function_normalized}"
    "pause_menu_renderer.shutdown();CloseWindow();returnHostExitCode::success;"
    _clean_shutdown_binding)
if(_clean_shutdown_binding EQUAL -1)
    message(FATAL_ERROR
        "Stage11D host rejected production clean-shutdown binding")
endif()

math(EXPR _run_raw_end "${_run_raw_begin} + ${_run_end}")
foreach(_label IN ITEMS presented_semantics reached reached_merge
        visible_capture captured summary)
    foreach(_edge IN ITEMS BEGIN END)
        set(_marker
            "// STAGE11D_LOOT_VALIDATION_SEAM_${_edge} ${_label}")
        string(FIND "${_host_source}" "${_marker}" _marker_position)
        if(_marker_position EQUAL -1
                OR NOT _marker_position GREATER _run_raw_begin
                OR NOT _marker_position LESS _run_raw_end)
            message(FATAL_ERROR
                "Stage11D host rejected ${_label} seam outside production run scope")
        endif()
    endforeach()
endforeach()

set(_ground_view_anchor "const GroundLootView ground_loot_view = [&]() noexcept {")
string(FIND "${_host_source}" "${_ground_view_anchor}" _ground_view_raw)
string(FIND "${_host_source}"
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN presented_semantics"
    _presented_marker_raw)
if(_ground_view_raw EQUAL -1 OR _presented_marker_raw EQUAL -1
        OR NOT _ground_view_raw LESS _presented_marker_raw)
    message(FATAL_ERROR
        "Stage11D host cannot isolate the real GroundLootView presentation crop")
endif()
math(EXPR _ground_crop_length "${_presented_marker_raw} - ${_ground_view_raw}")
string(SUBSTRING "${_host_source}" ${_ground_view_raw}
    ${_ground_crop_length} _ground_crop_raw)
evidence_sanitize_cpp_for_scan("${_ground_crop_raw}" _ground_crop_code)
stage11d_require_token_depth("presented GroundLootView construction"
    "${_ground_crop_code}" "${_ground_view_anchor}" 0)
stage11d_require_token_depth("production renderer GroundLootView result"
    "${_ground_crop_code}" "return renderer.draw(" 1)

stage11d_extract_host_seam(presented_semantics _presented_semantics_code)
stage11d_require_token_depth("presented target-visible call"
    "${_presented_semantics_code}" "stage11d_target_visible(" 0)
set(_target_call_position ${stage11d_last_token_position})
stage11d_require_token_depth("presented semantic call"
    "${_presented_semantics_code}" "stage11d_record_semantics(" 1)
set(_record_call_position ${stage11d_last_token_position})
if(NOT _target_call_position LESS _record_call_position)
    message(FATAL_ERROR
        "Stage11D host rejected target-visible to semantic-record order")
endif()
stage11d_count_token("${_presented_semantics_code}"
    "renderer.hud_notice_view()" _notice_sample_count)
if(NOT _notice_sample_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D host requires two fresh HUD notice samples; found ${_notice_sample_count}")
endif()
string(REGEX REPLACE "[ \t\r\n]+" "" _presented_semantics_normalized
    "${_presented_semantics_code}")
foreach(_binding IN ITEMS
        "stage11d_validation_state.target_visible=stage11d_target_visible(config,current,pause_menu,runtime.render_status(),presented_loot_filter,ground_loot_view,renderer.hud_notice_view(),stage11d_validation_state);"
        "if(stage11d_validation_state.target_visible&&!stage11d_validation_state.captured){stage11d_record_semantics(stage11d_validation_state,current,runtime.item_state(),ground_loot_view,renderer.hud_notice_view());}")
    string(FIND "${_presented_semantics_normalized}" "${_binding}" _binding_found)
    if(_binding_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D host rejected presented semantic binding")
    endif()
endforeach()

evidence_find_cpp_code_token("${_host_source}"
    "bool present_frame_and_maybe_capture(" _present_helper_start)
if(_present_helper_start EQUAL -1)
    message(FATAL_ERROR "Stage11D host is missing presentation helper")
endif()
string(SUBSTRING "${_host_source}" ${_present_helper_start} -1
    _present_helper_tail)
string(FIND "${_present_helper_tail}" "void draw_stage12_ui_material_gallery("
    _present_helper_end)
if(_present_helper_end EQUAL -1)
    message(FATAL_ERROR "Stage11D host cannot isolate presentation helper")
endif()
string(SUBSTRING "${_present_helper_tail}" 0 ${_present_helper_end}
    _present_helper_raw)
evidence_sanitize_cpp_for_scan("${_present_helper_raw}" _present_helper_code)
set(_present_helper_chain_tokens
    "EndDrawing()"
    "if (path == nullptr) return true"
    "Image image = LoadImageFromScreen()"
    "if (image.data == nullptr) return false"
    "const bool exported = ExportImage(image, path)"
    "UnloadImage(image)"
    "return exported")
set(_present_helper_previous -1)
foreach(_token IN LISTS _present_helper_chain_tokens)
    stage11d_require_token_depth("presentation-helper lifecycle token"
        "${_present_helper_code}" "${_token}" 1)
    set(_position ${stage11d_last_token_position})
    if(NOT _present_helper_previous EQUAL -1
            AND NOT _position GREATER _present_helper_previous)
        message(FATAL_ERROR
            "Stage11D host rejected presentation-helper lifecycle order")
    endif()
    set(_present_helper_previous ${_position})
endforeach()

set(_capture_region_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN reached")
set(_capture_region_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END captured")
string(FIND "${_host_source}" "${_capture_region_begin}" _capture_begin_at)
string(FIND "${_host_source}" "${_capture_region_end}" _capture_end_at)
if(_capture_begin_at EQUAL -1 OR _capture_end_at EQUAL -1
        OR NOT _capture_begin_at LESS _capture_end_at)
    message(FATAL_ERROR "Stage11D host cannot isolate capture-state chain")
endif()
string(LENGTH "${_capture_region_end}" _capture_end_length)
math(EXPR _capture_region_length
    "${_capture_end_at} - ${_capture_begin_at} + ${_capture_end_length}")
string(SUBSTRING "${_host_source}" ${_capture_begin_at}
    ${_capture_region_length} _capture_region_raw)
string(REPLACE "${_capture_region_begin}" "TASK5B_CAPTURE_BEGIN"
    _capture_region_marked "${_capture_region_raw}")
string(REPLACE "${_capture_region_end}" "TASK5B_CAPTURE_END"
    _capture_region_marked "${_capture_region_marked}")
evidence_sanitize_cpp_for_scan("${_capture_region_marked}" _capture_region_code)
set(_capture_chain_tokens
    "const bool stage11d_reached ="
    "const bool loot_validation_visible_capture ="
    "present_frame_and_maybe_capture("
    "const bool captured_stage10_frame ="
    "stage11d_validation_state.captured = true")
set(_previous_position -1)
foreach(_token IN LISTS _capture_chain_tokens)
    if(_token STREQUAL "stage11d_validation_state.captured = true")
        set(_expected_depth 1)
    else()
        set(_expected_depth 0)
    endif()
    stage11d_require_token_depth("capture-chain token"
        "${_capture_region_code}" "${_token}" ${_expected_depth})
    set(_position ${stage11d_last_token_position})
    if(NOT _previous_position EQUAL -1 AND _position LESS _previous_position)
        message(FATAL_ERROR "Stage11D host rejected capture-state order")
    endif()
    set(_previous_position ${_position})
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _capture_region_normalized
    "${_capture_region_code}")
foreach(_binding IN ITEMS
        "constboolstage11d_reached=stage11d_validation_state.captured&&(config.stage11d_loot_validation!=Stage11DLootValidationScenario::rare_only_abyss||stage11d_validation_state.abyss_claimed);"
        "constboolloot_validation_visible_capture=stage11d_validation_state.target_visible;"
        "constboolcaptured_stage10_frame=captured_stage10_target&&capture_succeeded;"
        "if(captured_stage10_frame&&stage11d_validation_state.target_visible){stage11d_validation_state.captured=true;}")
    string(FIND "${_capture_region_normalized}" "${_binding}" _binding_found)
    if(_binding_found EQUAL -1)
        message(FATAL_ERROR "Stage11D host rejected capture-state binding")
    endif()
endforeach()

stage11d_extract_host_seam(summary _summary_seam_code)
stage11d_require_token_depth("summary call" "${_summary_seam_code}"
    "write_stage11d_loot_validation_summary(" 0)
string(REGEX REPLACE "[ \t\r\n]+" "" _summary_normalized
    "${_summary_seam_code}")
string(FIND "${_summary_normalized}"
    "write_stage11d_loot_validation_summary(config,stage11d_validation_state,pause_menu);"
    _summary_binding)
if(_summary_binding EQUAL -1)
    message(FATAL_ERROR "Stage11D host rejected summary call binding")
endif()
endif()
endif()

set(_legacy_tokens
    "GroundLootRenderConsumers"
    "make_ground_loot_render_consumers"
    "\.hud_view"
    "\.room_view")
foreach(_legacy_token IN LISTS _legacy_tokens)
    if(_combat_header MATCHES "${_legacy_token}" OR _combat_source MATCHES "${_legacy_token}")
        message(FATAL_ERROR "Stage11D renderer integration must not restore legacy wrapper token: ${_legacy_token}")
    endif()
endforeach()

function(stage11d_reject_consumer_rebuild SOURCE_TEXT CONSUMER_NAME)
    set(_rebuild_tokens
        "build_ground_loot_view[ \t\r\n]*\\("
        "ground_loot_visible[ \t\r\n]*\\("
        "make_combat_render_plan[ \t\r\n]*\\("
        "LootFilterMode"
        "loot_filter_mode")
    foreach(_token IN LISTS _rebuild_tokens)
        if("${SOURCE_TEXT}" MATCHES "${_token}")
            message(FATAL_ERROR
                "Stage11D ${CONSUMER_NAME} renderer must not rebuild or independently filter GroundLootView")
        endif()
    endforeach()
endfunction()

stage11d_reject_consumer_rebuild("${_hud_source}" "HUD")

# The room renderer intentionally keeps a label-free icon path for frame-start
# capture. Permit only that fixed shape: one canonical visibility predicate in
# draw_ground_items, one builder in draw_ground_loot_icons_only, and the same
# renderer-owned filter mode at both room call sites.
string(FIND "${_room_source}" "void draw_ground_items(" _room_items_start)
string(FIND "${_room_source}" "void draw_secondary_loot_icon(" _room_items_end)
string(FIND "${_room_source}"
    "GroundLootView CombatRenderer::draw_ground_loot_icons_only("
    _icons_only_start)
string(FIND "${_room_source}" "void CombatRenderer::draw_room(" _draw_room_start)
if(_room_items_start EQUAL -1 OR _room_items_end EQUAL -1 OR
   _icons_only_start EQUAL -1 OR _draw_room_start EQUAL -1 OR
   NOT _room_items_start LESS _room_items_end OR
   NOT _icons_only_start LESS _draw_room_start)
    message(FATAL_ERROR
        "Stage11D room renderer structure is missing or reordered")
endif()

math(EXPR _room_items_length "${_room_items_end} - ${_room_items_start}")
string(SUBSTRING "${_room_source}" ${_room_items_start}
    ${_room_items_length} _room_items_source)
math(EXPR _icons_only_length "${_draw_room_start} - ${_icons_only_start}")
string(SUBSTRING "${_room_source}" ${_icons_only_start}
    ${_icons_only_length} _icons_only_source)
string(SUBSTRING "${_room_source}" ${_draw_room_start} -1 _draw_room_source)

string(REGEX REPLACE "[ \t\r\n]+" "" _room_items_normalized
    "${_room_items_source}")
string(REGEX REPLACE "[ \t\r\n]+" "" _icons_only_normalized
    "${_icons_only_source}")
string(REGEX REPLACE "[ \t\r\n]+" "" _draw_room_normalized
    "${_draw_room_source}")

string(REGEX MATCHALL "ground_loot_visible[ \t\r\n]*\\("
    _room_predicate_calls "${_room_source}")
list(LENGTH _room_predicate_calls _room_predicate_count)
if(NOT _room_predicate_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D room canonical predicate must appear exactly once; found ${_room_predicate_count}")
endif()
string(FIND "${_room_items_normalized}"
    "if(!ground_loot_visible(item,mode))continue;"
    _effective_predicate_index)
if(_effective_predicate_index EQUAL -1)
    message(FATAL_ERROR
        "Stage11D room canonical predicate must be the effective condition in draw_ground_items exactly once")
endif()

string(REGEX MATCHALL "build_ground_loot_view[ \t\r\n]*\\("
    _room_builder_calls "${_room_source}")
list(LENGTH _room_builder_calls _room_builder_count)
if(NOT _room_builder_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D room builder must appear exactly once; found ${_room_builder_count}")
endif()
string(REGEX MATCHALL
    "build_ground_loot_view\\(snapshot,loot_filter_mode_,width,height\\)"
    _icons_only_builder_calls "${_icons_only_normalized}")
list(LENGTH _icons_only_builder_calls _icons_only_builder_count)
if(NOT _icons_only_builder_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D icons-only builder must remain unique and locked to its dedicated scope")
endif()

string(REGEX MATCHALL
    "draw_ground_items\\(snapshot,loot_filter_mode_,material_pack_,width,height\\)"
    _icons_only_draw_calls "${_icons_only_normalized}")
list(LENGTH _icons_only_draw_calls _icons_only_draw_count)
if(NOT _icons_only_draw_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D icons-only draw must use the renderer loot filter mode exactly once")
endif()
string(REGEX MATCHALL
    "draw_ground_items\\(current,loot_filter_mode_,material_pack_,width,height\\)"
    _production_room_draw_calls "${_draw_room_normalized}")
list(LENGTH _production_room_draw_calls _production_room_draw_count)
if(NOT _production_room_draw_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D production room draw must use the renderer loot filter mode exactly once")
endif()

if(_room_source MATCHES "make_combat_render_plan[ \t\r\n]*\\(" OR
   _room_source MATCHES "LootFilterMode::")
    message(FATAL_ERROR
        "Stage11D room renderer must not create an independent render plan or fixed filter mode")
endif()
string(REGEX MATCHALL "settings::LootFilterMode" _room_mode_type_uses
    "${_room_source}")
list(LENGTH _room_mode_type_uses _room_mode_type_count)
string(REGEX MATCHALL "loot_filter_mode_" _room_mode_member_uses
    "${_room_source}")
list(LENGTH _room_mode_member_uses _room_mode_member_count)
if(NOT _room_mode_type_count EQUAL 1 OR NOT _room_mode_member_count EQUAL 3)
    message(FATAL_ERROR
        "Stage11D room filter mode wiring must match the canonical predicate and two draw paths")
endif()

string(FIND "${_combat_source}" "GroundLootView CombatRenderer::draw(" _draw_start)
if(_draw_start EQUAL -1)
    message(FATAL_ERROR "Stage11D renderer integration requires CombatRenderer::draw")
endif()
string(SUBSTRING "${_combat_source}" ${_draw_start} -1 _draw_source)

# Normalize formatting only. Variable names remain intact and are recovered below.
string(REGEX REPLACE "[ \t\r\n]+" "" _draw_normalized "${_draw_source}")

string(REGEX MATCHALL "make_combat_render_plan\\(" _factory_calls "${_draw_normalized}")
list(LENGTH _factory_calls _factory_call_count)
if(NOT _factory_call_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D render-plan factory must be called exactly once inside CombatRenderer::draw; found ${_factory_call_count}")
endif()

string(REGEX MATCH
    "constCombatRenderPlan([A-Za-z_][A-Za-z0-9_]*)=make_combat_render_plan\\("
    _plan_declaration
    "${_draw_normalized}")
if(NOT _plan_declaration)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must store its single factory result in a const CombatRenderPlan")
endif()
set(_plan_variable "${CMAKE_MATCH_1}")
set(_canonical_ground_loot "${_plan_variable}.ground_loot")

string(REGEX MATCHALL "build_ground_loot_view[ \t\r\n]*\\(" _builder_calls "${_combat_source}")
list(LENGTH _builder_calls _builder_call_count)
if(NOT _builder_call_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderPlan factory must build GroundLootView exactly once; found ${_builder_call_count}")
endif()

# A const-reference alias is semantically the same view and avoids another container copy.
set(_ground_loot_alias "")
string(REGEX MATCH
    "const(GroundLootView|auto)&([A-Za-z_][A-Za-z0-9_]*)=${_plan_variable}\\.ground_loot;"
    _alias_declaration
    "${_draw_normalized}")
if(_alias_declaration)
    set(_ground_loot_alias "${CMAKE_MATCH_2}")
endif()

set(_room_consumer_pattern
    "draw_room\\([^,;]+,([A-Za-z_][A-Za-z0-9_.]*)(,[^;]+)?\\)")
string(REGEX MATCHALL "${_room_consumer_pattern}" _room_consumer_calls "${_draw_normalized}")
list(LENGTH _room_consumer_calls _room_consumer_count)
if(NOT _room_consumer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must have exactly one room GroundLootView consumer")
endif()
string(REGEX MATCH "${_room_consumer_pattern}" _room_consumer "${_draw_normalized}")
set(_room_argument "${CMAKE_MATCH_1}")

set(_hud_consumer_pattern
    "hud_renderer_\\.draw_ground_loot\\(([A-Za-z_][A-Za-z0-9_.]*)\\)")
string(REGEX MATCHALL "${_hud_consumer_pattern}" _hud_consumer_calls "${_draw_normalized}")
list(LENGTH _hud_consumer_calls _hud_consumer_count)
if(NOT _hud_consumer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must have exactly one HUD GroundLootView consumer")
endif()
string(REGEX MATCH "${_hud_consumer_pattern}" _hud_consumer "${_draw_normalized}")
set(_hud_argument "${CMAKE_MATCH_1}")

foreach(_consumer IN ITEMS room hud)
    set(_argument "${_${_consumer}_argument}")
    if("${_argument}" STREQUAL "${_canonical_ground_loot}")
        continue()
    endif()
    if(NOT "${_ground_loot_alias}" STREQUAL "" AND
       "${_argument}" STREQUAL "${_ground_loot_alias}")
        continue()
    endif()
    message(FATAL_ERROR
        "Stage11D consumer divergence: room and HUD consumers must use the same GroundLootView from CombatRenderPlan (${_consumer} uses ${_argument})")
endforeach()

string(REGEX MATCHALL "return[A-Za-z_]" _return_calls "${_draw_normalized}")
list(LENGTH _return_calls _return_count)
if(NOT _return_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must return its shared GroundLootView exactly once")
endif()
string(FIND "${_draw_normalized}"
    "return${_canonical_ground_loot}" _canonical_return)
set(_alias_return -1)
if(NOT "${_ground_loot_alias}" STREQUAL "")
    string(FIND "${_draw_normalized}"
        "return${_ground_loot_alias}" _alias_return)
endif()
if(_canonical_return EQUAL -1 AND _alias_return EQUAL -1)
    message(FATAL_ERROR
        "Stage11D CombatRenderer::draw must return the same GroundLootView consumed by room and HUD")
endif()

message(STATUS
    "[stage11d-renderer-integration] factory_calls=${_factory_call_count} builder_calls=${_builder_call_count} room_consumers=${_room_consumer_count} hud_consumers=${_hud_consumer_count} shared_returns=${_return_count}")
