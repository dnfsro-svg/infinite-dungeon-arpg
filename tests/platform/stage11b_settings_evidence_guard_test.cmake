if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
else()
    set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
endif()
if(DEFINED STAGE_OVERRIDE)
    set(_stage "${STAGE_OVERRIDE}")
else()
    set(_stage "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.cpp")
endif()
if(DEFINED RUNTIME_OVERRIDE)
    set(_runtime "${RUNTIME_OVERRIDE}")
else()
    set(_runtime
        "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp")
endif()
set(_stage_header "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.hpp")
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_pause_renderer "${SOURCE_ROOT}/src/platform/raylib/pause_menu_renderer.cpp")
set(_font_source "${SOURCE_ROOT}/src/platform/raylib/death_overlay_font.cpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
else()
    set(_formal "${SOURCE_ROOT}/tests/platform/stage11b_settings_formal_game_validation.cpp")
endif()
foreach(_required IN ITEMS "${_host}" "${_stage}" "${_runtime}"
        "${_stage_header}" "${_header}" "${_formal}"
        "${_pause_renderer}" "${_font_source}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11B evidence target is missing: ${_required}")
    endif()
endforeach()

file(READ "${_host}" _host_text)
file(READ "${_stage}" _stage_text)
file(READ "${_runtime}" _runtime_text)
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_header}" _header_text)
file(READ "${_formal}" _formal_text)
file(READ "${_pause_renderer}" _pause_renderer_text)
file(READ "${_font_source}" _font_source_text)
set(_combined "${_header}\n${_host_text}\n${_formal_text}")

function(stage11b_extract_sanitized_block SANITIZED_SOURCE SIGNATURE OUT_BLOCK)
    evidence_find_cpp_function_bounds_in_sanitized(
        "${SANITIZED_SOURCE}" "${SIGNATURE}"
        _block_begin _block_open _block_end)
    math(EXPR _block_length "${_block_end} - ${_block_begin} + 1")
    string(SUBSTRING "${SANITIZED_SOURCE}"
        ${_block_begin} ${_block_length} _block)
    set(${OUT_BLOCK} "${_block}" PARENT_SCOPE)
endfunction()

function(stage11b_count_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${TOKEN}" token_length)
    string(LENGTH "${SOURCE}" before_length)
    string(REPLACE "${TOKEN}" "" without_token "${SOURCE}")
    string(LENGTH "${without_token}" after_length)
    math(EXPR token_count
        "(${before_length} - ${after_length}) / ${token_length}")
    set(${OUT_COUNT} ${token_count} PARENT_SCOPE)
endfunction()

function(stage11b_brace_depth SOURCE POSITION OUT_DEPTH)
    string(SUBSTRING "${SOURCE}" 0 ${POSITION} prefix)
    string(REGEX REPLACE "[^{}]" "" braces "${prefix}")
    string(LENGTH "${braces}" brace_length)
    set(depth 0)
    if(brace_length GREATER 0)
        math(EXPR brace_last "${brace_length} - 1")
        foreach(brace_index RANGE 0 ${brace_last})
            string(SUBSTRING "${braces}" ${brace_index} 1 brace)
            if(brace STREQUAL "{")
                math(EXPR depth "${depth} + 1")
            else()
                math(EXPR depth "${depth} - 1")
            endif()
        endforeach()
    endif()
    set(${OUT_DEPTH} ${depth} PARENT_SCOPE)
endfunction()

function(stage11b_unconditional_cpp_surface SOURCE OUT_ACTIVE OUT_LEXICAL)
    evidence_sanitize_cpp_for_scan("${SOURCE}" lexical)
    string(LENGTH "${lexical}" source_length)
    set(cursor 0)
    set(conditional_depth 0)
    set(active "")
    while(cursor LESS source_length)
        string(SUBSTRING "${lexical}" ${cursor} -1 tail)
        string(FIND "${tail}" "\n" newline)
        if(newline EQUAL -1)
            set(line "${tail}")
            set(line_length -1)
        else()
            math(EXPR line_length "${newline} + 1")
            string(SUBSTRING "${tail}" 0 ${line_length} line)
        endif()
        set(mask_line FALSE)
        if(line MATCHES
                "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR conditional_depth "${conditional_depth} + 1")
            set(mask_line TRUE)
        elseif(line MATCHES "^[ \t]*#[ \t]*endif([ \t\r\n]|$)")
            if(conditional_depth EQUAL 0)
                message(FATAL_ERROR
                    "Stage11B conditional owner surface is unbalanced")
            endif()
            math(EXPR conditional_depth "${conditional_depth} - 1")
            set(mask_line TRUE)
        elseif(conditional_depth GREATER 0)
            set(mask_line TRUE)
        endif()
        if(mask_line)
            string(REGEX REPLACE "[^\r\n]" " " line "${line}")
        endif()
        string(APPEND active "${line}")
        if(newline EQUAL -1)
            break()
        endif()
        math(EXPR cursor "${cursor} + ${line_length}")
    endwhile()
    if(NOT conditional_depth EQUAL 0)
        message(FATAL_ERROR
            "Stage11B conditional owner surface is unbalanced")
    endif()
    set(${OUT_ACTIVE} "${active}" PARENT_SCOPE)
    set(${OUT_LEXICAL} "${lexical}" PARENT_SCOPE)
endfunction()

function(stage11b_find_balanced_scope_end SOURCE OPEN_INDEX OUT_END OUT_VALID)
    string(LENGTH "${SOURCE}" source_length)
    set(cursor ${OPEN_INDEX})
    set(depth 0)
    while(cursor LESS source_length)
        string(SUBSTRING "${SOURCE}" ${cursor} 1 character)
        if(character STREQUAL "{")
            math(EXPR depth "${depth} + 1")
        elseif(character STREQUAL "}")
            math(EXPR depth "${depth} - 1")
            if(depth EQUAL 0)
                set(${OUT_END} ${cursor} PARENT_SCOPE)
                set(${OUT_VALID} TRUE PARENT_SCOPE)
                return()
            endif()
        endif()
        math(EXPR cursor "${cursor} + 1")
    endwhile()
    set(${OUT_END} -1 PARENT_SCOPE)
    set(${OUT_VALID} FALSE PARENT_SCOPE)
endfunction()

function(stage11b_mask_non_direct_scopes SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" source_length)
    set(masked "")
    set(copy_cursor 0)
    while(copy_cursor LESS source_length)
        string(SUBSTRING "${SOURCE}" ${copy_cursor} -1 tail)
        string(REGEX MATCH
            "\\][ \t\r\n]*(\\([^{};]*\\))?[ \t\r\n]*(mutable[ \t\r\n]*)?(noexcept([ \t\r\n]*\\([^{};]*\\))?[ \t\r\n]*)?(->[^{;]*)?[ \t\r\n]*\\{"
            lambda_match "${tail}")
        string(REGEX MATCH
            "(if|while)[ \t\r\n]*(constexpr[ \t\r\n]*)?\\([ \t\r\n]*(false|0[uUlL]*|![ \t\r\n]*true)[ \t\r\n]*\\)[ \t\r\n]*(do[ \t\r\n]*)?([^{;]*\\{|[^{};]*;)"
            dead_match "${tail}")
        set(scope_match "")
        set(scope_relative -1)
        set(scope_kind "")
        if(NOT lambda_match STREQUAL "")
            string(FIND "${tail}" "${lambda_match}" scope_relative)
            set(scope_match "${lambda_match}")
            set(scope_kind lambda)
        endif()
        if(NOT dead_match STREQUAL "")
            string(FIND "${tail}" "${dead_match}" dead_relative)
            if(scope_relative EQUAL -1 OR dead_relative LESS scope_relative)
                set(scope_match "${dead_match}")
                set(scope_relative ${dead_relative})
                set(scope_kind dead)
            endif()
        endif()
        if(scope_relative EQUAL -1)
            string(APPEND masked "${tail}")
            break()
        endif()
        string(FIND "${scope_match}" "{" open_in_match)
        math(EXPR match_index "${copy_cursor} + ${scope_relative}")
        if(scope_kind STREQUAL "dead")
            set(remove_begin ${match_index})
        else()
            math(EXPR remove_begin "${match_index} + ${open_in_match}")
        endif()
        math(EXPR copy_length "${remove_begin} - ${copy_cursor}")
        if(copy_length GREATER 0)
            string(SUBSTRING "${SOURCE}" ${copy_cursor} ${copy_length} chunk)
            string(APPEND masked "${chunk}")
        endif()
        if(open_in_match EQUAL -1)
            string(LENGTH "${scope_match}" match_length)
            math(EXPR scope_end "${match_index} + ${match_length} - 1")
        else()
            math(EXPR open_index "${match_index} + ${open_in_match}")
            stage11b_find_balanced_scope_end(
                "${SOURCE}" ${open_index} scope_end scope_valid)
            if(NOT scope_valid)
                message(FATAL_ERROR
                    "Stage11B direct-scope fixture has no closing brace")
            endif()
        endif()
        math(EXPR copy_cursor "${scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${masked}" PARENT_SCOPE)
endfunction()

function(stage11b_extract_unique_owner
        ACTIVE LEXICAL SIGNATURE LABEL OUT_BLOCK)
    stage11b_count_token("${ACTIVE}" "${SIGNATURE}" active_count)
    stage11b_count_token("${LEXICAL}" "${SIGNATURE}" lexical_count)
    if(NOT active_count EQUAL 1 OR NOT lexical_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11B ${LABEL} must have one active and lexical definition")
    endif()
    foreach(owner_surface IN ITEMS ACTIVE LEXICAL)
        string(FIND "${${owner_surface}}" "${SIGNATURE}" owner_position)
        stage11b_brace_depth("${${owner_surface}}" ${owner_position}
            owner_depth)
        if(NOT owner_depth EQUAL 1)
            message(FATAL_ERROR
                "Stage11B ${LABEL} must be a namespace-level definition")
        endif()
    endforeach()
    stage11b_extract_sanitized_block("${ACTIVE}" "${SIGNATURE}" owner_block)
    stage11b_mask_non_direct_scopes("${owner_block}" owner_direct)
    set(${OUT_BLOCK} "${owner_direct}" PARENT_SCOPE)
endfunction()

evidence_sanitize_cpp_for_scan("${_stage_text}" _stage_code)
stage11b_unconditional_cpp_surface("${_runtime_text}"
    _runtime_active _runtime_lexical)
stage11b_unconditional_cpp_surface("${_host_text}"
    _host_active _host_lexical)
stage11b_extract_sanitized_block("${_stage_code}"
    "PhysicalKeySnapshot inject_stage11b_physical_edges(" _stage11b_injection_block)
stage11b_extract_sanitized_block("${_stage_code}"
    "bool stage11b_validation_complete(" _stage11b_complete_block)
stage11b_extract_sanitized_block("${_stage_code}"
    "std::uint64_t stage11b_snapshot_hash(" _stage11b_hash_block)
stage11b_extract_sanitized_block("${_stage_code}"
    "void write_stage11b_validation_summary(" _stage11b_summary_block)
evidence_find_cpp_code_token("${_host_text}"
    "if (forward_actions) {" _host_actions_begin)
if(_host_actions_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires accepted queue_action evidence")
endif()
string(SUBSTRING "${_host_text}" ${_host_actions_begin} -1
    _host_actions_tail)
evidence_find_cpp_code_token("${_host_actions_tail}"
    "if (forward_descent && frame_input.keys.e)" _host_actions_end)
if(_host_actions_end EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires accepted queue_action evidence")
endif()
string(SUBSTRING "${_host_actions_tail}" 0 ${_host_actions_end}
    _host_actions_slice)
evidence_sanitize_cpp_for_scan("${_host_actions_slice}"
    _host_actions_block)
stage11b_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_submitted_actions("
    "submitted-actions owner" _submitted_observer_block)
stage11b_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_pause_transition("
    "pause-transition owner" _pause_transition_block)
stage11b_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "PresentationDecision HostValidationRuntime::observe_presented_frame("
    "presented-frame owner" _presented_frame_block)
stage11b_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_capture_result("
    "capture-result owner" _capture_result_block)
stage11b_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::write_summaries("
    "summary owner" _runtime_summary_block)
stage11b_extract_unique_owner("${_host_active}" "${_host_lexical}"
    "HostExitCode run_raylib_host("
    "Host run owner" _host_run_block)

function(arpg_require_stage11b_block_token LABEL BLOCK TOKEN)
    # All callers pass blocks extracted from the single sanitized Stage surface.
    string(FIND "${BLOCK}" "${TOKEN}" _token_found)
    if(_token_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing ${LABEL} token: ${TOKEN}")
    endif()
endfunction()

function(stage11b_require_direct_token LABEL BLOCK TOKEN)
    stage11b_count_token("${BLOCK}" "${TOKEN}" token_count)
    string(FIND "${BLOCK}" "${TOKEN}" token_position)
    if(NOT token_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11B ${LABEL} direct token is missing or duplicated: ${TOKEN}")
    endif()
    stage11b_brace_depth("${BLOCK}" ${token_position} token_depth)
    if(NOT token_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11B ${LABEL} token is outside direct method scope: ${TOKEN}")
    endif()
endfunction()

foreach(_required IN ITEMS
        "Stage11BValidationScenario"
        "sample_physical_keys"
        "map_host_frame_input"
        "submit_frame_actions"
        "present_frame_and_maybe_capture"
        "SettingsStore"
        "settings_store.load()"
        "settings_store.save("
        "run_child"
        "restarted_settings"
        "paused_freeze"
        "rebound_attack")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing required token: ${_required}")
    endif()
endforeach()

arpg_require_stage11b_block_token("Stage injection" "${_stage11b_injection_block}" "StableKey::j")
arpg_require_stage11b_block_token("Stage injection" "${_stage11b_injection_block}" "StableKey::u")
arpg_require_stage11b_block_token("Stage completion" "${_stage11b_complete_block}" "state.pause_capture_while_paused")
arpg_require_stage11b_block_token("Stage hash" "${_stage11b_hash_block}" "mix(snapshot.depth)")
arpg_require_stage11b_block_token("Stage summary" "${_stage11b_summary_block}" "state.player_monster_hash_before <<")
arpg_require_stage11b_block_token("Stage summary" "${_stage11b_summary_block}" "state.load_status")
string(REGEX REPLACE "[ \t\r\n]+" "" _pause_transition_compact
    "${_pause_transition_block}")
stage11b_require_direct_token("pause-transition owner"
    "${_pause_transition_block}"
    "if (impl_->config->stage11b_validation")
set(_pause_transition_contract
    "if(impl_->config->stage11b_validation==Stage11BValidationScenario::paused_freeze&&was_open&&!is_open&&impl_->states.stage11b.resume_input_injected){impl_->states.stage11b.resume_observed=true;impl_->states.stage11b.resume_ticks_before=impl_->states.stage11b.fixed_ticks;}")
stage11b_count_token("${_pause_transition_compact}"
    "${_pause_transition_contract}" _pause_transition_contract_count)
if(NOT _pause_transition_contract_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11B pause-transition owner lacks the exact resume contract")
endif()

string(REGEX REPLACE "[ \t\r\n]+" "" _presented_frame_compact
    "${_presented_frame_block}")
foreach(_stage11b_present_direct_token IN ITEMS
        "if (impl_->states.stage11b.resume_observed)"
        "const bool stage11b_paused_visible_capture ="
        "impl_->pending_stage11b_paused_visible_capture ="
        "decision.validation_complete ="
        "return decision;")
    stage11b_require_direct_token("presented-frame owner"
        "${_presented_frame_block}" "${_stage11b_present_direct_token}")
endforeach()
stage11b_require_direct_token("presented-frame owner"
    "${_presented_frame_block}"
    "if (impl_->config->stage11b_validation
            == Stage11BValidationScenario::paused_freeze
        && pause_menu.screen != PauseScreen::closed)")
set(_stage11b_resume_after_contract
    "if(impl_->states.stage11b.resume_observed){impl_->states.stage11b.resume_ticks_after=impl_->states.stage11b.fixed_ticks;}")
set(_stage11b_paused_contract
    "if(impl_->config->stage11b_validation==Stage11BValidationScenario::paused_freeze&&pause_menu.screen!=PauseScreen::closed){if(impl_->states.stage11b.paused_presented==0U){impl_->states.stage11b.paused_ticks_before=impl_->states.stage11b.fixed_ticks;impl_->states.stage11b.player_monster_hash_before=host_validation::stage11b_snapshot_hash(snapshot);}++impl_->states.stage11b.paused_presented;impl_->states.stage11b.paused_ticks_after=impl_->states.stage11b.fixed_ticks;impl_->states.stage11b.player_monster_hash_after=host_validation::stage11b_snapshot_hash(snapshot);}")
set(_stage11b_visible_capture_contract
    "constboolstage11b_paused_visible_capture=impl_->config->stage11b_validation==Stage11BValidationScenario::paused_freeze&&pause_menu.screen!=PauseScreen::closed&&impl_->states.stage11b.paused_presented>=120U&&!impl_->states.stage11b.pause_capture_while_paused;")
set(_stage11b_pending_contract
    "impl_->pending_stage11b_paused_visible_capture=stage11b_paused_visible_capture;")
set(_stage11b_completion_contract
    "decision.validation_complete=stage10_reached||stage11_reached||stage11b_reached||stage11c_reached||stage11d_reached||stage17_reached;")
foreach(_stage11b_present_contract IN ITEMS
        _stage11b_resume_after_contract _stage11b_paused_contract
        _stage11b_visible_capture_contract _stage11b_pending_contract
        _stage11b_completion_contract)
    stage11b_count_token("${_presented_frame_compact}"
        "${${_stage11b_present_contract}}" _stage11b_present_contract_count)
    if(NOT _stage11b_present_contract_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11B presented-frame owner lacks exact contract: ${_stage11b_present_contract}")
    endif()
endforeach()
set(_stage11b_present_previous -1)
foreach(_stage11b_present_contract IN ITEMS
        _stage11b_resume_after_contract _stage11b_paused_contract
        _stage11b_visible_capture_contract _stage11b_pending_contract
        _stage11b_completion_contract)
    string(FIND "${_presented_frame_compact}"
        "${${_stage11b_present_contract}}" _stage11b_present_position)
    if(NOT _stage11b_present_previous EQUAL -1
            AND _stage11b_present_position LESS _stage11b_present_previous)
        message(FATAL_ERROR
            "Stage11B presented-frame owner is reordered")
    endif()
    set(_stage11b_present_previous ${_stage11b_present_position})
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" "" _capture_result_compact
    "${_capture_result_block}")
stage11b_require_direct_token("capture-result owner"
    "${_capture_result_block}"
    "if (owner != CaptureOwner::generic_validation)")
stage11b_require_direct_token("capture-result owner"
    "${_capture_result_block}"
    "impl_->states.stage11b.pause_capture_while_paused =")
set(_stage11b_capture_owner_contract
    "if(owner!=CaptureOwner::generic_validation){if(owner==CaptureOwner::stage17&&succeeded){host_validation::mark_stage17_capture_complete(impl_->states.stage17);}return;}")
set(_stage11b_capture_assignment
    "impl_->states.stage11b.pause_capture_while_paused=impl_->pending_stage11b_paused_visible_capture;")
stage11b_count_token("${_capture_result_compact}"
    "${_stage11b_capture_owner_contract}" _stage11b_capture_owner_count)
stage11b_count_token("${_capture_result_compact}"
    "${_stage11b_capture_assignment}" _stage11b_capture_assignment_count)
stage11b_count_token("${_capture_result_compact}"
    "impl_->states.stage11b.pause_capture_while_paused="
    _stage11b_capture_lhs_count)
stage11b_count_token("${_capture_result_compact}"
    "impl_->states.stage11b.pause_capture_while_paused"
    _stage11b_capture_state_reference_count)
if(NOT _stage11b_capture_owner_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11B capture-result owner arbitration is missing or altered")
endif()
if(NOT _stage11b_capture_assignment_count EQUAL 1
        OR NOT _stage11b_capture_lhs_count EQUAL 1
        OR NOT _stage11b_capture_state_reference_count EQUAL 1)
    message(FATAL_ERROR
        "T7C-M22: paused capture must assign the pending value exactly")
endif()
string(FIND "${_capture_result_compact}"
    "${_stage11b_capture_owner_contract}" _stage11b_capture_owner_position)
string(FIND "${_capture_result_compact}"
    "${_stage11b_capture_assignment}" _stage11b_capture_assignment_position)
string(FIND "${_capture_result_compact}"
    "if(!succeeded)return;" _stage11b_capture_success_position)
if(_stage11b_capture_success_position EQUAL -1
        OR NOT _stage11b_capture_owner_position LESS
            _stage11b_capture_assignment_position
        OR NOT _stage11b_capture_assignment_position LESS
            _stage11b_capture_success_position)
    message(FATAL_ERROR
        "Stage11B capture-result owner flow is missing or reordered")
endif()
stage11b_count_token("${_runtime_summary_block}"
    "host_validation::write_stage11b_validation_summary("
    _stage11b_summary_owner_count)
if(NOT _stage11b_summary_owner_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11B runtime summary owner is missing or duplicated")
endif()
if(NOT _stage_header_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Stage11B evidence guard missing Stage state definition")
endif()
foreach(_host_definition IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary(")
    string(FIND "${_host_text}" "${_host_definition}" _host_definition_found)
    if(NOT _host_definition_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard found Stage definition in host: ${_host_definition}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "quote_command_argument"
        "run_a.sav"
        "run_b.sav"
        "establish_v6_character_slots")
    string(FIND "${_formal_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing real persistence path: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS "PauseMenuRenderer" "draw_crisp_ui_text"
        "record_ui_text_bounds" "设置已恢复默认值")
    string(FIND "${_pause_renderer_text}${_font_source_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing CJK notice rendering: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS "TestAccess" "validation_input_setter"
        "queue_action" "pause_menu.committed =" "LoadImageFromScreen()"
        "character-save-sentinel")
    string(FIND "${_formal_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard rejected forbidden token: ${_forbidden}")
    endif()
endforeach()

string(FIND "${_formal_text}" "platform::run_raylib_host(config)"
    _formal_host_call)
if(_formal_host_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires formal harness to invoke platform::run_raylib_host(config)")
endif()
string(FIND "${_formal_text}" "std::system(command.c_str())"
    _formal_child_call)
if(_formal_child_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires independent child-process invocation via std::system(command.c_str())")
endif()

foreach(_host_forbidden IN ITEMS
        "pause_input.focus_lost = false"
        "snapshot.down.fill(false)"
        "snapshot.pressed.fill(false)"
        "snapshot.escape = false"
        "snapshot.enter = false")
    string(FIND "${_host_text}" "${_host_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard rejected host bypass: ${_host_forbidden}")
    endif()
endforeach()
string(FIND "${_host_text}"
    "? !physical_keys.focus_lost : true" _forced_focus_context)
if(NOT _forced_focus_context EQUAL -1)
    message(FATAL_ERROR "Stage11B evidence guard rejected forced focused pause context")
endif()

string(FIND "${_host_active}" "EndDrawing();" _present)
string(FIND "${_host_active}" "LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR _capture LESS _present)
    message(FATAL_ERROR "Stage11B evidence guard requires one post-Present capture helper")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \\t\\n]*\\(" _screen_loads "${_host_active}")
list(LENGTH _screen_loads _screen_load_count)
string(REGEX MATCHALL "runtime\\.fixed_tick[ \\t\\n]*\\(" _fixed_ticks "${_host_run_block}")
list(LENGTH _fixed_ticks _fixed_tick_count)
if(NOT _screen_load_count EQUAL 1)
    message(FATAL_ERROR "Stage11B-count-capture: requires one screen capture")
endif()
if(NOT _fixed_tick_count EQUAL 1)
    message(FATAL_ERROR "Stage11B-count-fixed: requires one fixed-tick path")
endif()
string(REGEX REPLACE "[ \t\r\n]+" "" _normalized_host "${_host_run_block}")
string(FIND "${_normalized_host}"
    "if(host_gate.forward_gameplay)" _fixed_gate)
string(FIND "${_normalized_host}"
    "runtime.fixed_tick(step_movement,loot_pickup_policy(live_settings.loot_filter_mode));"
    _live_policy_call)
string(FIND "${_normalized_host}"
    "runtime.fixed_tick(step_movement,loot_pickup_policy(pause_menu.draft.loot_filter_mode));"
    _draft_policy_call)
if(NOT _draft_policy_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard rejects draft loot policy in fixed_tick")
endif()
if(_fixed_gate EQUAL -1 OR _live_policy_call EQUAL -1
        OR NOT _fixed_gate LESS _live_policy_call)
    message(FATAL_ERROR
        "Stage11B evidence guard requires live loot policy behind host gate")
endif()
string(FIND "${_host_actions_block}"
    "const SubmittedFrameActions submitted_actions =" _accepted_actions)
string(FIND "${_host_actions_block}"
    "validation_runtime->observe_submitted_actions(submitted_actions);"
    _accepted_observer)
set(_accepted_runtime_tail "${_submitted_observer_block}")
foreach(_accepted_runtime_token IN ITEMS
        "observe_stage17_submitted_actions("
        "Stage11BValidationScenario::rebound_attack"
        "injected_frame == 28U"
        "old_attack_checked = true;"
        "old_attack_count +="
        "submitted_actions.combat[0] ? 1U : 0U;"
        "injected_frame == 29U"
        "new_attack_count +="
        "submitted_actions.combat[0] ? 1U : 0U;")
    string(FIND "${_accepted_runtime_tail}" "${_accepted_runtime_token}"
        _accepted_runtime_index)
    if(_accepted_runtime_index EQUAL -1)
        set(_accepted_runtime_valid FALSE)
        break()
    endif()
    math(EXPR _accepted_runtime_after "${_accepted_runtime_index} + 1")
    string(SUBSTRING "${_accepted_runtime_tail}" ${_accepted_runtime_after}
        -1 _accepted_runtime_tail)
    set(_accepted_runtime_valid TRUE)
endforeach()
string(REGEX MATCHALL
    "submitted_actions[.]combat\\[0\\][ \t\r\n]*[?][ \t\r\n]*1U[ \t\r\n]*:[ \t\r\n]*0U"
    _accepted_attack_counts "${_submitted_observer_block}")
list(LENGTH _accepted_attack_counts _accepted_attack_count)
if(_accepted_actions EQUAL -1 OR _accepted_observer EQUAL -1
        OR NOT _accepted_actions LESS _accepted_observer
        OR NOT _accepted_runtime_valid OR NOT _accepted_attack_count EQUAL 2)
    message(FATAL_ERROR "Stage11B evidence guard requires accepted queue_action evidence")
endif()
set(_pause_transition_call_token
    "validation_runtime->observe_pause_transition(pause_was_open,pause_open);")
foreach(_host_surface IN ITEMS _host_active _host_lexical)
    string(REGEX REPLACE "[ \t\r\n]+" "" _host_surface_compact
        "${${_host_surface}}")
    stage11b_count_token("${_host_surface_compact}"
        "${_pause_transition_call_token}" _pause_transition_global_count)
    if(NOT _pause_transition_global_count EQUAL 1)
        message(FATAL_ERROR
            "T7C-M07 pause transition must have one active and lexical Host call")
    endif()
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _host_run_compact
    "${_host_run_block}")
string(FIND "${_host_run_compact}"
    "constboolpause_open=pause_menu.screen!=PauseScreen::closed;"
    _m07_pause_open_position)
string(FIND "${_host_run_compact}" "${_pause_transition_call_token}"
    _m07_pause_transition_position)
string(FIND "${_host_run_compact}"
    "constfloatframe_seconds=GetFrameTime();" _m07_frame_time_position)
string(FIND "${_host_run_compact}" "HostFrameGateResulthost_gate{};"
    _m07_gate_declaration_position)
string(FIND "${_host_run_compact}" "host_gate=gate_host_frame("
    _m07_gate_call_position)
string(FIND "${_host_run_compact}" "runtime.fixed_tick(step_movement,"
    _m07_fixed_tick_position)
if(_m07_pause_open_position EQUAL -1
        OR _m07_pause_transition_position EQUAL -1
        OR _m07_frame_time_position EQUAL -1
        OR _m07_gate_declaration_position EQUAL -1
        OR _m07_gate_call_position EQUAL -1
        OR _m07_fixed_tick_position EQUAL -1
        OR NOT _m07_pause_open_position LESS _m07_pause_transition_position
        OR NOT _m07_pause_transition_position LESS _m07_frame_time_position
        OR NOT _m07_frame_time_position LESS _m07_gate_declaration_position
        OR NOT _m07_gate_declaration_position LESS _m07_gate_call_position
        OR NOT _m07_gate_call_position LESS _m07_fixed_tick_position)
    message(FATAL_ERROR
        "T7C-M07 pre-tick pause observation must follow pause settlement and precede GetFrameTime, frame gate, and fixed tick")
endif()

set(_pause_cjk_assignment
    "pause_cjk_ready=pause_menu_renderer.has_cjk_font();")
stage11b_count_token("${_host_run_compact}" "boolpause_cjk_ready=false;"
    _pause_cjk_initialization_count)
stage11b_count_token("${_host_run_compact}" "${_pause_cjk_assignment}"
    _pause_cjk_assignment_count)
set(_pause_cjk_contract
    "boolpause_cjk_ready=false;if(!config.stage12_material_background_only&&!config.stage12_material_icons_only&&pause_menu.screen!=PauseScreen::closed){pause_menu_renderer.draw(pause_menu,renderer.material_pack());pause_cjk_ready=pause_menu_renderer.has_cjk_font();}constPresentationDecisiondecision=validation_runtime->observe_presented_frame(current,pause_menu,pause_cjk_ready);")
string(FIND "${_host_run_compact}" "${_pause_cjk_contract}"
    _pause_cjk_contract_position)
if(NOT _pause_cjk_initialization_count EQUAL 1
        OR NOT _pause_cjk_assignment_count EQUAL 1
        OR _pause_cjk_contract_position EQUAL -1)
    message(FATAL_ERROR
        "T7C pause CJK readiness must start false and become renderer-backed only after the real pause draw")
endif()

stage11b_count_token("${_host_run_block}"
    "validation_runtime->observe_presented_frame("
    _presented_observer_count)
stage11b_count_token("${_host_run_block}"
    "present_frame_and_maybe_capture(" _present_call_count)
stage11b_count_token("${_host_run_block}"
    "validation_runtime->observe_capture_result("
    _capture_result_call_count)
string(FIND "${_host_run_block}"
    "validation_runtime->observe_presented_frame(" _presented_observer_call)
if(NOT _presented_observer_count EQUAL 1
        OR NOT _present_call_count EQUAL 2
        OR NOT _capture_result_call_count EQUAL 1
        OR _presented_observer_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11B evidence guard requires the real ordinary presentation and post-Present capture result")
endif()
string(SUBSTRING "${_host_run_block}" ${_presented_observer_call} -1
    _presented_observer_tail)
string(FIND "${_presented_observer_tail}"
    "present_frame_and_maybe_capture(" _paused_capture_present_call)
string(FIND "${_presented_observer_tail}"
    "validation_runtime->observe_capture_result(" _capture_result_call)
if(_paused_capture_present_call EQUAL -1
        OR _capture_result_call EQUAL -1
        OR NOT _paused_capture_present_call LESS _capture_result_call)
    message(FATAL_ERROR
        "Stage11B evidence guard requires the real ordinary presentation and post-Present capture result")
endif()
foreach(_removed_host_stage11b IN ITEMS
        "stage11b_validation_state"
        "HostValidationStateAccess::stage11b("
        "host_validation::write_stage11b_validation_summary(")
    string(FIND "${_host_text}" "${_removed_host_stage11b}"
        _removed_host_stage11b_index)
    if(NOT _removed_host_stage11b_index EQUAL -1)
        message(FATAL_ERROR
            "Stage11B evidence guard rejects direct Host validation ownership: ${_removed_host_stage11b}")
    endif()
endforeach()

foreach(_ordered IN ITEMS
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "HostFrameInput frame_input = map_host_frame_input("
        "HostFrameGateResult host_gate"
        "submit_frame_actions(*session, frame_input)")
    string(FIND "${_host_run_block}" "${_ordered}" _order_index)
    if(_order_index EQUAL -1)
        message(FATAL_ERROR "Stage11B evidence guard missing production pipeline step: ${_ordered}")
    endif()
    if(DEFINED _previous_order_index AND _order_index LESS _previous_order_index)
        message(FATAL_ERROR "Stage11B evidence guard rejected out-of-order physical input pipeline")
    endif()
    set(_previous_order_index ${_order_index})
endforeach()
