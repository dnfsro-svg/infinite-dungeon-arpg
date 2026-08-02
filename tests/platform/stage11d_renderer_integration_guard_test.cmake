if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

set(_combat_header_path "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.hpp")
set(_combat_source_path "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_room_source_path "${SOURCE_ROOT}/src/platform/raylib/room_renderer.cpp")
set(_hud_source_path "${SOURCE_ROOT}/src/platform/raylib/hud_renderer.cpp")
set(_host_source_path "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_runtime_source_path
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp")
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
if(DEFINED HOST_VALIDATION_RUNTIME_OVERRIDE)
    set(_runtime_source_path "${HOST_VALIDATION_RUNTIME_OVERRIDE}")
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
        "${_runtime_source_path}"
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
file(READ "${_runtime_source_path}" _runtime_source)
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

function(stage11d_renderer_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
    string(LENGTH "${_logical_source}" _source_length)
    set(_cursor 0)
    set(_conditional_depth 0)
    set(_surface "")
    while(_cursor LESS _source_length)
        string(SUBSTRING "${_logical_source}" ${_cursor} -1 _tail)
        string(FIND "${_tail}" "\n" _newline)
        if(_newline EQUAL -1)
            set(_line "${_tail}")
            set(_line_length -1)
        else()
            math(EXPR _line_length "${_newline} + 1")
            string(SUBSTRING "${_tail}" 0 ${_line_length} _line)
        endif()
        if(_line MATCHES
                "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} + 1")
        elseif(_line MATCHES "^[ \t]*#[ \t]*endif([ \t\r\n]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} - 1")
            if(_conditional_depth LESS 0)
                message(FATAL_ERROR
                    "Stage11D renderer conditional surface is unbalanced")
            endif()
        elseif(_conditional_depth EQUAL 0)
            string(APPEND _surface "${_line}")
        endif()
        if(_newline EQUAL -1)
            break()
        endif()
        math(EXPR _cursor "${_cursor} + ${_line_length}")
    endwhile()
    if(NOT _conditional_depth EQUAL 0)
        message(FATAL_ERROR
            "Stage11D renderer conditional surface is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
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

function(stage11d_renderer_matching_brace SURFACE OPEN_POSITION OUT_POSITION)
    string(LENGTH "${SURFACE}" _length)
    set(_depth 0)
    set(_close -1)
    while(OPEN_POSITION LESS _length)
        string(SUBSTRING "${SURFACE}" ${OPEN_POSITION} 1 _character)
        if(_character STREQUAL "{")
            math(EXPR _depth "${_depth} + 1")
        elseif(_character STREQUAL "}")
            math(EXPR _depth "${_depth} - 1")
            if(_depth EQUAL 0)
                set(_close ${OPEN_POSITION})
                break()
            endif()
        endif()
        math(EXPR OPEN_POSITION "${OPEN_POSITION} + 1")
    endwhile()
    if(_close EQUAL -1)
        message(FATAL_ERROR
            "Stage11D renderer direct-scope fixture has no closing brace")
    endif()
    set(${OUT_POSITION} ${_close} PARENT_SCOPE)
endfunction()

function(stage11d_renderer_mask_non_direct_scopes SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" _source_length)
    set(_masked "")
    set(_copy_cursor 0)
    set(_dead_condition
        "(false|0[uUlL]*|![ \t\r\n]*true|1[uUlL]*[ \t\r\n]*==[ \t\r\n]*0[uUlL]*|0[uUlL]*[ \t\r\n]*==[ \t\r\n]*1[uUlL]*)")
    while(_copy_cursor LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_copy_cursor} -1 _tail)
        string(REGEX MATCH
            "\\][ \t\r\n]*(\\([^{};]*\\))?[ \t\r\n]*(mutable[ \t\r\n]*)?(noexcept([ \t\r\n]*\\([^{};]*\\))?[ \t\r\n]*)?(->[^{;]*)?[ \t\r\n]*\\{"
            _lambda_match "${_tail}")
        string(REGEX MATCH
            "(if|while)[ \t\r\n]*(constexpr[ \t\r\n]*)?\\([ \t\r\n]*${_dead_condition}[ \t\r\n]*\\)[ \t\r\n]*(do[ \t\r\n]*)?([^{;]*\\{|[^{};]*;)"
            _dead_branch_match "${_tail}")
        string(REGEX MATCH
            "for[ \t\r\n]*\\([ \t\r\n]*;[ \t\r\n]*${_dead_condition}[ \t\r\n]*;[^)]*\\)[ \t\r\n]*([^{;]*\\{|[^{};]*;)"
            _dead_for_match "${_tail}")
        set(_scope_match "")
        set(_scope_relative -1)
        set(_scope_kind "")
        if(NOT _lambda_match STREQUAL "")
            string(FIND "${_tail}" "${_lambda_match}" _scope_relative)
            set(_scope_match "${_lambda_match}")
            set(_scope_kind lambda)
        endif()
        foreach(_candidate IN ITEMS _dead_branch_match _dead_for_match)
            set(_dead_match "${${_candidate}}")
            if(_dead_match STREQUAL "")
                continue()
            endif()
            string(FIND "${_tail}" "${_dead_match}" _dead_relative)
            if(_scope_relative EQUAL -1 OR _dead_relative LESS _scope_relative)
                set(_scope_match "${_dead_match}")
                set(_scope_relative ${_dead_relative})
                set(_scope_kind dead-control)
            endif()
        endforeach()
        if(_scope_relative EQUAL -1)
            string(APPEND _masked "${_tail}")
            break()
        endif()
        string(FIND "${_scope_match}" "{" _open_in_match)
        math(EXPR _match_index "${_copy_cursor} + ${_scope_relative}")
        if(_scope_kind STREQUAL "dead-control")
            set(_remove_begin ${_match_index})
        else()
            math(EXPR _remove_begin "${_match_index} + ${_open_in_match}")
        endif()
        math(EXPR _copy_length "${_remove_begin} - ${_copy_cursor}")
        if(_copy_length GREATER 0)
            string(SUBSTRING "${SOURCE}" ${_copy_cursor} ${_copy_length}
                _copy_chunk)
            string(APPEND _masked "${_copy_chunk}")
        endif()
        if(_open_in_match EQUAL -1)
            string(LENGTH "${_scope_match}" _scope_length)
            math(EXPR _scope_end "${_match_index} + ${_scope_length} - 1")
        else()
            math(EXPR _open_index "${_match_index} + ${_open_in_match}")
            stage11d_renderer_matching_brace("${SOURCE}" ${_open_index}
                _scope_end)
        endif()
        math(EXPR _copy_cursor "${_scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${_masked}" PARENT_SCOPE)
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
string(REGEX REPLACE "[ \t\r\n]+" "" _report_target_normalized
    "${_report_target_function}")
if(_report_target_normalized MATCHES "GetScreen(Width|Height)[(]")
    message(FATAL_ERROR
        "T7C-M06-global: Stage11D target visibility rejected globals")
endif()
foreach(_required_dimension_binding IN ITEMS
        "intscreen_width,intscreen_height"
        "make_hud_layout(screen_width,screen_height,false)")
    string(FIND "${_report_target_normalized}"
        "${_required_dimension_binding}" _dimension_found)
    if(_dimension_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D target visibility rejected explicit screen dimensions")
    endif()
endforeach()

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
stage11d_renderer_unconditional_cpp_surface("${_host_source}" _host_code)
stage11d_renderer_unconditional_cpp_surface("${_runtime_source}" _runtime_code)
evidence_extract_cpp_function_block("${_host_code}"
    "HostExitCode run_raylib_host(" _run_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _run_normalized "${_run_code}")
stage11d_renderer_mask_non_direct_scopes("${_run_code}" _run_direct_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _run_direct_normalized
    "${_run_direct_code}")
string(REPLACE "validation_runtime.get()->" "validation_runtime->"
    _run_direct_normalized "${_run_direct_normalized}")

stage11d_count_token("${_runtime_code}"
    "void HostValidationRuntime::observe_ground_loot(" _ground_owner_count)
if(NOT _ground_owner_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D renderer requires one active runtime ground-loot owner")
endif()
evidence_extract_cpp_function_block("${_runtime_code}"
    "void HostValidationRuntime::observe_ground_loot(" _ground_owner)
stage11d_renderer_mask_non_direct_scopes("${_ground_owner}"
    _ground_owner_direct)
string(REGEX REPLACE "[ \t\r\n]+" "" _ground_owner_normalized
    "${_ground_owner_direct}")
string(FIND "${_ground_owner_normalized}"
    "if(impl_->states.stage11d.target_visible&&!impl_->states.stage11d.captured){host_validation::stage11d_record_semantics("
    _ground_record_condition)
if(_ground_record_condition EQUAL -1)
    message(FATAL_ERROR
        "T7C-ground-condition: Stage11D rejected direct semantic gate")
endif()
foreach(_binding IN ITEMS
        "stage11d_target_visible("
        "stage11d_record_semantics("
        "ground_loot_view"
        "notices"
        "screen_width"
        "screen_height")
    string(FIND "${_ground_owner_normalized}" "${_binding}" _binding_found)
    if(_binding_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D runtime ground-loot owner rejected binding: ${_binding}")
    endif()
endforeach()
if(_ground_owner_normalized MATCHES "GetScreen(Width|Height)[(]")
    message(FATAL_ERROR
        "T7C-M06-global: Stage11D runtime rejected global dimensions")
endif()

set(_ground_anchor "constGroundLootViewground_loot_view=[&]()noexcept{")
string(FIND "${_run_normalized}" "${_ground_anchor}" _ground_begin)
if(_ground_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D host cannot bind the presented GroundLootView")
endif()
stage11d_count_token("${_run_normalized}"
    "renderer.draw_ground_loot_icons_only(presented_snapshot,frame_camera)"
    _icons_only_frame_camera_count)
if(NOT _icons_only_frame_camera_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D icons-only host path must reuse the single presented-frame camera")
endif()
string(SUBSTRING "${_run_normalized}" ${_ground_begin} -1 _presented_surface)
set(_ground_direct_anchor
    "constGroundLootViewground_loot_view=[&]()noexcept")
string(FIND "${_run_direct_normalized}" "${_ground_direct_anchor}"
    _ground_direct_begin)
if(_ground_direct_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D host cannot bind the direct presented GroundLootView")
endif()
string(SUBSTRING "${_run_direct_normalized}" ${_ground_direct_begin} -1
    _presented_direct_surface)

string(REGEX MATCHALL "constGroundLootView[A-Za-z_][A-Za-z0-9_]*="
    _ground_views "${_run_normalized}")
list(LENGTH _ground_views _ground_view_count)
if(NOT _ground_view_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D host rejected rebuilt second GroundLootView")
endif()
stage11d_count_token("${_run_normalized}" "build_ground_loot_view("
    _host_ground_builder_count)
if(NOT _host_ground_builder_count EQUAL 0)
    message(FATAL_ERROR
        "T7C-M04-builder: Stage11D rejected direct GroundLootView rebuild")
endif()
string(REGEX MATCHALL "validation_runtime->observe_ground_loot[(]"
    _ground_observers "${_presented_direct_surface}")
list(LENGTH _ground_observers _ground_observer_count)
if(NOT _ground_observer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D host requires one ground-loot facade observer")
endif()
set(_ground_observer_shared
    "validation_runtime->observe_ground_loot(current,pause_menu,runtime.render_status(),presented_loot_filter,ground_loot_view,")
set(_ground_observer_notices
    "${_ground_observer_shared}renderer.hud_notice_view(),runtime.item_state(),")
set(_ground_observer_call
    "${_ground_observer_notices}GetScreenWidth(),GetScreenHeight());")
string(FIND "${_presented_direct_surface}" "${_ground_observer_shared}"
    _ground_shared_position)
string(FIND "${_presented_direct_surface}" "${_ground_observer_notices}"
    _ground_notices_position)
string(FIND "${_presented_direct_surface}" "${_ground_observer_call}"
    _ground_observer_position)
if(_ground_shared_position EQUAL -1)
    message(FATAL_ERROR "T7C-M04: Stage11D rejected shared GroundLootView")
elseif(_ground_notices_position EQUAL -1)
    message(FATAL_ERROR "T7C-M05: Stage11D rejected fresh HUD notices")
elseif(_ground_observer_position EQUAL -1)
    message(FATAL_ERROR "T7C-M06: Stage11D rejected real screen dimensions")
endif()
string(FIND "${_run_direct_normalized}" "${_ground_direct_anchor}"
    _ground_run_position)
string(FIND "${_run_direct_normalized}"
    "validation_runtime->observe_ground_loot(" _observer_run_position)
stage11d_brace_depth("${_run_direct_normalized}" ${_ground_run_position}
    _ground_run_depth)
stage11d_brace_depth("${_run_direct_normalized}" ${_observer_run_position}
    _observer_run_depth)
if(NOT _ground_run_depth EQUAL _observer_run_depth)
    message(FATAL_ERROR
        "Stage11D host rejected ground-loot observer owner scope")
endif()

string(FIND "${_presented_direct_surface}"
    "validation_runtime->observe_active_skill_draw("
    _stage17_draw_observer_position)
if(_stage17_draw_observer_position EQUAL -1
        OR NOT _stage17_draw_observer_position LESS _ground_observer_position)
    message(FATAL_ERROR
        "T7C-M01: Stage11D ground-loot observer must follow the Stage17 draw observer")
endif()

string(FIND "${_presented_direct_surface}"
    "if(config.stage12_material_runtime_status!=nullptr){"
    _stage12_status_position)
string(FIND "${_presented_direct_surface}"
    "material_status.bundled_font_total_atlas_byte_budget=kUiFontTotalAtlasByteBudget;"
    _stage12_status_end_position)
if(_stage12_status_position EQUAL -1
        OR _stage12_status_end_position EQUAL -1
        OR NOT _stage12_status_position LESS _stage12_status_end_position
        OR NOT _stage12_status_end_position LESS _ground_observer_position)
    message(FATAL_ERROR
        "T7C-M02: Stage11D ground-loot observer must follow Stage12 runtime-status collection")
endif()

foreach(_overlay IN ITEMS
        "pause_menu_renderer.draw("
        "inventory.draw("
        "draw_passive_tree_overlay(")
    if(_overlay STREQUAL "draw_passive_tree_overlay(")
        set(_overlay_diagnostic "T7C-M03-passive")
    elseif(_overlay STREQUAL "inventory.draw(")
        set(_overlay_diagnostic "T7C-M03-inventory")
    else()
        set(_overlay_diagnostic "T7C-M03-pause")
    endif()
    string(FIND "${_presented_direct_surface}" "${_overlay}" _overlay_position)
    if(_overlay_position EQUAL -1
            OR NOT _ground_observer_position LESS _overlay_position)
        message(FATAL_ERROR
            "${_overlay_diagnostic}: Stage11D ground-loot observer must precede its overlay")
    endif()
endforeach()

string(FIND "${_presented_surface}"
    "constPresentationDecisiondecision=validation_runtime->observe_presented_frame("
    _presented_observer)
if(_presented_observer EQUAL -1)
    message(FATAL_ERROR
        "Stage11D host rejected presented-frame facade observation")
endif()
foreach(_overlay IN ITEMS
        "draw_passive_tree_overlay("
        "inventory.draw("
        "pause_menu_renderer.draw(")
    string(FIND "${_presented_surface}" "${_overlay}" _overlay_position)
    if(NOT _overlay_position LESS _presented_observer)
        message(FATAL_ERROR
            "Stage11D host rejected presented-frame observer before overlays")
    endif()
endforeach()

string(REGEX MATCHALL "validation_runtime->observe_capture_result[(]"
    _capture_callbacks "${_presented_surface}")
list(LENGTH _capture_callbacks _capture_callback_count)
if(NOT _capture_callback_count EQUAL 1)
    message(FATAL_ERROR
        "T7C-M19: Stage11D requires one capture-result callback")
endif()
string(FIND "${_presented_surface}" "present_frame_and_maybe_capture("
    _present_call)
string(FIND "${_presented_surface}"
    "validation_runtime->observe_capture_result(" _capture_callback)
if(_present_call EQUAL -1 OR NOT _present_call LESS _capture_callback)
    message(FATAL_ERROR
        "Stage11D host rejected post-present capture-result callback")
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
# the shared GroundItemRange renderer, one builder in
# draw_ground_loot_icons_only, and the same renderer-owned filter mode and
# frame camera at both room call sites.
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

string(FIND "${_icons_only_normalized}"
    "draw_ground_loot_icons_only(constdungeon::DungeonSnapshot&snapshot,constCombatCameraView&camera)noexcept{"
    _icons_only_camera_parameter)
if(_icons_only_camera_parameter EQUAL -1 OR
   _icons_only_normalized MATCHES "make_combat_camera_view[(]")
    message(FATAL_ERROR
        "Stage11D icons-only path must consume, not reconstruct, the presented-frame camera")
endif()

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
    "build_ground_loot_view\\(snapshot,loot_filter_mode_,camera,width,height\\)"
    _icons_only_builder_calls "${_icons_only_normalized}")
list(LENGTH _icons_only_builder_calls _icons_only_builder_count)
if(NOT _icons_only_builder_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D icons-only builder must remain unique and locked to its dedicated scope")
endif()

string(REGEX MATCHALL
    "draw_ground_items\\(ground_item_range\\(snapshot\\),loot_filter_mode_,material_pack_,camera,width,height\\)"
    _icons_only_draw_calls "${_icons_only_normalized}")
list(LENGTH _icons_only_draw_calls _icons_only_draw_count)
if(NOT _icons_only_draw_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D icons-only draw must use the renderer loot filter mode exactly once")
endif()
string(REGEX MATCHALL
    "draw_ground_items\\(ground_item_range\\(current\\),loot_filter_mode_,material_pack_,camera,width,height\\)"
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
