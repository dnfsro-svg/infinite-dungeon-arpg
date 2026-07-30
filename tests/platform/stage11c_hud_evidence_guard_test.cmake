if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
set(_host_validation_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp")
if(DEFINED HOST_VALIDATION_RUNTIME_OVERRIDE)
    set(_host_validation_runtime "${HOST_VALIDATION_RUNTIME_OVERRIDE}")
endif()
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_input_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.hpp")
set(_input_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.cpp")
if(DEFINED INPUT_OVERRIDE)
    set(_input_source "${INPUT_OVERRIDE}")
endif()
set(_stage_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.hpp")
if(DEFINED STAGE11C_HEADER_OVERRIDE)
    set(_stage_header "${STAGE11C_HEADER_OVERRIDE}")
endif()
set(_stage_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.cpp")
if(DEFINED STAGE11C_SOURCE_OVERRIDE)
    set(_stage_source "${STAGE11C_SOURCE_OVERRIDE}")
endif()
set(_formal "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_game_validation.cpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
endif()
set(_validator "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_validator.ps1")
set(_bad "${SOURCE_ROOT}/tests/platform/stage11c_hud_bad_formal_input.txt")
foreach(_file IN ITEMS "${_host}" "${_host_validation_runtime}" "${_header}"
        "${_input_header}" "${_input_source}"
        "${_stage_header}" "${_stage_source}" "${_formal}" "${_validator}" "${_bad}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11C HUD evidence target is missing: ${_file}")
    endif()
endforeach()
file(READ "${_host}" _host_text)
file(READ "${_host_validation_runtime}" _host_validation_runtime_text)

function(stage11c_count_raw_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${SOURCE}" _source_length)
    set(_scan 0)
    set(_count 0)
    while(_scan LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "${TOKEN}" _relative)
        if(_relative EQUAL -1)
            break()
        endif()
        math(EXPR _position "${_scan} + ${_relative}")
        math(EXPR _count "${_count} + 1")
        string(LENGTH "${TOKEN}" _token_length)
        math(EXPR _scan "${_position} + ${_token_length}")
    endwhile()
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(stage11c_unconditional_cpp_surface SOURCE OUT_SURFACE)
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
                    "Stage11C evidence conditional surface is unbalanced")
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
            "Stage11C evidence conditional surface is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

file(READ "${_header}" _header_text)
file(READ "${_input_header}" _input_header_text)
file(READ "${_input_source}" _input_source_text)
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_stage_source}" _stage_source_text)
file(READ "${_formal}" _formal_text)
file(READ "${_validator}" _validator_text)
set(_combined "${_header_text}\n${_input_header_text}\n${_input_source_text}\n${_stage_header_text}\n${_stage_source_text}\n${_host_validation_runtime_text}\n${_host_text}\n${_formal_text}")

foreach(_required IN ITEMS
        "Stage11CHudValidationScenario"
        "normal_combat" "low_health_status" "cleared_exit"
        "abyss_abandon" "level_up_points" "debug_overlay"
        "platform::run_raylib_host(config)"
        "std::system(command.c_str())"
        "stage11c_production_snapshot_hash"
        "production_snapshot_hash"
        "present_frame_and_maybe_capture"
        "font_ready" "status_tags" "notice_kinds" "notice_texts" "navigation_values")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard missing required production token: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "HudViewModel direct_model"
        "HudNoticeState injected_notice"
        "CombatSnapshot injected_combat"
        "TestAccess"
        "session.queue_action"
        "queue_logical_action"
        "state.progression ="
        "result=pass"
        "LoadImageFromScreen()")
    string(FIND "${_formal_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        if(_forbidden STREQUAL "HudViewModel direct_model")
            set(_reason "direct ViewModel assignment")
        elseif(_forbidden STREQUAL "state.progression =")
            set(_reason "bypassed Session progression")
        elseif(_forbidden STREQUAL "result=pass")
            set(_reason "fake result pass")
        elseif(_forbidden STREQUAL "session.queue_action" OR _forbidden STREQUAL "queue_logical_action")
            set(_reason "logical action queue")
        else()
            set(_reason "forbidden formal injection ${_forbidden}")
        endif()
        message(FATAL_ERROR "Stage11C evidence guard rejected ${_reason}")
    endif()
endforeach()

foreach(_required_input IN ITEMS
        "void inject_validation_action("
        "void inject_validation_movement("
        "settings::binding_for(settings_data, action)"
        "snapshot.down[index] = true;"
        "if (pressed) snapshot.pressed[index] = true;")
    string(FIND "${_input_source_text}" "${_required_input}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected skipped stable binding: ${_required_input}")
    endif()
endforeach()
set(_physical_input_forbidden
    ".queue_action(" "request_descent(" "request_passive_"
    "TestAccess" "HudViewModel direct_model")
foreach(_forbidden IN LISTS _physical_input_forbidden)
    string(FIND "${_input_source_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected non-physical shared input: ${_forbidden}")
    endif()
endforeach()

evidence_extract_cpp_function_block("${_stage_source_text}"
    "PhysicalKeySnapshot inject_stage11c_physical_edges(" _stage11c_driver)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "std::uint64_t stage11c_production_snapshot_hash(" _stage11c_hash_function)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "bool stage11c_hud_validation_reached(" _stage11c_reached_function)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "void write_stage11c_hud_validation_summary(" _stage11c_summary_function)
function(stage11c_require_function_tokens LABEL SURFACE)
    foreach(_token IN LISTS ARGN)
        string(FIND "${SURFACE}" "${_token}" _token_index)
        if(_token_index EQUAL -1)
            message(FATAL_ERROR "Stage11C evidence guard missing ${LABEL} token: ${_token}")
        endif()
    endforeach()
endfunction()
function(stage11c_code_brace_depth SURFACE POSITION OUT_DEPTH)
    if(POSITION EQUAL 0)
        set(${OUT_DEPTH} 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    string(REGEX REPLACE "[^{}]" "" _braces "${_prefix}")
    string(LENGTH "${_braces}" _brace_length)
    set(_depth 0)
    if(_brace_length GREATER 0)
        math(EXPR _brace_last "${_brace_length} - 1")
        foreach(_brace_index RANGE 0 ${_brace_last})
            string(SUBSTRING "${_braces}" ${_brace_index} 1 _brace)
            if(_brace STREQUAL "{")
                math(EXPR _depth "${_depth} + 1")
            else()
                math(EXPR _depth "${_depth} - 1")
            endif()
        endforeach()
    endif()
    set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
endfunction()

function(stage11c_require_unique_hash_core_token SURFACE TOKEN OUT_POSITION)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    math(EXPR _after "${_position} + 1")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _tail)
    string(FIND "${_tail}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    set(${OUT_POSITION} ${_position} PARENT_SCOPE)
endfunction()

function(stage11c_matching_brace_position SURFACE OPEN_POSITION OUT_POSITION)
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
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    set(${OUT_POSITION} ${_close} PARENT_SCOPE)
endfunction()

function(stage11c_mask_non_direct_executable_scopes SOURCE OUT_SOURCE)
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
            stage11c_matching_brace_position("${SOURCE}" ${_open_index}
                _scope_end)
        endif()
        math(EXPR _copy_cursor "${_scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${_masked}" PARENT_SCOPE)
endfunction()

function(stage11c_require_hash_core SURFACE)
    stage11c_require_unique_hash_core_token("${SURFACE}"
        "std::uint64_t hash = 1469598103934665603ULL;" _initialization)
    stage11c_require_unique_hash_core_token("${SURFACE}" "hash ^= value;" _xor)
    stage11c_require_unique_hash_core_token("${SURFACE}"
        "hash *= 1099511628211ULL;" _prime)
    set(_mix_signature "const auto mix = [&hash](std::uint64_t value) noexcept {")
    stage11c_require_unique_hash_core_token("${SURFACE}" "${_mix_signature}"
        _mix_begin)
    stage11c_require_unique_hash_core_token("${SURFACE}" "return hash;"
        _final_return)
    string(SUBSTRING "${SURFACE}" ${_mix_begin} -1 _mix_tail)
    string(FIND "${_mix_tail}" "{" _mix_open_relative)
    math(EXPR _mix_open "${_mix_begin} + ${_mix_open_relative}")
    stage11c_matching_brace_position("${SURFACE}" ${_mix_open} _mix_close)
    stage11c_code_brace_depth("${SURFACE}" ${_initialization} _initialization_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_mix_open} _mix_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_xor} _xor_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_prime} _prime_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_final_return} _return_depth)
    if(NOT _initialization LESS _mix_begin OR NOT _mix_begin LESS _xor
            OR NOT _xor LESS _prime OR NOT _prime LESS _mix_close
            OR NOT _initialization_depth EQUAL 1 OR NOT _mix_depth EQUAL 1
            OR NOT _xor_depth EQUAL 2 OR NOT _prime_depth EQUAL 2
            OR NOT _return_depth EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    set(_previous_chain_position -1)
    foreach(_token IN ITEMS
            "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
            "mix(snapshot.commit_generation);" "mix(snapshot.room_index);"
            "mix(snapshot.room_seed);" "mix(snapshot.depth);"
            "mix(snapshot.floor_room_index);"
            "mix(static_cast<std::uint64_t>(snapshot.phase));"
            "mix(snapshot.is_abyss ? 1U : 0U);"
            "mix(snapshot.abyss_exit_confirmation_armed ? 1U : 0U);"
            "mix(snapshot.remaining_targets);" "mix(snapshot.progression.level);"
            "mix(snapshot.progression.experience);"
            "mix(snapshot.progression.unspent_passive_points);")
        stage11c_require_unique_hash_core_token("${SURFACE}" "${_token}"
            _chain_position)
        stage11c_code_brace_depth("${SURFACE}" ${_chain_position} _chain_depth)
        if(NOT _chain_depth EQUAL 1
                OR NOT _previous_chain_position LESS _chain_position)
            message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
        endif()
        set(_previous_chain_position ${_chain_position})
    endforeach()
endfunction()

function(stage11c_return_positions SURFACE OUT_POSITIONS)
    string(LENGTH "${SURFACE}" _length)
    set(_scan 0)
    set(_positions)
    while(_scan LESS _length)
        string(SUBSTRING "${SURFACE}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "return" _relative)
        if(_relative EQUAL -1)
            break()
        endif()
        math(EXPR _position "${_scan} + ${_relative}")
        set(_code_token TRUE)
        if(_position GREATER 0)
            math(EXPR _previous_position "${_position} - 1")
            string(SUBSTRING "${SURFACE}" ${_previous_position} 1 _previous)
            if(_previous MATCHES "[A-Za-z0-9_]")
                set(_code_token FALSE)
            endif()
        endif()
        math(EXPR _after_return "${_position} + 6")
        if(_after_return LESS _length)
            string(SUBSTRING "${SURFACE}" ${_after_return} 1 _next)
            if(_code_token AND NOT _next MATCHES "[A-Za-z0-9_]")
                list(APPEND _positions ${_position})
            endif()
        endif()
        math(EXPR _scan "${_position} + 6")
    endwhile()
    set(${OUT_POSITIONS} "${_positions}" PARENT_SCOPE)
endfunction()

function(stage11c_require_single_final_return LABEL SURFACE EXPECTED)
    stage11c_return_positions("${SURFACE}" _returns)
    list(LENGTH _returns _return_count)
    if(NOT _return_count EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
    endif()
    list(GET _returns 0 _return_position)
    string(LENGTH "${EXPECTED}" _expected_length)
    string(SUBSTRING "${SURFACE}" ${_return_position} ${_expected_length}
        _actual_return)
    if(NOT _actual_return STREQUAL "${EXPECTED}")
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
    endif()
    foreach(_required IN LISTS ARGN)
        string(FIND "${SURFACE}" "${_required}" _required_position)
        if(_required_position EQUAL -1 OR _return_position LESS _required_position)
            message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
        endif()
    endforeach()
endfunction()

function(stage11c_require_switch_return_inventory SURFACE)
    string(FIND "${SURFACE}" "switch (scenario) {" _switch_position)
    if(_switch_position EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    stage11c_code_brace_depth("${SURFACE}" ${_switch_position} _switch_depth)
    if(NOT _switch_depth EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    stage11c_return_positions("${SURFACE}" _returns)
    list(LENGTH _returns _return_count)
    if(NOT _return_count EQUAL 9)
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    list(GET _returns 0 _pre_switch_return)
    string(SUBSTRING "${SURFACE}" ${_pre_switch_return} 13 _pre_switch_value)
    if(NOT _pre_switch_value STREQUAL "return false;")
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    math(EXPR _last_return_index "${_return_count} - 1")
    foreach(_return_index RANGE 1 ${_last_return_index})
        list(GET _returns ${_return_index} _return_position)
        if(_return_position LESS _switch_position)
            message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
        endif()
    endforeach()
endfunction()

function(stage11c_reject_summary_returns SURFACE)
    stage11c_return_positions("${SURFACE}" _returns)
    if(_returns)
        message(FATAL_ERROR "Stage11C evidence guard rejected summary return inventory")
    endif()
endfunction()

function(stage11c_reject_hash_direct_overwrite SURFACE)
    string(FIND "${SURFACE}" "std::uint64_t hash =" _initialization)
    if(_initialization EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash direct overwrite")
    endif()
    string(SUBSTRING "${SURFACE}" ${_initialization} -1 _initialization_tail)
    string(FIND "${_initialization_tail}" ";" _initialization_end_relative)
    if(_initialization_end_relative EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash direct overwrite")
    endif()
    math(EXPR _initialization_end
        "${_initialization} + ${_initialization_end_relative}")
    string(LENGTH "${SURFACE}" _length)
    set(_scan 0)
    while(_scan LESS _length)
        string(SUBSTRING "${SURFACE}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "hash" _relative)
        if(_relative EQUAL -1)
            break()
        endif()
        math(EXPR _position "${_scan} + ${_relative}")
        math(EXPR _after_hash "${_position} + 4")
        string(SUBSTRING "${SURFACE}" ${_after_hash} -1 _after_hash_text)
        string(REGEX MATCH "^[ \t\r\n]*=" _direct_assignment
            "${_after_hash_text}")
        string(REGEX MATCH "^[ \t\r\n]*==" _comparison
            "${_after_hash_text}")
        if(_direct_assignment AND NOT _comparison
                AND _position GREATER _initialization_end)
            message(FATAL_ERROR
                "Stage11C evidence guard rejected production hash direct overwrite")
        endif()
        math(EXPR _scan "${_after_hash}")
    endwhile()
endfunction()
stage11c_require_function_tokens("physical driver" "${_stage11c_driver}"
    "++state.injected_frames;" "nearest_living_monster(combat_snapshot)"
    "inject_validation_movement(" "inject_validation_action(")
stage11c_require_function_tokens("production hash" "${_stage11c_hash_function}"
    "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
    "mix(snapshot.progression.unspent_passive_points);"
    "mix(monster.spawn_ordinal);" "return hash;")
stage11c_require_hash_core("${_stage11c_hash_function}")
stage11c_require_function_tokens("reached predicate" "${_stage11c_reached_function}"
    "case Scenario::low_health_status:" "player.hp > 0"
    "case Scenario::cleared_exit:" "case Scenario::abyss_abandon:"
    "case Scenario::level_up_points:" "case Scenario::debug_overlay:")
stage11c_require_function_tokens("summary" "${_stage11c_summary_function}"
    "state.model.player" "player.status_tag_count"
    "state.notices.primary.kind" "state.notices.secondary.kind"
    "state.model.navigation.depth" "state.cjk_font_ready"
    "state.production_snapshot_hash" "stage11c_validation_result")
stage11c_require_single_final_return("physical driver" "${_stage11c_driver}"
    "return snapshot;"
    "++state.injected_frames;" "nearest_living_monster(combat_snapshot)"
    "inject_validation_movement(" "inject_validation_action(")
stage11c_require_single_final_return("production hash"
    "${_stage11c_hash_function}" "return hash;"
    "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
    "mix(snapshot.progression.unspent_passive_points);"
    "mix(monster.spawn_ordinal);")
stage11c_require_switch_return_inventory("${_stage11c_reached_function}")
stage11c_reject_summary_returns("${_stage11c_summary_function}")
stage11c_reject_hash_direct_overwrite("${_stage11c_hash_function}")
foreach(_forbidden IN LISTS _physical_input_forbidden)
    string(FIND "${_stage11c_driver}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected non-physical scenario driver: ${_forbidden}")
    endif()
endforeach()

stage11c_unconditional_cpp_surface("${_host_text}" _host_code)
stage11c_unconditional_cpp_surface("${_host_validation_runtime_text}"
    _host_validation_runtime_code)

function(stage11c_extract_runtime_method LABEL SIGNATURE OUT_METHOD)
    stage11c_count_raw_token("${_host_validation_runtime_code}"
        "${SIGNATURE}" _definition_count)
    if(NOT _definition_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11C evidence guard requires one active runtime ${LABEL} owner")
    endif()
    string(FIND "${_host_validation_runtime_code}" "${SIGNATURE}"
        _definition_position)
    stage11c_code_brace_depth("${_host_validation_runtime_code}"
        ${_definition_position} _definition_depth)
    if(NOT _definition_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected runtime ${LABEL} owner scope")
    endif()
    evidence_extract_cpp_function_block("${_host_validation_runtime_code}"
        "${SIGNATURE}" _method)
    set(${OUT_METHOD} "${_method}" PARENT_SCOPE)
endfunction()

stage11c_extract_runtime_method("HUD snapshot preparation"
    "void HostValidationRuntime::prepare_hud_snapshot("
    _stage11c_prepare_hud)
stage11c_extract_runtime_method("T7C-HUD-observation"
    "void HostValidationRuntime::observe_hud("
    _stage11c_observe_hud)
stage11c_extract_runtime_method("presented-frame completion"
    "PresentationDecision HostValidationRuntime::observe_presented_frame("
    _stage11c_observe_presented)
stage11c_extract_runtime_method("capture result"
    "void HostValidationRuntime::observe_capture_result("
    _stage11c_observe_capture)
stage11c_extract_runtime_method("summary"
    "void HostValidationRuntime::write_summaries("
    _stage11c_write_summaries)

foreach(_method IN ITEMS
        _stage11c_prepare_hud _stage11c_observe_hud
        _stage11c_observe_presented _stage11c_observe_capture
        _stage11c_write_summaries)
    stage11c_mask_non_direct_executable_scopes("${${_method}}"
        _stage11c_direct_method)
    string(REGEX REPLACE "[ \t\r\n]+" "" ${_method}_normalized
        "${_stage11c_direct_method}")
endforeach()

function(stage11c_require_runtime_binding LABEL SURFACE REGEX)
    set(_remaining "${SURFACE}")
    set(_match_count 0)
    while(TRUE)
        string(REGEX MATCH "${REGEX}" _match "${_remaining}")
        if("${_match}" STREQUAL "")
            break()
        endif()
        string(FIND "${_remaining}" "${_match}" _match_begin)
        string(LENGTH "${_match}" _match_length)
        if(_match_begin LESS 0 OR _match_length EQUAL 0)
            message(FATAL_ERROR
                "Stage11C evidence guard could not advance ${LABEL} scan")
        endif()
        math(EXPR _next_begin "${_match_begin} + ${_match_length}")
        string(SUBSTRING "${_remaining}" ${_next_begin} -1 _remaining)
        math(EXPR _match_count "${_match_count} + 1")
    endwhile()
    if(NOT _match_count EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
    endif()
endfunction()

stage11c_require_runtime_binding("HUD fixture ownership"
    "${_stage11c_prepare_hud_normalized}"
    "Stage11CHudValidationScenario::low_health_status")
stage11c_require_runtime_binding("HUD fixture max-barrier assignment"
    "${_stage11c_prepare_hud_normalized}" "[.]max_barrier=1000;")
stage11c_require_runtime_binding("HUD fixture barrier assignment"
    "${_stage11c_prepare_hud_normalized}" "[.]barrier=625;")
stage11c_require_runtime_binding("authoritative HUD model capture"
    "${_stage11c_observe_hud_normalized}" "[.]model=model;")
stage11c_require_runtime_binding("authoritative HUD notices capture"
    "${_stage11c_observe_hud_normalized}" "[.]notices=notices;")
stage11c_require_runtime_binding("T7C-M10 layout binding"
    "${_stage11c_observe_hud_normalized}"
    "[.]layout=make_hud_layout[(]screen_width,screen_height,true[)];")
stage11c_require_runtime_binding("T7C-M10 hash binding"
    "${_stage11c_observe_hud_normalized}"
    "[.]production_snapshot_hash=host_validation::stage11c_production_snapshot_hash[(]snapshot[)];")
stage11c_require_runtime_binding("per-frame HUD debug synchronization"
    "${_stage11c_observe_hud_normalized}" "[.]debug_visible=draw_debug;")
stage11c_require_runtime_binding("HUD target-frame increment"
    "${_stage11c_observe_hud_normalized}" "[+][+][^;]*target_presented_frames;")
stage11c_require_runtime_binding("HUD target-frame reset"
    "${_stage11c_observe_hud_normalized}" "[.]target_presented_frames=0U;")
stage11c_require_runtime_binding("four-frame HUD completion"
    "${_stage11c_observe_presented_normalized}"
    "[.]stage11c[.]target_presented_frames>=4U")
stage11c_require_runtime_binding("Stage11C completion publication"
    "${_stage11c_observe_presented_normalized}"
    "validation_complete=.*stage11c_reached")
stage11c_require_runtime_binding("T7C-Stage11C-pending"
    "${_stage11c_observe_presented_normalized}"
    "impl_->[A-Za-z_][A-Za-z0-9_]*=stage11c_reached;")
string(REGEX MATCH
    "impl_->([A-Za-z_][A-Za-z0-9_]*)=stage11c_reached;"
    _stage11c_pending_match "${_stage11c_observe_presented_normalized}")
if("${_stage11c_pending_match}" STREQUAL "")
    message(FATAL_ERROR
        "Stage11C evidence guard rejected pending capture publication")
endif()
set(_stage11c_pending_field "${CMAKE_MATCH_1}")
stage11c_require_runtime_binding("T7C-Stage11C-capture"
    "${_stage11c_observe_capture_normalized}"
    "if[(]impl_->${_stage11c_pending_field}[)][{]impl_->states[.]stage11c[.]captured=true;[}]")
stage11c_require_runtime_binding("T7C-M25 Stage11C summary ownership"
    "${_stage11c_write_summaries_normalized}"
    "write_stage11c_hud_validation_summary[(]")

foreach(_forbidden_global IN ITEMS "GetScreenWidth(" "GetScreenHeight(")
    string(FIND "${_stage11c_observe_hud}" "${_forbidden_global}" _global_found)
    if(NOT _global_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected authoritative HUD layout computation")
    endif()
endforeach()

string(FIND "${_stage11c_summary_function}"
    "const bool stage11c_validation_result = state.captured\n            && state.cjk_font_ready && state.production_snapshot_hash != 0U;"
    _stage11c_summary_gate)
if(_stage11c_summary_gate EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake summary state")
endif()

evidence_extract_cpp_function_block("${_host_code}"
    "HostExitCode run_raylib_host(" _host_run_code)
stage11c_mask_non_direct_executable_scopes("${_host_run_code}"
    _host_run_direct)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_run_normalized
    "${_host_run_direct}")
set(_ordinary_hud_anchor
    "constHudPresentedFramehud_presented_frame=current.death.has_value()")
string(FIND "${_host_run_normalized}" "${_ordinary_hud_anchor}"
    _ordinary_hud_begin)
if(_ordinary_hud_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11C evidence guard cannot bind ordinary HUD presentation")
endif()
string(SUBSTRING "${_host_run_normalized}" ${_ordinary_hud_begin} -1
    _ordinary_hud_surface)

stage11c_count_raw_token("${_ordinary_hud_surface}"
    "validation_runtime->observe_hud(" _host_hud_arrow_count)
stage11c_count_raw_token("${_ordinary_hud_surface}"
    "validation_runtime.get()->observe_hud(" _host_hud_get_count)
math(EXPR _host_hud_observer_count
    "${_host_hud_arrow_count} + ${_host_hud_get_count}")
if(NOT _host_hud_observer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11C evidence guard requires one direct ordinary-frame HUD observer")
endif()
set(_host_hud_observer_position -1)
foreach(_host_hud_accessor IN ITEMS
        "validation_runtime->" "validation_runtime.get()->")
    string(FIND "${_ordinary_hud_surface}"
        "${_host_hud_accessor}observe_hud(" _candidate_hud_observer_position)
    if(NOT _candidate_hud_observer_position EQUAL -1)
        set(_host_hud_observer_position ${_candidate_hud_observer_position})
    endif()
endforeach()
string(SUBSTRING "${_ordinary_hud_surface}"
    ${_host_hud_observer_position} -1 _host_hud_observer_tail)
string(FIND "${_host_hud_observer_tail}" ";" _host_hud_observer_end)
if(_host_hud_observer_end EQUAL -1)
    message(FATAL_ERROR "T7C-M10: Stage11C HUD observer call is incomplete")
endif()
math(EXPR _host_hud_observer_length "${_host_hud_observer_end} + 1")
string(SUBSTRING "${_host_hud_observer_tail}" 0
    ${_host_hud_observer_length} _host_hud_observer_call)
string(FIND "${_host_hud_observer_call}"
    "observe_hud(current,renderer.hud_model()," _real_hud_model_argument)
if(_real_hud_model_argument EQUAL -1)
    message(FATAL_ERROR
        "T7C-M10: Stage11C evidence guard rejected fabricated HUD model argument")
endif()
string(FIND "${_host_hud_observer_call}"
    "renderer.hud_notice_view(),draw_debug,GetScreenWidth(),GetScreenHeight());"
    _real_hud_notices_argument)
if(_real_hud_notices_argument EQUAL -1)
    message(FATAL_ERROR
        "T7C-M10: Stage11C evidence guard rejected fabricated HUD notices argument")
endif()

string(FIND "${_ordinary_hud_surface}"
    "renderer.observe_presented_hud_frame(hud_presented_frame,"
    _renderer_hud_position)
string(FIND "${_ordinary_hud_surface}" "BeginDrawing();"
    _begin_drawing_position)
if(_renderer_hud_position EQUAL -1 OR _begin_drawing_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11C evidence guard cannot bind HUD observer presentation order")
elseif(NOT _renderer_hud_position LESS _host_hud_observer_position)
    message(FATAL_ERROR "T7C-M08: HUD observer must follow renderer HUD")
elseif(NOT _host_hud_observer_position LESS _begin_drawing_position)
    message(FATAL_ERROR "T7C-M09: HUD observer must precede BeginDrawing")
endif()

unset(_previous_prepare_order)
foreach(_ordered IN ITEMS
        "audio.update("
        "validation_runtime->prepare_hud_snapshot(current);"
        "presented_snapshot=current;")
    string(FIND "${_host_run_normalized}" "${_ordered}" _ordered_position)
    if(_ordered_position EQUAL -1
            OR (DEFINED _previous_prepare_order
                AND _ordered_position LESS _previous_prepare_order))
        message(FATAL_ERROR
            "Stage11C evidence guard rejected HUD snapshot preparation order")
    endif()
    set(_previous_prepare_order ${_ordered_position})
endforeach()

string(FIND "${_host_run_normalized}"
    "validation_runtime->set_render_readiness(hud_resources_ready,"
    _real_font_readiness)
if(_real_font_readiness EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake font-ready")
endif()
string(REGEX REPLACE "[ \t\r\n]+" ""
    _host_validation_runtime_normalized
    "${_host_validation_runtime_code}")
stage11c_require_runtime_binding("font-ready ownership"
    "${_host_validation_runtime_normalized}"
    "[.]stage11c[.]cjk_font_ready=cjk_font_ready;")

string(REGEX MATCHALL "validation_runtime->observe_capture_result[(]"
    _capture_callbacks "${_ordinary_hud_surface}")
list(LENGTH _capture_callbacks _capture_callback_count)
if(NOT _capture_callback_count EQUAL 1)
    message(FATAL_ERROR
        "T7C-M20: Stage11C requires one ordinary-frame capture-result callback")
endif()
string(FIND "${_ordinary_hud_surface}" "present_frame_and_maybe_capture("
    _present_call)
string(FIND "${_ordinary_hud_surface}"
    "validation_runtime->observe_capture_result(" _capture_callback)
if(_present_call EQUAL -1 OR _capture_callback EQUAL -1
        OR NOT _present_call LESS _capture_callback)
    message(FATAL_ERROR
        "Stage11C evidence guard rejected post-present capture-result order")
endif()

string(REGEX MATCHALL "validation_runtime->write_summaries[(]"
    _summary_calls "${_host_run_normalized}")
list(LENGTH _summary_calls _summary_call_count)
if(NOT _summary_call_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11C evidence guard requires one facade summary call")
endif()
string(FIND "${_host_run_normalized}"
    "validation_runtime->write_summaries(runtime.clean_shutdown_state(),pause_menu);"
    _summary_call)
string(FIND "${_host_run_normalized}" "audio.shutdown();" _audio_shutdown)
if(_summary_call EQUAL -1 OR _audio_shutdown EQUAL -1
        OR NOT _summary_call LESS _audio_shutdown)
    message(FATAL_ERROR
        "Stage11C evidence guard rejected facade summary binding/order")
endif()

foreach(_forbidden_host_owner IN ITEMS
        "stage11c_validation_state"
        "Stage11CHudValidationState"
        "host_validation::stage11c_hud_validation_reached("
        "host_validation::stage11c_production_snapshot_hash("
        "host_validation::write_stage11c_hud_validation_summary(")
    string(FIND "${_host_code}" "${_forbidden_host_owner}" _host_owner_found)
    if(NOT _host_owner_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected Host-owned Stage11C state")
    endif()
endforeach()

set(_stage11c_runtime_surface
    "${_stage11c_prepare_hud}\n${_stage11c_observe_hud}")
string(REGEX MATCH
    "(current|previous|snapshot|state)[.]progression[.][A-Za-z_]+[ \t\r\n]*=[^=]"
    _stage11c_progression_bypass "${_stage11c_runtime_surface}")
if(NOT _stage11c_progression_bypass)
    string(REGEX MATCH
        "(current|previous|snapshot|state)[.]progression[ \t\r\n]*=[^=]"
        _stage11c_progression_bypass "${_stage11c_runtime_surface}")
endif()
if(_stage11c_progression_bypass)
    message(FATAL_ERROR
        "Stage11C evidence guard rejected bypassed Session progression")
endif()

unset(_previous_input_order)
foreach(_ordered IN ITEMS
        "constPhysicalKeySnapshotsampled_physical_keys=sample_physical_keys();"
        "validation_runtime->inject_physical_edges("
        "HostFrameInputframe_input=map_host_frame_input("
        "HostFrameGateResulthost_gate"
        "submit_frame_actions(*session,frame_input)")
    string(FIND "${_host_run_normalized}" "${_ordered}" _index)
    if(_index EQUAL -1
            OR (DEFINED _previous_input_order
                AND _index LESS _previous_input_order))
        message(FATAL_ERROR
            "Stage11C evidence guard rejected physical sample-map-pause-submit order")
    endif()
    set(_previous_input_order ${_index})
endforeach()

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "Image image = LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR NOT _present LESS _capture)
    message(FATAL_ERROR "Stage11C evidence guard rejected pre-Present capture")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \t\n]*[(]" _loads "${_host_code}")
list(LENGTH _loads _load_count)
if(NOT _load_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11C evidence guard requires one post-Present capture helper")
endif()


foreach(_required IN ITEMS
        "137,80,78,71,13,10,26,10" "1280" "720" "LastWriteTimeUtc"
        "Measure-HudTextRegion" "HollowBoxes" "notice_texts"
        "non-positive HUD rect" "snapshot hash mismatch"
        "font_ready" "production_snapshot_hash")
    string(FIND "${_validator_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence validator missing check: ${_required}")
    endif()
endforeach()
