if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
get_filename_component(_host_source_dir "${_host}" DIRECTORY)
set(_host_validation_runtime
    "${_host_source_dir}/host_validation_runtime.cpp")
set(_settings_runtime
    "${_host_source_dir}/host_settings_runtime.cpp")
set(_stage_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp")
set(_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_runtime.cpp")
set(_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_game_validation.cpp")
set(_validator "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_validator.ps1")
set(_validator_ast_guard
    "${SOURCE_ROOT}/tests/platform/stage11d_loot_validator_ast_guard.ps1")
if(DEFINED HEADER_OVERRIDE)
    set(_header "${HEADER_OVERRIDE}")
endif()
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
if(DEFINED HOST_VALIDATION_RUNTIME_OVERRIDE)
    set(_host_validation_runtime "${HOST_VALIDATION_RUNTIME_OVERRIDE}")
endif()
if(DEFINED SETTINGS_RUNTIME_OVERRIDE)
    set(_settings_runtime "${SETTINGS_RUNTIME_OVERRIDE}")
endif()
if(DEFINED STAGE11D_HEADER_OVERRIDE)
    set(_stage_header "${STAGE11D_HEADER_OVERRIDE}")
endif()
if(DEFINED RUNTIME_OVERRIDE)
    set(_runtime "${RUNTIME_OVERRIDE}")
endif()
if(DEFINED REPORT_OVERRIDE)
    set(_report "${REPORT_OVERRIDE}")
endif()
if(DEFINED RENDERER_OVERRIDE)
    set(_renderer "${RENDERER_OVERRIDE}")
endif()
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
endif()
if(DEFINED VALIDATOR_OVERRIDE)
    set(_validator "${VALIDATOR_OVERRIDE}")
endif()
foreach(_file IN ITEMS "${_header}" "${_host}" "${_host_validation_runtime}"
        "${_settings_runtime}"
        "${_stage_header}" "${_runtime}" "${_report}" "${_renderer}"
        "${_formal}" "${_validator}" "${_validator_ast_guard}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11D loot evidence input is missing: ${_file}")
    endif()
endforeach()

file(READ "${_header}" _header_text)
file(READ "${_host}" _host_text)
file(READ "${_host_validation_runtime}" _host_validation_runtime_text)
file(READ "${_settings_runtime}" _settings_runtime_text)
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_runtime}" _runtime_text)
file(READ "${_report}" _report_text)
file(READ "${_renderer}" _renderer_text)
file(READ "${_formal}" _formal_text)
file(READ "${_validator}" _validator_text)
set(_combined
    "${_header_text}\n${_stage_header_text}\n${_runtime_text}\n${_report_text}\n${_host_validation_runtime_text}\n${_host_text}\n${_formal_text}")

function(stage11d_count_raw_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${SOURCE}" _source_length)
    string(LENGTH "${TOKEN}" _token_length)
    string(REPLACE "${TOKEN}" "" _without "${SOURCE}")
    string(LENGTH "${_without}" _without_length)
    math(EXPR _removed "${_source_length} - ${_without_length}")
    math(EXPR _count "${_removed} / ${_token_length}")
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(stage11d_fold_cpp_phase2_splices SOURCE OUT_SOURCE)
    string(ASCII 92 _backslash)
    string(ASCII 13 _carriage_return)
    string(ASCII 10 _line_feed)
    set(_folded "${SOURCE}")
    string(REPLACE "${_backslash}${_carriage_return}${_line_feed}" ""
        _folded "${_folded}")
    string(REPLACE "${_backslash}${_line_feed}" ""
        _folded "${_folded}")
    set(${OUT_SOURCE} "${_folded}" PARENT_SCOPE)
endfunction()

# Prove the CMake byte construction for both C++ phase-2 newline forms before
# relying on the cheap complete-Host inventory below.
string(ASCII 92 _stage11d_probe_backslash)
string(ASCII 13 _stage11d_probe_carriage_return)
string(ASCII 10 _stage11d_probe_line_feed)
set(_stage11d_probe_expected
    "validation_runtime->inject_physical_edges(")
set(_stage11d_probe_lf
    "validation_runtime->inject_phy${_stage11d_probe_backslash}${_stage11d_probe_line_feed}sical_edges(")
set(_stage11d_probe_crlf
    "validation_runtime->inject_phy${_stage11d_probe_backslash}${_stage11d_probe_carriage_return}${_stage11d_probe_line_feed}sical_edges(")
stage11d_fold_cpp_phase2_splices("${_stage11d_probe_lf}"
    _stage11d_probe_lf_folded)
stage11d_fold_cpp_phase2_splices("${_stage11d_probe_crlf}"
    _stage11d_probe_crlf_folded)
if(NOT _stage11d_probe_lf_folded STREQUAL _stage11d_probe_expected
        OR NOT _stage11d_probe_crlf_folded STREQUAL
            _stage11d_probe_expected)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot fold C++ phase-2 line splices")
endif()

# Task 7A's facade runtime ownership is unconditional. Fold C++ phase-2
# splices and remove complete conditional regions so an inactive exact leg
# cannot satisfy the Stage11D owner binding.
function(stage11d_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
    if(ARGC GREATER 2)
        set(${ARGV2} "${_logical_source}" PARENT_SCOPE)
    endif()
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
                    "Stage11D facade input conditional is unbalanced")
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
            "Stage11D facade input conditional is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

# Replacing the complete `// marker` with an identifier before sanitizing is
# deliberate: a real line-comment marker becomes code, while a marker hidden
# in a comment/string/raw-string remains non-code and disappears.
function(stage11d_prepare_marker_surface SOURCE PREFIX LABELS OUT_CODE)
    set(_marked "${SOURCE}")
    foreach(_label IN LISTS LABELS)
        foreach(_kind IN ITEMS BEGIN END)
            set(_marker
                "// STAGE11D_LOOT_VALIDATION_SEAM_${_kind} ${_label}")
            stage11d_count_raw_token("${_marked}" "${_marker}" _count)
            if(NOT _count EQUAL 1)
                message(FATAL_ERROR
                    "Stage11D loot evidence guard cannot bind ${PREFIX} ${_label} seam marker")
            endif()
            set(_token "TASK5A_${PREFIX}_${_label}_${_kind}_MARKER")
            string(REPLACE "${_marker}" "${_token}" _marked "${_marked}")
        endforeach()
    endforeach()
    stage11d_unconditional_cpp_surface("${_marked}" _code)
    foreach(_label IN LISTS LABELS)
        foreach(_kind IN ITEMS BEGIN END)
            set(_token "TASK5A_${PREFIX}_${_label}_${_kind}_MARKER")
            stage11d_count_raw_token("${_code}" "${_token}" _count)
            if(NOT _count EQUAL 1)
                message(FATAL_ERROR
                    "Stage11D loot evidence guard cannot bind ${PREFIX} ${_label} seam marker")
            endif()
        endforeach()
    endforeach()
    set(${OUT_CODE} "${_code}" PARENT_SCOPE)
endfunction()

function(stage11d_code_brace_depth SURFACE POSITION OUT_DEPTH)
    if(POSITION EQUAL 0)
        set(${OUT_DEPTH} 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    string(REGEX REPLACE "[^{}]" "" _braces "${_prefix}")
    string(LENGTH "${_braces}" _length)
    set(_depth 0)
    if(_length GREATER 0)
        math(EXPR _last "${_length} - 1")
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

function(stage11d_require_direct_statement_owner SURFACE POSITION ERROR_MESSAGE)
    if(POSITION EQUAL 0)
        set(_prefix "")
    else()
        string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    endif()
    string(FIND "${_prefix}" ";" _semicolon_position REVERSE)
    string(FIND "${_prefix}" "{" _open_brace_position REVERSE)
    string(FIND "${_prefix}" "}" _close_brace_position REVERSE)
    set(_boundary_position ${_semicolon_position})
    if(_open_brace_position GREATER _boundary_position)
        set(_boundary_position ${_open_brace_position})
    endif()
    if(_close_brace_position GREATER _boundary_position)
        set(_boundary_position ${_close_brace_position})
    endif()
    math(EXPR _leader_begin "${_boundary_position} + 1")
    math(EXPR _leader_length "${POSITION} - ${_leader_begin}")
    if(_leader_length GREATER 0)
        string(SUBSTRING "${SURFACE}" ${_leader_begin}
            ${_leader_length} _leader)
    else()
        set(_leader "")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" "" _leader "${_leader}")
    if(NOT _leader STREQUAL "")
        message(FATAL_ERROR "${ERROR_MESSAGE}")
    endif()
endfunction()

function(stage11d_matching_brace_position SURFACE OPEN_POSITION OUT_POSITION)
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
            "Stage11D direct-scope fixture has no closing brace")
    endif()
    set(${OUT_POSITION} ${_close} PARENT_SCOPE)
endfunction()

function(stage11d_extract_direct_braced_statement
        LABEL SURFACE HEADER EXPECTED_DEPTH OUT_BLOCK)
    stage11d_count_raw_token("${SURFACE}" "${HEADER}" _header_count)
    if(NOT _header_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} owner")
    endif()
    string(FIND "${SURFACE}" "${HEADER}" _header_position)
    stage11d_code_brace_depth("${SURFACE}" ${_header_position}
        _header_depth)
    if(NOT _header_depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} owner")
    endif()
    stage11d_require_direct_statement_owner("${SURFACE}" ${_header_position}
        "Stage11D loot evidence guard rejected ${LABEL} owner")

    string(SUBSTRING "${SURFACE}" ${_header_position} -1 _header_tail)
    string(FIND "${_header_tail}" "{" _open_relative)
    string(FIND "${_header_tail}" ";" _semicolon_relative)
    if(_open_relative EQUAL -1
            OR (NOT _semicolon_relative EQUAL -1
                AND _semicolon_relative LESS _open_relative))
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} owner")
    endif()
    math(EXPR _open_position "${_header_position} + ${_open_relative}")
    stage11d_matching_brace_position("${SURFACE}" ${_open_position}
        _close_position)
    math(EXPR _block_length
        "${_close_position} - ${_header_position} + 1")
    string(SUBSTRING "${SURFACE}" ${_header_position} ${_block_length}
        _block)
    set(${OUT_BLOCK} "${_block}" PARENT_SCOPE)
endfunction()

function(stage11d_mask_non_direct_executable_scopes SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" _source_length)
    set(_masked "")
    set(_copy_cursor 0)
    set(_dead_condition
        "(false|0[uUlL]*|![ \t\r\n]*true|1[uUlL]*[ \t\r\n]*==[ \t\r\n]*0[uUlL]*|0[uUlL]*[ \t\r\n]*==[ \t\r\n]*1[uUlL]*)")
    while(_copy_cursor LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_copy_cursor} -1 _tail)
        string(REGEX MATCH
            "\\][ \t\r\n]*(\\([^{};]*\\))?[ \t\r\n]*((mutable|constexpr|consteval|static)[ \t\r\n]*)*(noexcept([ \t\r\n]*\\([^{};]*\\))?[ \t\r\n]*)?(->[^{;]*)?[ \t\r\n]*\\{"
            _lambda_match "${_tail}")
        string(REGEX MATCH
            "(class|struct)[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[^;{}]*\\{"
            _local_type_match "${_tail}")
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
        if(NOT _local_type_match STREQUAL "")
            string(FIND "${_tail}" "${_local_type_match}" _local_relative)
            if(_scope_relative EQUAL -1 OR _local_relative LESS _scope_relative)
                set(_scope_match "${_local_type_match}")
                set(_scope_relative ${_local_relative})
                set(_scope_kind local-type)
            endif()
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
        if(_scope_kind STREQUAL "dead-control"
                OR _scope_kind STREQUAL "local-type")
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
            stage11d_matching_brace_position("${SOURCE}" ${_open_index}
                _scope_end)
        endif()
        math(EXPR _copy_cursor "${_scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${_masked}" PARENT_SCOPE)
endfunction()

if(NOT (DEFINED STAGE11D_TASK7C_ONLY AND STAGE11D_TASK7C_ONLY))
function(stage11d_require_marker_depth SURFACE PREFIX LABEL EXPECTED_DEPTH)
    foreach(_kind IN ITEMS BEGIN END)
        set(_token "TASK5A_${PREFIX}_${LABEL}_${_kind}_MARKER")
        string(FIND "${SURFACE}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR
                "Stage11D loot evidence guard cannot bind ${PREFIX} ${LABEL} seam marker")
        endif()
        stage11d_code_brace_depth("${SURFACE}" ${_position} _depth)
        if(NOT _depth EQUAL EXPECTED_DEPTH)
            message(FATAL_ERROR
                "Stage11D loot evidence guard rejected ${PREFIX} ${LABEL} seam scope")
        endif()
    endforeach()
endfunction()

function(stage11d_extract_marker_region SURFACE PREFIX LABEL OUT_REGION)
    set(_begin "TASK5A_${PREFIX}_${LABEL}_BEGIN_MARKER")
    set(_end "TASK5A_${PREFIX}_${LABEL}_END_MARKER")
    string(FIND "${SURFACE}" "${_begin}" _begin_at)
    string(FIND "${SURFACE}" "${_end}" _end_at)
    if(_begin_at EQUAL -1 OR _end_at EQUAL -1 OR NOT _begin_at LESS _end_at)
        message(FATAL_ERROR
            "Stage11D loot evidence guard cannot isolate ${PREFIX} ${LABEL} seam")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${SURFACE}" ${_begin_at} ${_length} _region)
    set(${OUT_REGION} "${_region}" PARENT_SCOPE)
endfunction()

function(stage11d_extract_raw_seam SOURCE LABEL OUT_REGION)
    set(_begin "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN ${LABEL}")
    set(_end "// STAGE11D_LOOT_VALIDATION_SEAM_END ${LABEL}")
    string(FIND "${SOURCE}" "${_begin}" _begin_at)
    string(FIND "${SOURCE}" "${_end}" _end_at)
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${SOURCE}" ${_begin_at} ${_length} _region)
    set(${OUT_REGION} "${_region}" PARENT_SCOPE)
endfunction()

function(stage11d_validate_facade_input_owner ACTIVE_SURFACE LEXICAL_SURFACE)
    set(_signature
        "PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges(")
    stage11d_count_raw_token("${ACTIVE_SURFACE}" "${_signature}"
        _active_signature_count)
    stage11d_count_raw_token("${LEXICAL_SURFACE}" "${_signature}"
        _lexical_signature_count)
    if(NOT _active_signature_count EQUAL 1
            OR NOT _lexical_signature_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
    string(FIND "${ACTIVE_SURFACE}" "${_signature}" _signature_position)
    stage11d_code_brace_depth("${ACTIVE_SURFACE}" ${_signature_position}
        _signature_depth)
    if(NOT _signature_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()

    evidence_find_cpp_function_bounds_in_sanitized(
        "${ACTIVE_SURFACE}" "${_signature}"
        _function_begin _function_open _function_end)
    math(EXPR _function_length
        "${_function_end} - ${_function_begin} + 1")
    string(SUBSTRING "${ACTIVE_SURFACE}" ${_function_begin}
        ${_function_length} _function)
    set(_stage11d_call
        "host_validation::inject_stage11d_physical_edges(")
    stage11d_count_raw_token("${_function}" "${_stage11d_call}"
        _stage11d_call_count)
    string(FIND "${_function}" "${_stage11d_call}"
        _stage11d_call_position)
    if(NOT _stage11d_call_count EQUAL 1
            OR _stage11d_call_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
    stage11d_code_brace_depth("${_function}" ${_stage11d_call_position}
        _stage11d_call_depth)
    if(NOT _stage11d_call_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" "" _normalized "${_function}")
    string(FIND "${_normalized}"
        "impl_->states.stage11d.suspend_injection=gameplay_rearm_required;"
        _suspend_binding)
    string(FIND "${_normalized}"
        "constPhysicalKeySnapshotstage11d_physical_keys=host_validation::inject_stage11d_physical_edges(stage11c_physical_keys,*impl_->config,input_settings,dungeon_snapshot,impl_->states.stage11d);"
        _binding)
    if(_suspend_binding EQUAL -1 OR _binding EQUAL -1
            OR NOT _suspend_binding LESS _binding)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
endfunction()

stage11d_unconditional_cpp_surface("${_host_validation_runtime_text}"
    _host_validation_runtime_code _host_validation_runtime_lexical_code)
stage11d_validate_facade_input_owner("${_host_validation_runtime_code}"
    "${_host_validation_runtime_lexical_code}")
if(DEFINED STAGE11D_INPUT_OWNER_ONLY AND STAGE11D_INPUT_OWNER_ONLY)
    message(STATUS "Stage11D facade runtime input-owner guard passed")
    return()
endif()

set(_host_validation_post_tick_signature
    "void HostValidationRuntime::observe_post_fixed_tick(")
stage11d_count_raw_token("${_host_validation_runtime_code}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_active_count)
stage11d_count_raw_token("${_host_validation_runtime_lexical_code}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_lexical_count)
if(NOT _host_validation_post_tick_active_count EQUAL 1
        OR NOT _host_validation_post_tick_lexical_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing facade abyss observer owner")
endif()
set(_host_validation_platform_namespace_signature
    "namespace arpg::platform {")
stage11d_count_raw_token("${_host_validation_runtime_code}"
    "${_host_validation_platform_namespace_signature}"
    _host_validation_platform_namespace_active_count)
stage11d_count_raw_token("${_host_validation_runtime_lexical_code}"
    "${_host_validation_platform_namespace_signature}"
    _host_validation_platform_namespace_lexical_count)
if(NOT _host_validation_platform_namespace_active_count EQUAL 1
        OR NOT _host_validation_platform_namespace_lexical_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected facade abyss observer namespace")
endif()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_host_validation_runtime_code}"
    "${_host_validation_platform_namespace_signature}"
    _host_validation_platform_namespace_begin
    _host_validation_platform_namespace_open
    _host_validation_platform_namespace_end)
math(EXPR _host_validation_platform_namespace_length
    "${_host_validation_platform_namespace_end} - ${_host_validation_platform_namespace_begin} + 1")
string(SUBSTRING "${_host_validation_runtime_code}"
    ${_host_validation_platform_namespace_begin}
    ${_host_validation_platform_namespace_length}
    _host_validation_platform_namespace)
evidence_find_cpp_function_bounds_in_sanitized(
    "${_host_validation_runtime_lexical_code}"
    "${_host_validation_platform_namespace_signature}"
    _host_validation_platform_namespace_lexical_begin
    _host_validation_platform_namespace_lexical_open
    _host_validation_platform_namespace_lexical_end)
math(EXPR _host_validation_platform_namespace_lexical_length
    "${_host_validation_platform_namespace_lexical_end} - ${_host_validation_platform_namespace_lexical_begin} + 1")
string(SUBSTRING "${_host_validation_runtime_lexical_code}"
    ${_host_validation_platform_namespace_lexical_begin}
    ${_host_validation_platform_namespace_lexical_length}
    _host_validation_platform_namespace_lexical)
stage11d_count_raw_token("${_host_validation_platform_namespace}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_namespace_active_count)
stage11d_count_raw_token("${_host_validation_platform_namespace_lexical}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_namespace_lexical_count)
if(NOT _host_validation_post_tick_namespace_active_count EQUAL 1
        OR NOT _host_validation_post_tick_namespace_lexical_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected facade abyss observer namespace")
endif()
string(FIND "${_host_validation_runtime_code}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_position)
stage11d_code_brace_depth("${_host_validation_runtime_code}"
    ${_host_validation_post_tick_position}
    _host_validation_post_tick_depth)
if(NOT _host_validation_post_tick_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected facade abyss observer scope")
endif()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_host_validation_runtime_code}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_begin _host_validation_post_tick_open
    _host_validation_post_tick_end)
math(EXPR _host_validation_post_tick_length
    "${_host_validation_post_tick_end} - ${_host_validation_post_tick_begin} + 1")
string(SUBSTRING "${_host_validation_runtime_code}"
    ${_host_validation_post_tick_begin} ${_host_validation_post_tick_length}
    _host_validation_post_tick_function)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _host_validation_post_tick_normalized
    "${_host_validation_post_tick_function}")
evidence_find_cpp_function_bounds_in_sanitized(
    "${_host_validation_runtime_lexical_code}"
    "${_host_validation_post_tick_signature}"
    _host_validation_post_tick_lexical_begin
    _host_validation_post_tick_lexical_open
    _host_validation_post_tick_lexical_end)
math(EXPR _host_validation_post_tick_lexical_length
    "${_host_validation_post_tick_lexical_end} - ${_host_validation_post_tick_lexical_begin} + 1")
string(SUBSTRING "${_host_validation_runtime_lexical_code}"
    ${_host_validation_post_tick_lexical_begin}
    ${_host_validation_post_tick_lexical_length}
    _host_validation_post_tick_lexical_function)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _host_validation_post_tick_lexical_normalized
    "${_host_validation_post_tick_lexical_function}")
set(_host_validation_post_tick_expected
    "voidHostValidationRuntime::observe_post_fixed_tick(constdungeon::DungeonSnapshot&snapshot,constitems::ItemOwnershipState*ownership)noexcept{if(ownership==nullptr)return;host_validation::observe_stage11d_abyss_claim(impl_->states.stage11d,snapshot,*ownership);}")
if(NOT _host_validation_post_tick_normalized STREQUAL
        _host_validation_post_tick_expected
        OR NOT _host_validation_post_tick_lexical_normalized STREQUAL
            _host_validation_post_tick_expected)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected facade abyss observer binding")
endif()
if(DEFINED STAGE11D_POST_TICK_OWNER_ONLY AND STAGE11D_POST_TICK_OWNER_ONLY)
    message(STATUS "Stage11D facade post-tick owner guard passed")
    return()
endif()

set(_host_facade_input_token
    "validation_runtime->inject_physical_edges(")
stage11d_fold_cpp_phase2_splices("${_host_text}" _host_phase2_text)
stage11d_count_raw_token("${_host_phase2_text}"
    "${_host_facade_input_token}" _host_raw_input_count)
# The production Host has one raw occurrence. Its active scope, exact
# arguments and ordering are proven again by the much smaller input crop
# below. Only ambiguous raw inventories pay for the full 79 KiB lexical and
# unconditional scans, which distinguish real/inactive duplicates from
# harmless comment, string and raw-string decoys.
if(NOT _host_raw_input_count EQUAL 1)
    stage11d_unconditional_cpp_surface("${_host_text}"
        _host_active_code _host_lexical_code)
    stage11d_count_raw_token("${_host_active_code}"
        "${_host_facade_input_token}" _host_active_input_count)
    stage11d_count_raw_token("${_host_lexical_code}"
        "${_host_facade_input_token}" _host_lexical_input_count)
    if(NOT _host_active_input_count EQUAL 1
            OR NOT _host_lexical_input_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
endif()

set(_stage_header_labels state)
stage11d_prepare_marker_surface("${_stage_header_text}" stage_header
    "${_stage_header_labels}" _stage_header_code)
stage11d_require_marker_depth("${_stage_header_code}" stage_header state 2)

set(_runtime_labels selectors safe_movement physical_driver fixed_step_runtime)
stage11d_prepare_marker_surface("${_runtime_text}" runtime
    "${_runtime_labels}" _runtime_code)
stage11d_require_marker_depth("${_runtime_code}" runtime selectors 1)
stage11d_require_marker_depth("${_runtime_code}" runtime safe_movement 2)
stage11d_require_marker_depth("${_runtime_code}" runtime physical_driver 1)
stage11d_require_marker_depth("${_runtime_code}" runtime fixed_step_runtime 1)

set(_report_labels evidence_semantics)
stage11d_prepare_marker_surface("${_report_text}" report
    "${_report_labels}" _report_code)
stage11d_require_marker_depth("${_report_code}" report evidence_semantics 1)

if(NOT (DEFINED STAGE11D_ABYSS_PHYSICAL_ONLY
        AND STAGE11D_ABYSS_PHYSICAL_ONLY))
stage11d_unconditional_cpp_surface("${_formal_text}"
    _formal_code _formal_lexical_code)
set(_formal_run_host_signature "bool run_host(")
stage11d_count_raw_token("${_formal_code}" "${_formal_run_host_signature}"
    _formal_run_host_active_count)
stage11d_count_raw_token("${_formal_lexical_code}"
    "${_formal_run_host_signature}" _formal_run_host_lexical_count)
if(NOT _formal_run_host_active_count EQUAL 1
        OR NOT _formal_run_host_lexical_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate formal run_host")
endif()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_formal_code}" "${_formal_run_host_signature}"
    _formal_run_host_begin _formal_run_host_open _formal_run_host_end)
math(EXPR _formal_run_host_length
    "${_formal_run_host_end} - ${_formal_run_host_begin} + 1")
string(SUBSTRING "${_formal_code}" ${_formal_run_host_begin}
    ${_formal_run_host_length} _formal_run_host)

set(_formal_scenario_field "config.stage11d_loot_validation")
stage11d_count_raw_token("${_formal_run_host}" "${_formal_scenario_field}"
    _formal_scenario_count)
set(_formal_scenario_assignment
    "config.stage11d_loot_validation = spec.scenario;")
string(FIND "${_formal_run_host}" "${_formal_scenario_assignment}"
    _formal_scenario_position)
if(NOT _formal_scenario_count EQUAL 1
        OR _formal_scenario_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires one direct run_host scenario assignment")
endif()
stage11d_code_brace_depth("${_formal_run_host}"
    ${_formal_scenario_position} _formal_scenario_depth)
if(NOT _formal_scenario_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires direct run_host scenario assignment")
endif()
stage11d_require_direct_statement_owner("${_formal_run_host}"
    ${_formal_scenario_position}
    "Stage11D loot evidence guard requires direct run_host scenario assignment")

set(_formal_steps_field "config.validation_steps_per_frame")
stage11d_count_raw_token("${_formal_run_host}" "${_formal_steps_field}"
    _formal_steps_count)
set(_formal_steps_assignment
    "config.validation_steps_per_frame = 1U;")
string(FIND "${_formal_run_host}" "${_formal_steps_assignment}"
    _formal_steps_position)
if(NOT _formal_steps_count EQUAL 1 OR _formal_steps_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires one direct 1-tick run_host assignment")
endif()
stage11d_code_brace_depth("${_formal_run_host}"
    ${_formal_steps_position} _formal_steps_depth)
if(NOT _formal_steps_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires direct run_host fixed-step assignment")
endif()
stage11d_require_direct_statement_owner("${_formal_run_host}"
    ${_formal_steps_position}
    "Stage11D loot evidence guard requires direct run_host fixed-step assignment")

set(_formal_default_limit
    "constexpr std::uint32_t kStage11DDefaultPresentedFrameLimit = 4000U;")
set(_formal_rare_base_budget
    "constexpr std::uint64_t kStage11DRareAbyssBaseFrameBudget = 12000U;")
set(_formal_rare_frames_per_monster
    "constexpr std::uint64_t kStage11DRareAbyssFramesPerMonster = 96U;")
set(_formal_vsync_setting
    "constexpr bool kStage11DFormalVsyncEnabled = false;")
foreach(_limit IN ITEMS "${_formal_default_limit}"
        "${_formal_rare_base_budget}" "${_formal_rare_frames_per_monster}"
        "${_formal_vsync_setting}")
    stage11d_count_raw_token("${_formal_code}" "${_limit}" _limit_count)
    string(FIND "${_formal_code}" "${_limit}" _limit_position)
    if(NOT _limit_count EQUAL 1 OR _limit_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires exact formal presented-frame constants")
    endif()
    stage11d_code_brace_depth("${_formal_code}" ${_limit_position}
        _limit_depth)
    if(NOT _limit_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires namespace-scope presented-frame constants")
    endif()
endforeach()
string(FIND "${_formal_code}"
    "kStage11DRareOnlyAbyssPresentedFrameLimit" _obsolete_rare_limit)
if(NOT _obsolete_rare_limit EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected fixed rare abyss frame limit")
endif()
evidence_extract_cpp_function_block("${_formal_text}"
    "stage11d_rare_abyss_presented_frame_budget("
    _formal_rare_budget_function)
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_rare_budget_normalized
    "${_formal_rare_budget_function}")
foreach(_rare_budget_property IN ITEMS
        "monster_count==0U"
        "static_cast<std::uint64_t>(monster_count)>static_cast<std::uint64_t>(arpg::limits::kRoomMonsterCapacity)"
        "budget=kStage11DRareAbyssBaseFrameBudget+static_cast<std::uint64_t>(monster_count)*kStage11DRareAbyssFramesPerMonster;"
        "budget>(std::numeric_limits<std::uint32_t>::max)()"
        "returnstatic_cast<std::uint32_t>(budget);")
    string(FIND "${_formal_rare_budget_normalized}"
        "${_rare_budget_property}" _rare_budget_property_position)
    if(_rare_budget_property_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires bounded population-derived rare abyss frame budget")
    endif()
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_code_normalized
    "${_formal_code}")
stage11d_count_raw_token("${_formal_code}" "static_assert("
    _formal_budget_static_assert_count)
if(NOT _formal_budget_static_assert_count EQUAL 4)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires four active rare-abyss budget boundary assertions")
endif()
foreach(_formal_budget_assertion IN ITEMS
        "static_assert(!stage11d_rare_abyss_presented_frame_budget(0U).has_value());"
        "static_assert(stage11d_rare_abyss_presented_frame_budget(1U).value_or(0U)==12096U);"
        "static_assert(stage11d_rare_abyss_presented_frame_budget(static_cast<std::uint32_t>(arpg::limits::kRoomMonsterCapacity)).value_or(0U)==122592U);"
        "static_assert(!stage11d_rare_abyss_presented_frame_budget(static_cast<std::uint32_t>(arpg::limits::kRoomMonsterCapacity+1U)).has_value());")
    string(FIND "${_formal_code_normalized}"
        "${_formal_budget_assertion}" _formal_budget_assertion_position)
    if(_formal_budget_assertion_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires exact rare-abyss budget boundary assertions")
    endif()
endforeach()
evidence_extract_cpp_function_block("${_formal_text}"
    "abyss_room_profile(" _formal_abyss_room_profile)
set(_formal_profile_budget_assignment
    "const auto budget = stage11d_rare_abyss_presented_frame_budget(")
stage11d_count_raw_token("${_formal_abyss_room_profile}"
    "${_formal_profile_budget_assignment}" _formal_profile_budget_count)
string(FIND "${_formal_abyss_room_profile}"
    "${_formal_profile_budget_assignment}" _formal_profile_budget_position)
if(NOT _formal_profile_budget_count EQUAL 1
        OR _formal_profile_budget_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires the production monster count to derive the rare-abyss frame budget")
endif()
stage11d_code_brace_depth("${_formal_abyss_room_profile}"
    ${_formal_profile_budget_position} _formal_profile_budget_depth)
if(NOT _formal_profile_budget_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires a direct rare-abyss frame-budget derivation")
endif()
stage11d_require_direct_statement_owner("${_formal_abyss_room_profile}"
    ${_formal_profile_budget_position}
    "Stage11D loot evidence guard requires a direct rare-abyss frame-budget derivation")
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_profile_normalized
    "${_formal_abyss_room_profile}")
foreach(_formal_profile_binding IN ITEMS
        "constautobudget=stage11d_rare_abyss_presented_frame_budget(plan.monster_count);"
        "returnAbyssRoomProfile{plan.blueprint_hash,plan.monster_count,*budget};")
    string(FIND "${_formal_profile_normalized}"
        "${_formal_profile_binding}" _formal_profile_binding_position)
    if(_formal_profile_binding_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires the production monster count and budget to flow through the abyss profile")
    endif()
endforeach()
set(_formal_limit_field
    "config.validation_exit_after_presented_frames")
stage11d_count_raw_token("${_formal_run_host}" "${_formal_limit_field}"
    _formal_limit_count)
set(_formal_limit_assignment
    "config.validation_exit_after_presented_frames = spec.abyss")
string(FIND "${_formal_run_host}" "${_formal_limit_assignment}"
    _formal_limit_position)
if(NOT _formal_limit_count EQUAL 2 OR _formal_limit_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires one assignment and one zero check for the run_host frame limit")
endif()
stage11d_code_brace_depth("${_formal_run_host}"
    ${_formal_limit_position} _formal_limit_depth)
if(NOT _formal_limit_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires direct run_host frame limit")
endif()
stage11d_require_direct_statement_owner("${_formal_run_host}"
    ${_formal_limit_position}
    "Stage11D loot evidence guard requires direct run_host frame limit")
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_run_host_normalized
    "${_formal_run_host}")
set(_formal_validation_chain
    "config.stage11d_loot_validation=spec.scenario;config.validation_steps_per_frame=1U;config.validation_exit_after_presented_frames=spec.abyss?selected.abyss_presented_frame_budget:kStage11DDefaultPresentedFrameLimit;if(config.validation_exit_after_presented_frames==0U)returnfalse;")
string(FIND "${_formal_run_host_normalized}"
    "${_formal_validation_chain}" _formal_validation_chain_position)
if(_formal_validation_chain_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires contiguous formal run_host validation configuration")
endif()
evidence_extract_cpp_function_block("${_formal_text}"
    "bool prepare_scenario(" _formal_prepare_scenario)
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_prepare_normalized
    "${_formal_prepare_scenario}")
foreach(_prepare_property IN ITEMS
        "draft.vsync_enabled=kStage11DFormalVsyncEnabled;"
        "settings_store.save(loaded.settings,draft)"
        "abyss_build_round_tripped"
        "saved.verified_state.item_ownership.items.size()"
        "saved.verified_state.item_ownership.equipment.equipped_ids"
        "saved.verified_state.passive_tree.allocated_bits"
        "saved.verified_state.progression.level"
        "saved.verified_state.progression.earned_passive_points"
        "saved.verified_state.progression.unspent_passive_points"
        "settings_saved.settings.vsync_enabled==kStage11DFormalVsyncEnabled")
    string(FIND "${_formal_prepare_normalized}" "${_prepare_property}"
        _prepare_property_position)
    if(_prepare_property_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires persisted non-VSync abyss validation build")
    endif()
endforeach()
evidence_extract_cpp_function_block("${_formal_text}"
    "SelectedStates select_states(" _formal_select_states)
stage11d_mask_non_direct_executable_scopes("${_formal_select_states}"
    _formal_select_active)
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_select_normalized
    "${_formal_select_active}")
evidence_extract_cpp_function_block("${_formal_text}"
    "bool prepare_stage11d_live_damage_build("
    _formal_live_damage_build)
stage11d_mask_non_direct_executable_scopes("${_formal_live_damage_build}"
    _formal_live_damage_build_active)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _formal_live_damage_build_normalized
    "${_formal_live_damage_build_active}")
foreach(_live_damage_property IN ITEMS
        "constexprstd::uint16_tkBarrierAffix=12U;"
        "if(base->slot==items::ItemSlot::weapon)continue;"
        "if(affix.affix_id==kBarrierAffix){++removed_count;continue;}"
        "retained[retained_count++]=affix;"
        "item.affixes=retained;"
        "item.affix_count=retained_count;"
        "if(retained_count!=5U||!items::validate_item(item))returnfalse;"
        "returnstate.item_ownership.items.size()==6U&&non_weapon_count==5U&&removed_count==5U&&items::validate_ownership(state.item_ownership);")
    string(FIND "${_formal_live_damage_build_normalized}"
        "${_live_damage_property}" _live_damage_property_position)
    if(_live_damage_property_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires an exact live-damage validation build")
    endif()
endforeach()
stage11d_count_raw_token("${_formal_select_normalized}"
    "install_stage10_validation_build(" _formal_build_install_count)
stage11d_count_raw_token("${_formal_select_normalized}"
    "install_stage10_validation_survival_passives("
    _formal_survival_install_count)
stage11d_count_raw_token("${_formal_select_normalized}"
    "prepare_stage11d_live_damage_build("
    _formal_live_damage_install_count)
foreach(_build_property IN ITEMS
        "autovalidation_state=next.state;"
        "install_stage10_validation_build(validation_state)"
        "install_stage10_validation_survival_passives(validation_state)"
        "prepare_stage11d_live_damage_build(validation_state)"
        "selected.abyss=validation_state;")
    string(FIND "${_formal_select_normalized}" "${_build_property}"
        _build_property_position)
    if(_build_property_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires the validation build only on the selected abyss state")
    endif()
endforeach()
if(NOT _formal_build_install_count EQUAL 1
        OR NOT _formal_survival_install_count EQUAL 1
        OR NOT _formal_live_damage_install_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires the validation build, survival passives, and live-damage fixture only on the selected abyss state")
endif()
set(_formal_validation_fixture_chain
    "&&arpg::test::install_stage10_validation_build(validation_state)&&arpg::test::install_stage10_validation_survival_passives(validation_state)&&prepare_stage11d_live_damage_build(validation_state)){selected.abyss=validation_state;")
string(FIND "${_formal_select_normalized}"
    "${_formal_validation_fixture_chain}" _formal_validation_fixture_position)
if(_formal_validation_fixture_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires every validation fixture to gate the selected abyss state")
endif()

# Turn the exact manifest literals into identifiers before lexical sanitizing.
# A literal copied into a comment, raw string, inactive preprocessor branch,
# dead branch or uncalled lambda therefore cannot impersonate main's stream.
set(_formal_manifest_marked "${_formal_text}")
set(_formal_manifest_labels
    ABYSS_MONSTERS
    ABYSS_INITIAL_OWNED_ITEMS
    ABYSS_VALIDATION_LEVEL
    ABYSS_VALIDATION_EARNED_PASSIVES
    ABYSS_VALIDATION_UNSPENT_PASSIVES
    ABYSS_VALIDATION_PASSIVE_BITS
    ABYSS_PRESENTED_FRAME_BUDGET
    FORMAL_VSYNC_ENABLED)
foreach(_manifest_label IN LISTS _formal_manifest_labels)
    string(TOLOWER "${_manifest_label}" _manifest_key)
    set(_manifest_literal "\"${_manifest_key}=\"")
    stage11d_count_raw_token("${_formal_manifest_marked}"
        "${_manifest_literal}" _manifest_literal_count)
    if(NOT _manifest_literal_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires one exact ${_manifest_key} manifest literal")
    endif()
    string(REPLACE "${_manifest_literal}"
        "STAGE11D_MANIFEST_${_manifest_label}"
        _formal_manifest_marked "${_formal_manifest_marked}")
endforeach()
stage11d_unconditional_cpp_surface("${_formal_manifest_marked}"
    _formal_manifest_code)
set(_formal_main_signature "int main(")
stage11d_count_raw_token("${_formal_manifest_code}"
    "${_formal_main_signature}" _formal_main_count)
if(NOT _formal_main_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate the formal manifest owner")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_formal_manifest_code}"
    "${_formal_main_signature}" _formal_main_begin _formal_main_open
    _formal_main_end)
math(EXPR _formal_main_length
    "${_formal_main_end} - ${_formal_main_begin} + 1")
string(SUBSTRING "${_formal_manifest_code}" ${_formal_main_begin}
    ${_formal_main_length} _formal_main)
stage11d_mask_non_direct_executable_scopes("${_formal_main}"
    _formal_main_active)
string(REGEX REPLACE "[ \t\r\n]+" "" _formal_main_normalized
    "${_formal_main_active}")
foreach(_manifest_label IN LISTS _formal_manifest_labels)
    set(_manifest_token "STAGE11D_MANIFEST_${_manifest_label}")
    stage11d_count_raw_token("${_formal_main_active}"
        "${_manifest_token}" _manifest_token_count)
    string(FIND "${_formal_main_active}" "${_manifest_token}"
        _manifest_token_position)
    if(NOT _manifest_token_count EQUAL 1
            OR _manifest_token_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires active main-scope manifest bindings")
    endif()
    stage11d_code_brace_depth("${_formal_main_active}"
        ${_manifest_token_position} _manifest_token_depth)
    if(NOT _manifest_token_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires direct main-scope manifest bindings")
    endif()
endforeach()
foreach(_manifest_chain IN ITEMS
        "STAGE11D_MANIFEST_ABYSS_MONSTERS<<selected.abyss_monster_count"
        "STAGE11D_MANIFEST_ABYSS_INITIAL_OWNED_ITEMS<<selected.abyss_initial_owned_item_count"
        "STAGE11D_MANIFEST_ABYSS_VALIDATION_LEVEL<<static_cast<unsigned>(selected.abyss.progression.level)"
        "STAGE11D_MANIFEST_ABYSS_VALIDATION_EARNED_PASSIVES<<static_cast<unsigned>(selected.abyss.progression.earned_passive_points)"
        "STAGE11D_MANIFEST_ABYSS_VALIDATION_UNSPENT_PASSIVES<<static_cast<unsigned>(selected.abyss.progression.unspent_passive_points)"
        "STAGE11D_MANIFEST_ABYSS_VALIDATION_PASSIVE_BITS<<selected.abyss.passive_tree.allocated_bits"
        "STAGE11D_MANIFEST_ABYSS_PRESENTED_FRAME_BUDGET<<selected.abyss_presented_frame_budget"
        "STAGE11D_MANIFEST_FORMAL_VSYNC_ENABLED<<(kStage11DFormalVsyncEnabled?1:0)")
    string(FIND "${_formal_main_normalized}" "${_manifest_chain}"
        _manifest_chain_position)
    if(_manifest_chain_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires exact active manifest value bindings")
    endif()
endforeach()
execute_process(
    COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${_validator_ast_guard}" -ValidatorPath "${_validator}"
    RESULT_VARIABLE _stage11d_validator_ast_result
    OUTPUT_VARIABLE _stage11d_validator_ast_output
    ERROR_VARIABLE _stage11d_validator_ast_error)
if(NOT _stage11d_validator_ast_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected formal validator executable bindings: ${_stage11d_validator_ast_output}\n${_stage11d_validator_ast_error}")
endif()
set(_stage11d_validator_ast_checked TRUE)
if(DEFINED STAGE11D_FORMAL_LIMIT_ONLY AND STAGE11D_FORMAL_LIMIT_ONLY)
    message(STATUS "Stage11D formal run_host limit guard passed")
    return()
endif()
endif()

set(_host_labels fixed_step abyss_claim)
string(REPLACE "\r\n" "\n" _host_marker_count_text "${_host_text}")
foreach(_label IN LISTS _host_labels)
    foreach(_kind IN ITEMS BEGIN END)
        set(_marker
            "// STAGE11D_LOOT_VALIDATION_SEAM_${_kind} ${_label}")
        stage11d_count_raw_token("${_host_marker_count_text}"
            "${_marker}\n" _count)
        if(NOT _count EQUAL 1)
            message(FATAL_ERROR
                "Stage11D loot evidence guard cannot bind host ${_label} seam marker")
        endif()
    endforeach()
endforeach()

string(FIND "${_host_text}"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    _host_input_begin)
if(_host_input_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host input chain")
endif()
string(SUBSTRING "${_host_text}" ${_host_input_begin} -1 _host_input_tail)
string(FIND "${_host_input_tail}"
    "core::FixedStepFrame frame = host_gate.fixed_step;" _host_input_end)
if(_host_input_end EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host input chain")
endif()
string(SUBSTRING "${_host_input_tail}" 0 ${_host_input_end}
    _host_input_crop)
stage11d_unconditional_cpp_surface("${_host_input_crop}" _host_input_code)

string(FIND "${_host_text}"
    "core::FixedStepFrame frame = host_gate.fixed_step;"
    _host_fixed_step_begin)
if(_host_fixed_step_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host fixed-step chain")
endif()
string(SUBSTRING "${_host_text}" ${_host_fixed_step_begin} -1
    _host_fixed_step_tail)
string(FIND "${_host_fixed_step_tail}"
    "if (inventory.is_open() != inventory_open_before"
    _host_fixed_step_end)
if(_host_fixed_step_end EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host fixed-step chain")
endif()
string(SUBSTRING "${_host_fixed_step_tail}" 0 ${_host_fixed_step_end}
    _host_fixed_step_crop)
set(_host_fixed_step_labels fixed_step abyss_claim)
stage11d_prepare_marker_surface("${_host_fixed_step_crop}" host
    "${_host_fixed_step_labels}" _host_fixed_step_code)
stage11d_require_marker_depth("${_host_fixed_step_code}" host fixed_step 1)
stage11d_require_marker_depth("${_host_fixed_step_code}" host abyss_claim 1)

set(_stage11d_host_seam "")
foreach(_label IN LISTS _host_labels)
    stage11d_extract_raw_seam("${_host_text}" "${_label}" _region)
    string(APPEND _stage11d_host_seam "\n${_region}")
endforeach()
stage11d_extract_raw_seam("${_report_text}" evidence_semantics
    _stage11d_report_seam)
set(_stage11d_semantic_seam
    "${_stage11d_host_seam}\n${_stage11d_report_seam}\n${_host_validation_runtime_text}")
arpg_sanitize_cpp_source("${_stage11d_host_seam}" _stage11d_host_code)
arpg_sanitize_cpp_source("${_stage11d_semantic_seam}"
    _stage11d_semantic_code)

function(stage11d_extract_runtime_definition LABEL SIGNATURE OUT_FUNCTION)
    stage11d_count_raw_token("${_runtime_code}" "${SIGNATURE}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime ${LABEL} definition")
    endif()
    string(FIND "${_runtime_code}" "${SIGNATURE}" _begin)
    string(SUBSTRING "${_runtime_code}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "{" _open)
    string(FIND "${_tail}" ";" _semicolon)
    if(_open EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _open))
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime ${LABEL} definition")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${_runtime_code}"
        "${SIGNATURE}" _function_begin _function_open _function_end)
    math(EXPR _function_length
        "${_function_end} - ${_function_begin} + 1")
    string(SUBSTRING "${_runtime_code}" ${_function_begin}
        ${_function_length} _function)
    set(${OUT_FUNCTION} "${_function}" PARENT_SCOPE)
endfunction()

function(stage11d_extract_report_definition LABEL SIGNATURE OUT_FUNCTION)
    stage11d_count_raw_token("${_report_code}" "${SIGNATURE}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report ${LABEL} definition")
    endif()
    string(FIND "${_report_code}" "${SIGNATURE}" _begin)
    string(SUBSTRING "${_report_code}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "{" _open)
    string(FIND "${_tail}" ";" _semicolon)
    if(_open EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _open))
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report ${LABEL} definition")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${_report_code}"
        "${SIGNATURE}" _function_begin _function_open _function_end)
    math(EXPR _function_length
        "${_function_end} - ${_function_begin} + 1")
    string(SUBSTRING "${_report_code}" ${_function_begin}
        ${_function_length} _function)
    set(${OUT_FUNCTION} "${_function}" PARENT_SCOPE)
endfunction()

function(stage11d_require_unique_token_depth LABEL SURFACE TOKEN EXPECTED_DEPTH)
    stage11d_count_raw_token("${SURFACE}" "${TOKEN}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} token inventory")
    endif()
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    stage11d_code_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} scope")
    endif()
endfunction()

function(stage11d_find_host_code_token TOKEN OUT_POSITION)
    string(FIND "${_host_text}" "${TOKEN}" _raw_position)
    if(_raw_position EQUAL -1)
        set(${OUT_POSITION} -1 PARENT_SCOPE)
        return()
    endif()
    evidence_find_cpp_code_token("${_host_text}" "${TOKEN}" _code_position)
    set(${OUT_POSITION} ${_code_position} PARENT_SCOPE)
endfunction()

stage11d_require_unique_token_depth("stage_header state definition"
    "${_stage_header_code}" "struct Stage11DLootValidationState final" 2)

foreach(_required IN ITEMS
        "struct Stage11DLootValidationState final"
        "std::array<std::uint64_t, dungeon::kGroundDropCapacity> snapshot_item_ids{};"
        "std::array<std::uint64_t, dungeon::kGroundDropCapacity> inventory_item_ids{};"
        "std::array<std::uint32_t, 3> observed_drop_distance_milli{};"
        "std::uint32_t initial_monster_count{};"
        "std::uint32_t max_remaining_targets{};"
        "std::uint32_t max_defeated_monsters{};"
        "std::uint32_t monster_generator_version{};"
        "std::uint64_t monster_blueprint_hash{};"
        "bool monster_damage_observed{};"
        "std::uint64_t pickup_commit_generation{};"
        "bool abyss_claim_requested{};" "bool abyss_claimed{};"
        "bool suspend_injection{};"
        "bool sweep_cursor_initialized{};"
        "std::uint8_t sweep_waypoint{};"
        "Stage10GridRouteState sweep_grid{};"
        "Stage10ValidationState abyss_ranged{};"
        "PhysicalKeySnapshot inject_stage11d_physical_edges("
        "bool stage11d_validation_active("
        "void observe_stage11d_abyss_claim("
        "bool stage11d_target_visible("
        "void stage11d_record_semantics("
        "void write_stage11d_loot_validation_summary("
        "enum class LootFilterMode : std::uint8_t;"
        "struct DungeonRenderStatus;" "struct PauseMenuState;")
    string(FIND "${_stage_header_code}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime header token: ${_required}")
    endif()
endforeach()
foreach(_forbidden IN ITEMS "raylib.h" "renderer" "persistence" "test")
    string(FIND "${_stage_header_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected runtime header dependency: ${_forbidden}")
    endif()
endforeach()
stage11d_find_host_code_token("struct Stage11DLootValidationState final"
    _host_state_definition)
if(NOT _host_state_definition EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard found runtime state definition in host")
endif()

foreach(_declaration IN ITEMS
        "bool stage11d_target_visible("
        "void stage11d_record_semantics("
        "void write_stage11d_loot_validation_summary(")
    stage11d_require_unique_token_depth("report header declaration"
        "${_stage_header_code}" "${_declaration}" 2)
endforeach()

set(_runtime_definitions
    "ordinary rarity selector|bool stage11d_has_three_ordinary_rarities(|1"
    "ground selector|const dungeon::GroundItemSnapshot* stage11d_nearest_ground(|2"
    "rare abyss ground selector|const dungeon::GroundItemSnapshot*\nstage11d_rare_abyss_ground(|2"
    "monster selector|const combat::MonsterSnapshot* stage11d_priority_monster(|2"
    "attack selector|bool stage11d_attack_lane(|2"
    "rare abyss skill helper|Stage11DRareAbyssSkillInput\ninject_stage11d_rare_abyss_area_skill(|2"
    "rare abyss availability|bool stage11d_rare_abyss_player_available(|2"
    "rare abyss danger|bool stage11d_rare_abyss_danger_near_player(|2"
    "rare abyss ordinal selector|const combat::MonsterSnapshot*\nstage11d_monster_by_ordinal(|2"
    "rare abyss outer sweep default|bool stage11d_outer_sweep_route_is_default(|2"
    "rare abyss sweep cursor initializer|void initialize_stage11d_sweep_cursor(|2"
    "safe movement|combat::MovementInput stage11d_safe_movement_toward(|2"
    "physical driver|PhysicalKeySnapshot inject_stage11d_physical_edges(|1"
    "fixed-step activation|bool stage11d_validation_active(|1"
    "abyss observer|void observe_stage11d_abyss_claim(|1")
foreach(_entry IN LISTS _runtime_definitions)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _label)
    list(GET _parts 1 _signature)
    list(GET _parts 2 _expected_depth)
    stage11d_require_unique_token_depth("runtime ${_label} definition"
        "${_runtime_code}" "${_signature}" ${_expected_depth})
    stage11d_extract_runtime_definition("${_label}" "${_signature}"
        _definition)
endforeach()

foreach(_signature IN ITEMS
        "bool stage11d_has_three_ordinary_rarities("
        "const dungeon::GroundItemSnapshot* stage11d_nearest_ground("
        "const dungeon::GroundItemSnapshot*\nstage11d_rare_abyss_ground("
        "const combat::MonsterSnapshot* stage11d_priority_monster("
        "bool stage11d_attack_lane("
        "Stage11DRareAbyssSkillInput\ninject_stage11d_rare_abyss_area_skill("
        "bool stage11d_rare_abyss_player_available("
        "bool stage11d_rare_abyss_danger_near_player("
        "const combat::MonsterSnapshot*\nstage11d_monster_by_ordinal("
        "bool stage11d_outer_sweep_route_is_default("
        "void initialize_stage11d_sweep_cursor("
        "combat::MovementInput stage11d_safe_movement_toward("
        "PhysicalKeySnapshot inject_stage11d_physical_edges("
        "bool stage11d_validation_active("
        "void observe_stage11d_abyss_claim(")
    stage11d_find_host_code_token("${_signature}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard found runtime definition in host")
    endif()
endforeach()

set(_report_definitions
    "rarity-view helper|bool stage11d_view_has_rarity(|2"
    "abyss ordinal-view helper|bool stage11d_view_has_abyss_ordinal(|2"
    "semantic recorder|void stage11d_record_semantics(|1"
    "target-visible evaluator|bool stage11d_target_visible(|1"
    "scenario-name helper|const char* stage11d_scenario_name(|2"
    "summary writer|void write_stage11d_loot_validation_summary(|1")
foreach(_entry IN LISTS _report_definitions)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _label)
    list(GET _parts 1 _signature)
    list(GET _parts 2 _expected_depth)
    stage11d_require_unique_token_depth("report ${_label} definition"
        "${_report_code}" "${_signature}" ${_expected_depth})
    stage11d_extract_report_definition("${_label}" "${_signature}"
        _definition)
    stage11d_find_host_code_token("${_signature}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard found report definition in host")
    endif()
endforeach()

stage11d_extract_report_definition("semantic recorder"
    "void stage11d_record_semantics(" _report_record_function)
stage11d_extract_report_definition("target-visible evaluator"
    "bool stage11d_target_visible(" _report_target_function)
stage11d_extract_report_definition("abyss ordinal-view helper"
    "bool stage11d_view_has_abyss_ordinal("
    _report_abyss_ordinal_function)
stage11d_mask_non_direct_executable_scopes(
    "${_report_abyss_ordinal_function}"
    _report_abyss_ordinal_active)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _report_abyss_ordinal_normalized
    "${_report_abyss_ordinal_active}")
foreach(_required IN ITEMS
        "index<view.count"
        "view.labels[index].ordinal==ordinal"
        "view.labels[index].abyss"
        "returntrue;"
        "returnfalse;")
    string(FIND "${_report_abyss_ordinal_normalized}" "${_required}"
        _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires an abyss label with the same ordinal")
    endif()
endforeach()
stage11d_extract_direct_braced_statement("rare abyss target visibility"
    "${_report_target_function}"
    "if (scenario == Scenario::rare_only_abyss) {" 1
    _report_rare_abyss_target)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _report_rare_abyss_target_normalized
    "${_report_rare_abyss_target}")
set(_report_rare_abyss_target_chain
    "if(scenario==Scenario::rare_only_abyss){for(std::size_tindex=0U;index<snapshot.ground_item_count;++index){constauto&item=snapshot.ground_items[index];if(item.source==dungeon::GroundItemSource::abyss_chest&&item.rarity!=items::ItemRarity::rare&&ground_loot_visible(item,mode)&&stage11d_view_has_abyss_ordinal(view,item.ordinal)){returntrue;}}returnfalse;}")
if(NOT _report_rare_abyss_target_normalized STREQUAL
        _report_rare_abyss_target_chain)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires rare capture to bind the matching abyss label")
endif()
stage11d_extract_report_definition("summary writer"
    "void write_stage11d_loot_validation_summary(" _report_summary_function)
string(REGEX REPLACE "[ \t\r\n]+" "" _stage_header_normalized
    "${_stage_header_code}")
string(REGEX REPLACE "[ \t\r\n]+" "" _report_target_normalized
    "${_report_target_function}")
foreach(_dimension_binding IN ITEMS
        "Stage11DLootValidationState&,int,int)noexcept;"
        "intscreen_width,intscreen_height"
        "make_hud_layout(screen_width,screen_height,false)")
    if(_dimension_binding MATCHES "State")
        set(_dimension_surface "${_stage_header_normalized}")
    else()
        set(_dimension_surface "${_report_target_normalized}")
    endif()
    string(FIND "${_dimension_surface}" "${_dimension_binding}"
        _dimension_found)
    if(_dimension_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected explicit screen dimensions")
    endif()
endforeach()
if(_report_target_normalized MATCHES "GetScreen(Width|Height)[(]")
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected global screen dimensions")
endif()
endif()

stage11d_unconditional_cpp_surface("${_host_validation_runtime_text}"
    _stage11d_facade_code _stage11d_facade_lexical)
foreach(_entry IN ITEMS
        "ground-loot|void HostValidationRuntime::observe_ground_loot("
        "presented-frame|PresentationDecision HostValidationRuntime::observe_presented_frame("
        "capture-result|void HostValidationRuntime::observe_capture_result("
        "summary|void HostValidationRuntime::write_summaries(")
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _label)
    list(GET _parts 1 _signature)
    stage11d_count_raw_token("${_stage11d_facade_code}" "${_signature}"
        _definition_count)
    if(NOT _definition_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires one active facade ${_label} owner")
    endif()
endforeach()
evidence_extract_cpp_function_block("${_stage11d_facade_code}"
    "void HostValidationRuntime::observe_ground_loot(" _facade_ground)
evidence_extract_cpp_function_block("${_stage11d_facade_code}"
    "PresentationDecision HostValidationRuntime::observe_presented_frame("
    _facade_presented)
evidence_extract_cpp_function_block("${_stage11d_facade_code}"
    "void HostValidationRuntime::observe_capture_result(" _facade_capture)
evidence_extract_cpp_function_block("${_stage11d_facade_code}"
    "void HostValidationRuntime::write_summaries(" _facade_summaries)
foreach(_surface IN ITEMS
        _facade_ground _facade_presented _facade_capture _facade_summaries)
    stage11d_mask_non_direct_executable_scopes("${${_surface}}"
        _facade_direct_surface)
    string(REGEX REPLACE "[ \t\r\n]+" "" ${_surface}_normalized
        "${_facade_direct_surface}")
endforeach()

string(FIND "${_facade_ground_normalized}"
    "if(impl_->states.stage11d.target_visible&&!impl_->states.stage11d.captured){host_validation::stage11d_record_semantics("
    _ground_record_condition)
if(_ground_record_condition EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected target-visible uncaptured semantic recording")
endif()
foreach(_binding IN ITEMS
        "stage11d_target_visible(*impl_->config,snapshot,pause_menu,render_status,loot_filter,ground_loot_view,notices,impl_->states.stage11d,screen_width,screen_height)"
        "stage11d_record_semantics(impl_->states.stage11d,snapshot,ownership,ground_loot_view,notices)")
    string(FIND "${_facade_ground_normalized}" "${_binding}" _binding_found)
    if(_binding_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected facade semantic binding")
    endif()
endforeach()
if(_facade_ground_normalized MATCHES "GetScreen(Width|Height)[(]")
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected facade global screen dimensions")
endif()
foreach(_binding IN ITEMS
        "stage11d_reached"
        "validation_complete="
        "stage11d_reached"
        "generic_capture_visible=")
    string(FIND "${_facade_presented_normalized}" "${_binding}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard rejected decision.validation_complete binding")
    endif()
endforeach()
string(FIND "${_facade_presented_normalized}"
    "validation_complete=" _validation_complete_begin)
string(SUBSTRING "${_facade_presented_normalized}"
    ${_validation_complete_begin} -1 _validation_complete_tail)
string(FIND "${_validation_complete_tail}" "stage11d_reached"
    _stage11d_completion_member)
if(_stage11d_completion_member EQUAL -1)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected decision.validation_complete binding")
endif()
string(FIND "${_facade_presented_normalized}"
    "decision.validation_complete=stage10_reached||stage11_reached||stage11b_reached||stage11c_reached||stage11d_reached||stage17_reached;"
    _exact_validation_complete)
stage11d_count_raw_token("${_facade_presented_normalized}"
    "decision" _facade_decision_identifier_count)
foreach(_decision_field IN ITEMS
        validation_complete generic_capture_visible generic_capture_complete
        stage17_capture_path capture_owner)
    stage11d_count_raw_token("${_facade_presented_normalized}"
        "decision.${_decision_field}" _facade_decision_field_count)
    if(NOT _facade_decision_field_count EQUAL 2)
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard rejected single-write PresentationDecision semantics")
    endif()
endforeach()
if(_exact_validation_complete EQUAL -1
        OR NOT _facade_decision_identifier_count EQUAL 12)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected decision.validation_complete binding")
endif()
string(REGEX MATCH
    "impl_->([A-Za-z_][A-Za-z0-9_]*)=impl_->states[.]stage11d[.]target_visible;"
    _pending_stage11d_binding "${_facade_presented_normalized}")
set(_stage11d_pending_field "${CMAKE_MATCH_1}")
string(FIND "${_facade_capture_normalized}"
    "if(impl_->${_stage11d_pending_field}){impl_->states.stage11d.captured=true;}"
    _captured_binding)
if("${_pending_stage11d_binding}" STREQUAL ""
        OR "${_stage11d_pending_field}" STREQUAL ""
        OR _captured_binding EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected successful pending capture promotion")
endif()
stage11d_count_raw_token("${_facade_summaries_normalized}"
    "write_stage11d_loot_validation_summary(" _stage11d_summary_count)
if(NOT _stage11d_summary_count EQUAL 1)
    message(FATAL_ERROR
        "T7C-M25: Stage11D loot evidence guard requires one Stage11D summary write")
endif()
foreach(_task7c_summary_identifier IN ITEMS
        write_stage11b_validation_summary
        write_stage11c_hud_validation_summary
        write_stage11d_loot_validation_summary
        write_stage17_validation_summary)
    stage11d_count_raw_token("${_stage11d_facade_code}"
        "${_task7c_summary_identifier}"
        _task7c_runtime_summary_identifier_count)
    if(NOT _task7c_runtime_summary_identifier_count EQUAL 1)
        message(FATAL_ERROR
            "T7C-M25: Stage11D loot evidence guard rejected Runtime summary inventory")
    endif()
endforeach()
set(_task7c_expected_runtime_summaries
    "voidHostValidationRuntime::write_summaries(CleanShutdownStateclean_shutdown_state,constPauseMenuState&pause_menu)noexcept{impl_->states.stage17.clean_shutdown_exact_ready=clean_shutdown_state==CleanShutdownState::ready;host_validation::write_stage11b_validation_summary(*impl_->config,impl_->states.stage11b,pause_menu);host_validation::write_stage11c_hud_validation_summary(*impl_->config,impl_->states.stage11c);host_validation::write_stage11d_loot_validation_summary(*impl_->config,impl_->states.stage11d,pause_menu);host_validation::write_stage17_validation_summary(*impl_->config,impl_->states.stage17);}")
if(NOT _facade_summaries_normalized STREQUAL
        _task7c_expected_runtime_summaries)
    message(FATAL_ERROR
        "T7C-M25: Stage11D loot evidence guard rejected the canonical Runtime summary body")
endif()

stage11d_fold_cpp_phase2_splices("${_host_text}" _stage11d_host_phase2)
stage11d_unconditional_cpp_surface("${_stage11d_host_phase2}"
    _stage11d_host_active _stage11d_host_lexical)
set(_task7c_run_signature "HostExitCode run_raylib_host(")
stage11d_count_raw_token("${_stage11d_host_active}"
    "${_task7c_run_signature}" _task7c_active_run_count)
stage11d_count_raw_token("${_stage11d_host_lexical}"
    "${_task7c_run_signature}" _task7c_lexical_run_count)
string(FIND "${_stage11d_host_active}" "${_task7c_run_signature}"
    _task7c_run_position)
if(NOT _task7c_active_run_count EQUAL 1
        OR NOT _task7c_lexical_run_count EQUAL 1
        OR _task7c_run_position EQUAL -1)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard requires one real run_raylib_host owner")
endif()
stage11d_code_brace_depth("${_stage11d_host_active}"
    ${_task7c_run_position} _task7c_run_depth)
if(NOT _task7c_run_depth EQUAL 1)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected run_raylib_host scope")
endif()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_stage11d_host_active}" "${_task7c_run_signature}"
    _task7c_run_begin _task7c_run_open _task7c_run_end)
math(EXPR _task7c_run_length
    "${_task7c_run_end} - ${_task7c_run_begin} + 1")
string(SUBSTRING "${_stage11d_host_active}" ${_task7c_run_begin}
    ${_task7c_run_length} _stage11d_host_run)
stage11d_mask_non_direct_executable_scopes("${_stage11d_host_run}"
    _stage11d_host_direct)

foreach(_task7c_macro_surface IN ITEMS
        _stage11d_host_lexical _stage11d_facade_lexical)
    string(REGEX MATCH
        "(^|\n)[ \t]*#[ \t]*(define|undef)([ \t\r\n]|$)"
        _task7c_macro_directive "${${_task7c_macro_surface}}")
    if(NOT "${_task7c_macro_directive}" STREQUAL "")
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard rejected validation macro rewriting")
    endif()
endforeach()

foreach(_task7c_run_inventory IN ITEMS
        "begin_clean_exit|7" "presented_frame_count|7"
        "exit_requested|6" "request_clean_shutdown|1")
    string(REPLACE "|" ";" _task7c_inventory_parts
        "${_task7c_run_inventory}")
    list(GET _task7c_inventory_parts 0 _task7c_inventory_token)
    list(GET _task7c_inventory_parts 1 _task7c_inventory_expected)
    stage11d_count_raw_token("${_stage11d_host_run}"
        "${_task7c_inventory_token}" _task7c_inventory_count)
    if(NOT _task7c_inventory_count EQUAL _task7c_inventory_expected)
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard rejected Host exit-state inventory")
    endif()
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" "" _task7c_host_run_normalized
    "${_stage11d_host_run}")
set(_task7c_exit_declaration "boolexit_requested=false;")
set(_task7c_exit_lambda
    "constautobegin_clean_exit=[&]()noexcept{if(runtime.state()==DungeonRuntimeState::running&&runtime.session()!=nullptr){if(runtime.clean_shutdown_state()==CleanShutdownState::ready){exit_requested=true;}else{static_cast<void>(runtime.request_clean_shutdown());}return;}exit_requested=true;};")
set(_task7c_exit_loop "while(!exit_requested){")
set(_task7c_exit_shutdown_ready
    "if(runtime.clean_shutdown_state()==CleanShutdownState::ready||runtime.clean_shutdown_state()==CleanShutdownState::faulted){exit_requested=true;continue;}")
set(_task7c_exit_stage12_recovery
    "if(config.stage12_material_background_only||config.stage12_material_icons_only){TraceLog(LOG_ERROR,);exit_requested=true;continue;}")
set(_task7c_exit_residual "${_task7c_host_run_normalized}")
foreach(_task7c_exit_context IN ITEMS
        declaration lambda loop shutdown_ready stage12_recovery)
    set(_task7c_exit_context_variable
        "_task7c_exit_${_task7c_exit_context}")
    set(_task7c_exit_context_text
        "${${_task7c_exit_context_variable}}")
    stage11d_count_raw_token("${_task7c_exit_residual}"
        "${_task7c_exit_context_text}" _task7c_exit_context_count)
    if(NOT _task7c_exit_context_count EQUAL 1)
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard rejected canonical Host exit-state context")
    endif()
    string(REPLACE "${_task7c_exit_context_text}" ""
        _task7c_exit_residual "${_task7c_exit_residual}")
endforeach()
stage11d_count_raw_token("${_task7c_exit_residual}"
    "exit_requested" _task7c_residual_exit_state_count)
if(NOT _task7c_residual_exit_state_count EQUAL 0)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected residual Host exit-state access")
endif()
string(REGEX MATCH
    "(^|[^A-Za-z0-9_.>])(exit|quick_exit|_Exit|abort|terminate|ExitProcess|TerminateProcess|FatalExit|PostQuitMessage)([^A-Za-z0-9_]|$)"
    _task7c_dangerous_exit_identifier "${_task7c_host_run_normalized}")
if(NOT "${_task7c_dangerous_exit_identifier}" STREQUAL "")
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected a dangerous Host exit identifier")
endif()

stage11d_count_raw_token("${_stage11d_host_active}"
    "write_summaries" _task7c_host_summary_identifier_count)
if(NOT _task7c_host_summary_identifier_count EQUAL 1)
    message(FATAL_ERROR
        "T7C-M25: Stage11D loot evidence guard requires one Host summary write")
endif()
set(_task7c_main_loop_begin "while (!exit_requested) {")
stage11d_count_raw_token("${_stage11d_host_direct}"
    "${_task7c_main_loop_begin}" _task7c_main_loop_count)
string(FIND "${_stage11d_host_direct}" "${_task7c_main_loop_begin}"
    _task7c_main_loop_position)
string(LENGTH "${_task7c_main_loop_begin}" _task7c_main_loop_begin_length)
math(EXPR _task7c_main_loop_open
    "${_task7c_main_loop_position} + ${_task7c_main_loop_begin_length} - 1")
if(NOT _task7c_main_loop_count EQUAL 1
        OR _task7c_main_loop_position EQUAL -1)
    message(FATAL_ERROR
        "T7C-M25: Stage11D loot evidence guard cannot bind the Host main loop")
endif()
stage11d_matching_brace_position("${_stage11d_host_direct}"
    ${_task7c_main_loop_open} _task7c_main_loop_close)
math(EXPR _task7c_post_loop_begin "${_task7c_main_loop_close} + 1")
string(SUBSTRING "${_stage11d_host_direct}" ${_task7c_post_loop_begin}
    -1 _task7c_post_loop_surface)
string(REGEX REPLACE "[ \t\r\n]+" "" _task7c_post_loop_normalized
    "${_task7c_post_loop_surface}")
set(_task7c_unconditional_summary_tail
    "validation_runtime->write_summaries(runtime.clean_shutdown_state(),pause_menu);audio.shutdown();renderer.shutdown_resources();pause_menu_renderer.shutdown();window.close();returnHostExitCode::success;")
string(FIND "${_task7c_post_loop_normalized}"
    "${_task7c_unconditional_summary_tail}" _task7c_summary_tail_position)
if(NOT _task7c_summary_tail_position EQUAL 0)
    message(FATAL_ERROR
        "T7C-M25: Stage11D loot evidence guard rejected the unconditional Host summary tail")
endif()

set(_task7c_decision_begin "const PresentationDecision decision =")
set(_task7c_summary_begin "validation_runtime->write_summaries(")
foreach(_task7c_boundary IN ITEMS
        _task7c_decision_begin _task7c_summary_begin)
    stage11d_count_raw_token("${_stage11d_host_direct}"
        "${${_task7c_boundary}}" _task7c_boundary_count)
    if(NOT _task7c_boundary_count EQUAL 1)
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard cannot isolate the Host validation exit boundary")
    endif()
endforeach()
string(FIND "${_stage11d_host_direct}" "${_task7c_decision_begin}"
    _task7c_decision_begin_position)
string(FIND "${_stage11d_host_direct}" "${_task7c_summary_begin}"
    _task7c_summary_begin_absolute)
stage11d_code_brace_depth("${_stage11d_host_direct}"
    ${_task7c_decision_begin_position} _task7c_decision_depth)
stage11d_code_brace_depth("${_stage11d_host_direct}"
    ${_task7c_summary_begin_absolute} _task7c_summary_depth)
string(SUBSTRING "${_stage11d_host_direct}"
    ${_task7c_decision_begin_position} -1 _task7c_decision_tail)
string(FIND "${_task7c_decision_tail}" "${_task7c_summary_begin}"
    _task7c_summary_begin_position)
if(_task7c_summary_begin_position EQUAL -1)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard cannot isolate the Host validation exit boundary")
endif()
string(SUBSTRING "${_task7c_decision_tail}" 0
    ${_task7c_summary_begin_position} _task7c_exit_surface)
string(REGEX REPLACE "[ \t\r\n]+" "" _task7c_exit_normalized
    "${_task7c_exit_surface}")
set(_task7c_validation_exit
    "if(decision.validation_complete&&(!config.validation_capture_file.has_value()||effective_generic_complete)){begin_clean_exit();}")
string(FIND "${_task7c_exit_normalized}" "${_task7c_validation_exit}"
    _task7c_validation_exit_position)
set(_task7c_frame_exit
    "if(config.validation_exit_after_presented_frames!=0U&&presented_frame_count>=config.validation_exit_after_presented_frames){begin_clean_exit();}")
string(FIND "${_task7c_exit_normalized}" "${_task7c_frame_exit}"
    _task7c_frame_exit_position)
set(_task7c_canonical_tail
    "validation_runtime->observe_capture_result(selected_capture_owner,selected_validation_capture_succeeded);++presented_frame_count;constbooleffective_generic_complete=decision.generic_capture_complete||selected_generic_capture_succeeded_now;${_task7c_frame_exit}${_task7c_validation_exit}")
string(FIND "${_task7c_exit_normalized}" "${_task7c_canonical_tail}"
    _task7c_canonical_tail_position)
string(FIND "${_stage11d_host_direct}"
    "validation_runtime->observe_capture_result("
    _task7c_canonical_tail_absolute)
if(_task7c_canonical_tail_absolute EQUAL -1)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected the Host capture-result tail")
endif()
stage11d_code_brace_depth("${_stage11d_host_direct}"
    ${_task7c_canonical_tail_absolute} _task7c_canonical_tail_depth)
stage11d_count_raw_token("${_task7c_exit_normalized}"
    "decision.validation_complete" _task7c_validation_complete_count)
stage11d_count_raw_token("${_task7c_exit_normalized}"
    "begin_clean_exit();" _task7c_clean_exit_count)
stage11d_count_raw_token("${_task7c_exit_normalized}"
    "begin_clean_exit" _task7c_clean_exit_identifier_count)
stage11d_count_raw_token("${_task7c_exit_normalized}"
    "presented_frame_count" _task7c_presented_frame_count)
string(FIND "${_task7c_exit_normalized}"
    "constbooleffective_generic_complete=decision.generic_capture_complete||selected_generic_capture_succeeded_now;"
    _task7c_effective_complete_binding)
foreach(_task7c_forbidden_exit IN ITEMS
        "exit_requested" "request_clean_shutdown" "CloseWindow("
        "break;" "continue;" "return" "goto" "throw"
        "std::exit(" "std::quick_exit(" "std::terminate("
        "abort(" "_Exit(" "ExitProcess(")
    stage11d_count_raw_token("${_task7c_exit_normalized}"
        "${_task7c_forbidden_exit}" _task7c_forbidden_exit_count)
    if(NOT _task7c_forbidden_exit_count EQUAL 0)
        message(FATAL_ERROR
            "T7C-M24: Stage11D loot evidence guard rejected an alternate Host exit path")
    endif()
endforeach()
if(_task7c_validation_exit_position EQUAL -1
        OR _task7c_frame_exit_position EQUAL -1
        OR _task7c_canonical_tail_position EQUAL -1
        OR NOT _task7c_decision_depth EQUAL 3
        OR NOT _task7c_summary_depth EQUAL 2
        OR NOT _task7c_canonical_tail_depth EQUAL 3
        OR NOT _task7c_validation_complete_count EQUAL 1
        OR NOT _task7c_clean_exit_count EQUAL 2
        OR NOT _task7c_clean_exit_identifier_count EQUAL 2
        OR NOT _task7c_presented_frame_count EQUAL 4
        OR _task7c_effective_complete_binding EQUAL -1)
    message(FATAL_ERROR
        "T7C-M24: Stage11D loot evidence guard rejected a Host bypass of decision.validation_complete")
endif()

foreach(_forbidden_host_owner IN ITEMS
        "stage11d_validation_state"
        "Stage11DLootValidationState"
        "stage11d_target_visible("
        "stage11d_record_semantics("
        "write_stage11d_loot_validation_summary(")
    string(FIND "${_stage11d_host_active}" "${_forbidden_host_owner}"
        _host_owner_found)
    if(NOT _host_owner_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected Host-owned Stage11D presentation state")
    endif()
endforeach()

if(DEFINED STAGE11D_TASK7C_ONLY AND STAGE11D_TASK7C_ONLY)
    message(STATUS "Stage11D Task7C host validation boundary guard passed")
    return()
endif()
foreach(_required IN ITEMS
        "state.snapshot_item_ids[index] = item.item_id;"
        "state.inventory_item_ids[state.inventory_item_count++] = item.id;")
    string(FIND "${_report_record_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report recorder semantic: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS
        "state.max_defeated_monsters ="
        "state.monster_blueprint_hash = snapshot.monster_blueprint_hash;"
        "state.monster_damage_observed = true;"
        "state.observed_drop_distance_milli[distance_index] ="
        "state.pickup_commit_generation = status.loot_pickup.commit_generation;")
    string(FIND "${_report_target_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report evaluator semantic: ${_required}")
    endif()
endforeach()

stage11d_extract_runtime_definition("rare abyss ground selector"
    "const dungeon::GroundItemSnapshot*\nstage11d_rare_abyss_ground("
    _rare_abyss_ground_text)
stage11d_mask_non_direct_executable_scopes("${_rare_abyss_ground_text}"
    _rare_abyss_ground_active)
string(REGEX REPLACE "[ \t\r\n]+" "" _rare_abyss_ground_normalized
    "${_rare_abyss_ground_active}")
foreach(_required IN ITEMS
        "item.source!=dungeon::GroundItemSource::abyss_chest"
        "item.rarity==items::ItemRarity::rare"
        "state.abyss_item_id==0U||item.item_id==state.abyss_item_id"
        "return&item;"
        "returnnullptr;")
    string(FIND "${_rare_abyss_ground_normalized}" "${_required}"
        _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected rare abyss ground selector")
    endif()
endforeach()

stage11d_extract_runtime_definition("rare abyss outer sweep default"
    "bool stage11d_outer_sweep_route_is_default("
    _outer_sweep_default_text)
stage11d_mask_non_direct_executable_scopes("${_outer_sweep_default_text}"
    _outer_sweep_default_active)
string(REGEX REPLACE "[ \t\r\n]+" "" _outer_sweep_default_normalized
    "${_outer_sweep_default_active}")
foreach(_required IN ITEMS
        "returnroute.phase==Stage10GridRoutePhase::need_join"
        "route.boundary_column==0U"
        "route.route_rejoins==0U"
        "!route.pending_movement_progress_check"
        "route.previous_position.x==0.0F"
        "route.previous_position.y==0.0F"
        "route.previous_position.z==0.0F;")
    string(FIND "${_outer_sweep_default_normalized}" "${_required}"
        _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected outer sweep route ownership")
    endif()
endforeach()

stage11d_extract_runtime_definition("rare abyss sweep cursor initializer"
    "void initialize_stage11d_sweep_cursor("
    _sweep_cursor_initializer_text)
stage11d_mask_non_direct_executable_scopes("${_sweep_cursor_initializer_text}"
    _sweep_cursor_initializer_active)
string(REGEX REPLACE "[ \t\r\n]+" "" _sweep_cursor_initializer_normalized
    "${_sweep_cursor_initializer_active}")
foreach(_required IN ITEMS
        "if(state.sweep_cursor_initialized)return;"
        "state.sweep_cursor_initialized=true;"
        "if(state.sweep_waypoint!=0U||!stage11d_outer_sweep_route_is_default(state.sweep_grid)){return;}"
        "constexprstd::size_twaypoint_count=combat::room_spatial::rows*2U;"
        "combat::Vec3target=stage10_validation_sweep_waypoint(0U);"
        "for(std::size_tindex=1U;index<waypoint_count;++index)"
        "target=stage10_validation_sweep_waypoint(static_cast<std::uint8_t>(index));"
        "constfloatdistance=x*x+y*y;"
        "if(distance<best_distance){best_distance=distance;nearest=index;}"
        "state.sweep_waypoint=static_cast<std::uint8_t>(nearest);")
    string(FIND "${_sweep_cursor_initializer_normalized}" "${_required}"
        _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected nearest sweep cursor initialization")
    endif()
endforeach()

stage11d_extract_runtime_definition("physical driver"
    "PhysicalKeySnapshot inject_stage11d_physical_edges(" _driver_text)
stage11d_require_unique_token_depth("runtime physical driver" "${_driver_text}"
    "using Scenario = Stage11DLootValidationScenario;" 1)
stage11d_require_unique_token_depth("runtime physical suspension gate"
    "${_driver_text}" "state.suspend_injection" 1)
stage11d_require_unique_token_depth("runtime rare abyss availability gate"
    "${_driver_text}"
    "stage11d_rare_abyss_player_available(combat_state)" 2)
stage11d_count_raw_token("${_driver_text}"
    "inject_stage11d_rare_abyss_area_skill(snapshot, current)"
    _driver_area_skill_dispatch_count)
if(NOT _driver_area_skill_dispatch_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected runtime rare abyss skill dispatch inventory")
endif()
stage11d_require_unique_token_depth("runtime rare abyss melee fallback"
    "${_driver_text}" "state.abyss_ranged.close_for_light = true;" 3)
foreach(_required IN ITEMS
        "inject_validation_pressed(" "inject_validation_action("
        "inject_validation_movement(" "validation_movement_toward("
        "validation_route_fire_movement("
        "stage11d_safe_movement_toward(" "stage11d_attack_lane("
        "state.suspend_injection" "stage10_validation_sweep_movement("
        "state.sweep_grid" "state.sweep_waypoint"
        "stage11d_rare_abyss_player_available("
        "plan_abyss_ranged()"
        "stage11d_rare_abyss_danger_near_player("
        "stage10_validation_release_ranged_target("
        "state.abyss_ranged"
        "inject_stage11d_rare_abyss_area_skill(")
    string(FIND "${_driver_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime physical driver token: ${_required}")
    endif()
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _driver_normalized "${_driver_text}")
stage11d_extract_direct_braced_statement("rare abyss reward approach"
    "${_driver_text}"
    "if (config.stage11d_loot_validation == Scenario::rare_only_abyss" 1
    _rare_abyss_reward_approach)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _rare_abyss_reward_approach_normalized
    "${_rare_abyss_reward_approach}")
set(_rare_abyss_reward_approach_chain
    "if(config.stage11d_loot_validation==Scenario::rare_only_abyss&&current.combat.has_value()){constauto*item=stage11d_rare_abyss_ground(current,state);if(item!=nullptr){combat::MovementInputmovement=validation_movement_toward(current.combat->player.position,item->position);if(current.ecology==dungeon::checkpoint::DungeonElement::fire){movement=validation_route_fire_movement(current.combat->player.position,item->position,movement);}inject_validation_movement(snapshot,settings_data,movement);state.abyss_claim_requested=state.captured;returnsnapshot;}if(state.captured)returnsnapshot;}")
if(NOT _rare_abyss_reward_approach_normalized STREQUAL
        _rare_abyss_reward_approach_chain)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires uncaptured abyss rewards to use physical approach")
endif()
string(FIND "${_driver_normalized}"
    "if(config.stage11d_loot_validation==Scenario::none||snapshot.focus_lost||state.suspend_injection){returnsnapshot;}"
    _driver_suspension_gate)
if(_driver_suspension_gate EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input driver chain")
endif()
stage11d_extract_direct_braced_statement("rare abyss branch"
    "${_driver_text}" "if (aggressive_abyss) {" 1
    _aggressive_driver_text)
foreach(_required_branch_token IN ITEMS
        "stage11d_rare_abyss_player_available(combat_state)"
        "inject_stage11d_rare_abyss_area_skill(snapshot, current)"
        "state.abyss_ranged.close_for_light = true;")
    string(FIND "${_aggressive_driver_text}"
        "${_required_branch_token}" _required_branch_position)
    if(_required_branch_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected rare abyss branch owner")
    endif()
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _aggressive_driver_normalized
    "${_aggressive_driver_text}")
stage11d_count_raw_token("${_aggressive_driver_text}"
    "initialize_stage11d_sweep_cursor("
    _aggressive_sweep_cursor_call_count)
if(NOT _aggressive_sweep_cursor_call_count EQUAL 4)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected sweep cursor handoff inventory")
endif()
stage11d_extract_direct_braced_statement("rare abyss sweep opportunity"
    "${_aggressive_driver_text}"
    "if (!low_health && state.abyss_ranged.sweep_escape" 1
    _aggressive_sweep_opportunity)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _aggressive_sweep_opportunity_normalized
    "${_aggressive_sweep_opportunity}")
foreach(_required IN ITEMS
        "if(!low_health&&state.abyss_ranged.sweep_escape&&!state.abyss_ranged.recovery_target_valid){"
        "if(inject_stage11d_rare_abyss_area_skill(snapshot,current)==Stage11DRareAbyssSkillInput::injected){returnsnapshot;}"
        "if(!monster.active||monster.hp<=0||!validation_attack_lane(combat_state,monster)){continue;}"
        "inject_validation_action(snapshot,settings_data,settings::SettingAction::light_attack,true);returnsnapshot;")
    string(FIND "${_aggressive_sweep_opportunity_normalized}" "${_required}"
        _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected high-health sweep opportunity input")
    endif()
endforeach()
foreach(_required_branch_chain IN ITEMS
        "if(!stage11d_rare_abyss_player_available(combat_state)){returnsnapshot;}"
        "Stage10RangedValidationPlanplan=plan_abyss_ranged();"
        "constcombat::MonsterSnapshot*target=stage11d_monster_by_ordinal(combat_state,plan.target_ordinal);"
        "if(state.abyss_ranged.melee_chain&&validation_attack_lane(combat_state,*target)){inject_validation_action(snapshot,settings_data,settings::SettingAction::light_attack,true);returnsnapshot;}"
        "if(skill_input==Stage11DRareAbyssSkillInput::injected){returnsnapshot;}")
    string(FIND "${_aggressive_driver_normalized}"
        "${_required_branch_chain}" _required_branch_chain_position)
    if(_required_branch_chain_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected physical-input driver chain")
    endif()
endforeach()
foreach(_required_handoff_chain IN ITEMS
        "constautoinner_global_sweep_active=[&state]()noexcept{returnstate.abyss_ranged.sweep_escape&&!state.abyss_ranged.recovery_target_valid;};"
        "constboolinner_was_global=inner_global_sweep_active();"
        "Stage10RangedValidationPlannext=stage10_validation_ranged_plan(combat_state,state.abyss_ranged);"
        "constboolinner_took_global=!inner_was_global&&inner_is_global;"
        "if(inner_took_global){initialize_stage11d_sweep_cursor(state,combat_state.player.position);state.abyss_ranged.sweep_waypoint=state.sweep_waypoint;state.abyss_ranged.sweep_grid={};state.sweep_grid={};next=stage10_validation_ranged_plan(combat_state,state.abyss_ranged);inner_is_global=inner_global_sweep_active();}"
        "if((inner_was_global||inner_took_global)&&!inner_is_global){state.sweep_waypoint=state.abyss_ranged.sweep_waypoint;}"
        "if(state.abyss_ranged.sweep_escape||next.target_ordinal!=combat::kInvalidMonsterOrdinal){state.sweep_grid={};}"
        "returnnext;")
    string(FIND "${_aggressive_driver_normalized}"
        "${_required_handoff_chain}" _required_handoff_position)
    if(_required_handoff_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected dual sweep cursor ownership")
    endif()
endforeach()

stage11d_extract_direct_braced_statement("physical-input driver chain"
    "${_aggressive_driver_text}"
    "if (target != nullptr" 1
    _aggressive_danger_block)
string(REGEX REPLACE "[ \t\r\n]+" "" _aggressive_danger_normalized
    "${_aggressive_danger_block}")
string(FIND "${_aggressive_danger_normalized}"
    "if(target!=nullptr&&low_health&&!validation_attack_lane(combat_state,*target)&&stage11d_rare_abyss_danger_near_player("
    _aggressive_danger_gate_position)
if(_aggressive_danger_gate_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input driver chain")
endif()
foreach(_required_danger_token IN ITEMS
        "low_health"
        "stage11d_rare_abyss_danger_near_player("
        "stage10_validation_release_ranged_target("
        "state.abyss_ranged.stalled_target_ordinal = threatened_target;"
        "state.abyss_ranged.sweep_escape = true;"
        "plan = plan_abyss_ranged();"
        "target = stage11d_monster_by_ordinal(")
    string(FIND "${_aggressive_danger_block}"
        "${_required_danger_token}" _required_danger_position)
    if(_required_danger_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected physical-input driver chain")
    endif()
endforeach()

stage11d_extract_direct_braced_statement("physical-input driver chain"
    "${_aggressive_driver_text}" "if (target == nullptr) {" 1
    _aggressive_no_target_block)
foreach(_required_no_target_token IN ITEMS
        "plan.movement"
        "plan.movement_target_valid"
        "plan.movement_target"
        "stage10_validation_sweep_movement("
        "stage10_validation_sweep_waypoint("
        "validation_route_fire_movement("
        "inject_validation_movement(")
    string(FIND "${_aggressive_no_target_block}"
        "${_required_no_target_token}" _required_no_target_position)
    if(_required_no_target_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected physical-input driver chain")
    endif()
endforeach()
stage11d_extract_direct_braced_statement("physical-input recovery route"
    "${_aggressive_no_target_block}"
    "if (plan.movement.x != 0 || plan.movement.y != 0) {" 1
    _aggressive_recovery_block)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _aggressive_recovery_normalized "${_aggressive_recovery_block}")
set(_aggressive_recovery_chain
    "if(plan.movement.x!=0||plan.movement.y!=0){combat::MovementInputrecovery=plan.movement;if(current.ecology==dungeon::checkpoint::DungeonElement::fire&&plan.movement_target_valid){recovery=validation_route_fire_movement(combat_state.player.position,plan.movement_target,recovery);}inject_validation_movement(snapshot,settings_data,recovery);returnsnapshot;}")
if(NOT _aggressive_recovery_normalized STREQUAL
        _aggressive_recovery_chain)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input recovery route")
endif()
string(FIND "${_aggressive_no_target_block}"
    "stage10_validation_sweep_movement(" _global_sweep_position)
if(_global_sweep_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input driver chain")
endif()
string(SUBSTRING "${_aggressive_no_target_block}"
    ${_global_sweep_position} -1 _global_sweep_tail)
string(FIND "${_global_sweep_tail}"
    "stage10_validation_sweep_waypoint(" _global_waypoint_position)
string(FIND "${_global_sweep_tail}"
    "validation_route_fire_movement(" _global_fire_route_position)
string(FIND "${_global_sweep_tail}"
    "inject_validation_movement(" _global_injection_position)
if(_global_waypoint_position EQUAL -1
        OR _global_fire_route_position EQUAL -1
        OR _global_injection_position EQUAL -1
        OR NOT _global_fire_route_position LESS _global_waypoint_position
        OR NOT _global_waypoint_position LESS _global_injection_position)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input driver chain")
endif()

stage11d_extract_direct_braced_statement("physical-input movement return"
    "${_aggressive_driver_text}"
    "if (movement.x != 0 || movement.y != 0\n                || !plan.stance_reached || !plan.facing_target) {" 1
    _aggressive_movement_return_block)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _aggressive_movement_return_normalized
    "${_aggressive_movement_return_block}")
set(_aggressive_movement_return_chain
    "if(movement.x!=0||movement.y!=0||!plan.stance_reached||!plan.facing_target){if(state.abyss_ranged.close_for_light&&(movement.x!=0||movement.y!=0)&&inject_stage11d_rare_abyss_area_skill(snapshot,current)==Stage11DRareAbyssSkillInput::injected){returnsnapshot;}returnsnapshot;}")
if(NOT _aggressive_movement_return_normalized STREQUAL
        _aggressive_movement_return_chain)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input movement return")
endif()

stage11d_extract_direct_braced_statement("physical-input driver chain"
    "${_aggressive_driver_text}"
    "if (skill_input == Stage11DRareAbyssSkillInput::no_geometry) {" 1
    _aggressive_no_geometry_block)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _aggressive_no_geometry_normalized "${_aggressive_no_geometry_block}")
if(NOT _aggressive_no_geometry_normalized STREQUAL
        "if(skill_input==Stage11DRareAbyssSkillInput::no_geometry){stage10_validation_release_ranged_target(state.abyss_ranged);returnsnapshot;}")
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical-input driver chain")
endif()

stage11d_extract_direct_braced_statement("physical-input driver chain"
    "${_aggressive_driver_text}"
    "if (skill_input == Stage11DRareAbyssSkillInput::unavailable" 1
    _aggressive_melee_fallback_block)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _aggressive_melee_fallback_normalized
    "${_aggressive_melee_fallback_block}")
foreach(_required_fallback_token IN ITEMS
        "if(skill_input==Stage11DRareAbyssSkillInput::unavailable||skill_input==Stage11DRareAbyssSkillInput::waiting){"
        "state.abyss_ranged.melee_chain=true;"
        "state.abyss_ranged.close_for_light=true;"
        "plan_abyss_ranged()"
        "validation_route_fire_movement("
        "inject_validation_movement(")
    string(FIND "${_aggressive_melee_fallback_normalized}"
        "${_required_fallback_token}" _required_fallback_position)
    if(_required_fallback_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected physical-input driver chain")
    endif()
endforeach()
stage11d_count_raw_token("${_aggressive_driver_text}"
    "active_skill_slots" _driver_skill_access_count)
if(NOT _driver_skill_access_count EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected rare abyss direct skill-slot access")
endif()
foreach(_forbidden IN ITEMS
        ".request_active_skill_slot(" ".queue_action("
        "request_pickup(" "complete_pickup(" "TestAccess"
        "inventory_count =" "renderer.draw(")
    string(FIND "${_driver_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected physical-driver bypass: ${_forbidden}")
    endif()
endforeach()
stage11d_extract_runtime_definition("rare abyss skill helper"
    "Stage11DRareAbyssSkillInput\ninject_stage11d_rare_abyss_area_skill("
    _rare_abyss_skill_text)
foreach(_required IN ITEMS
        "const dungeon::DungeonSnapshot& dungeon_state"
        "const combat::CombatSnapshot& combat_state = *dungeon_state.combat;"
        "const auto inject_equipped_skill ="
        "const std::size_t cooldown = static_cast<std::size_t>(skill);"
        "combat_state.skill_cooldowns[cooldown] != 0U"
        "slot < dungeon_state.skill_loadout.slots.size()"
        "dungeon_state.skill_loadout.slots[slot].active != skill"
        "snapshot.active_skill_slots[slot] = true;"
        "skills::ActiveSkillId::storm_swords"
        "skills::ActiveSkillId::draw_slash"
        "Stage11DRareAbyssSkillInput::no_geometry"
        "Stage11DRareAbyssSkillInput::unavailable"
        "Stage11DRareAbyssSkillInput::waiting"
        "Stage11DRareAbyssSkillInput::injected")
    string(FIND "${_rare_abyss_skill_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime rare abyss skill token: ${_required}")
    endif()
endforeach()
string(REGEX REPLACE "[ \t\r\n]+" "" _rare_abyss_skill_normalized
    "${_rare_abyss_skill_text}")
stage11d_extract_direct_braced_statement("rare abyss storm success return"
    "${_rare_abyss_skill_text}"
    "if (storm == Stage11DEquippedSkillInput::injected) {" 2
    _rare_abyss_storm_success_block)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _rare_abyss_storm_success_normalized
    "${_rare_abyss_storm_success_block}")
if(NOT _rare_abyss_storm_success_normalized STREQUAL
        "if(storm==Stage11DEquippedSkillInput::injected){returnStage11DRareAbyssSkillInput::injected;}")
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected rare abyss storm success return")
endif()
stage11d_extract_direct_braced_statement("rare abyss draw success return"
    "${_rare_abyss_skill_text}"
    "if (draw == Stage11DEquippedSkillInput::injected) {" 2
    _rare_abyss_draw_success_block)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _rare_abyss_draw_success_normalized
    "${_rare_abyss_draw_success_block}")
if(NOT _rare_abyss_draw_success_normalized STREQUAL
        "if(draw==Stage11DEquippedSkillInput::injected){returnStage11DRareAbyssSkillInput::injected;}")
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected rare abyss draw success return")
endif()
stage11d_count_raw_token("${_rare_abyss_skill_text}"
    "return Stage11DRareAbyssSkillInput::injected;"
    _rare_abyss_injected_return_count)
if(NOT _rare_abyss_injected_return_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected rare abyss injected return inventory")
endif()
set(_rare_abyss_equipped_slot_chain
    "for(std::size_tslot=0U;slot<dungeon_state.skill_loadout.slots.size();++slot){if(dungeon_state.skill_loadout.slots[slot].active!=skill){continue;}if(combat_state.skill_cooldowns[cooldown]!=0U){returnStage11DEquippedSkillInput::waiting;}snapshot.active_skill_slots[slot]=true;returnStage11DEquippedSkillInput::injected;}")
string(FIND "${_rare_abyss_skill_normalized}"
    "${_rare_abyss_equipped_slot_chain}" _rare_abyss_equipped_slot_found)
if(_rare_abyss_equipped_slot_found EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected the equipped physical skill-slot chain")
endif()
stage11d_count_raw_token("${_rare_abyss_skill_text}"
    "snapshot.active_skill_slots[" _rare_abyss_skill_write_count)
if(NOT _rare_abyss_skill_write_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected rare abyss physical skill write inventory")
endif()
stage11d_require_unique_token_depth("rare abyss equipped skill write"
    "${_rare_abyss_skill_text}"
    "snapshot.active_skill_slots[slot] = true;" 3)
foreach(_required_chain IN ITEMS
        "unavailable=unavailable||storm==Stage11DEquippedSkillInput::absent;"
        "unavailable=unavailable||draw==Stage11DEquippedSkillInput::absent;"
        "for(constauto&slot:dungeon_state.skill_loadout.slots){area_skill_equipped=area_skill_equipped||slot.active==skills::ActiveSkillId::storm_swords||slot.active==skills::ActiveSkillId::draw_slash;}"
        "returnwaiting?Stage11DRareAbyssSkillInput::waiting:(unavailable||!area_skill_equipped)?Stage11DRareAbyssSkillInput::unavailable:Stage11DRareAbyssSkillInput::no_geometry;")
    string(FIND "${_rare_abyss_skill_normalized}"
        "${_required_chain}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected unavailable-skill fallback")
    endif()
endforeach()
foreach(_forbidden IN ITEMS
        "snapshot.active_skill_slots[1]"
        "snapshot.active_skill_slots[0]"
        "snapshot.active_skill_slots[1U]"
        "snapshot.active_skill_slots[0U]"
        "combat_state.skill_cooldowns[1]"
        "combat_state.skill_cooldowns[0]"
        "combat_state.skill_cooldowns[1U]"
        "combat_state.skill_cooldowns[0U]")
    string(FIND "${_rare_abyss_skill_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected hard-coded rare abyss skill slot: ${_forbidden}")
    endif()
endforeach()
stage11d_extract_runtime_definition("rare abyss danger"
    "bool stage11d_rare_abyss_danger_near_player(" _rare_abyss_danger_text)
foreach(_required IN ITEMS
        "!monster.active || monster.hp <= 0"
        "monster.reaction != combat::ReactionState::idle"
        "monster.ai_phase == combat::MonsterAiPhase::recovery"
        "monster.ai_phase == combat::MonsterAiPhase::cooldown"
        "monster.ai_phase == combat::MonsterAiPhase::defeated"
        "x * x + y * y <= radius_squared")
    string(FIND "${_rare_abyss_danger_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing rare abyss danger token: ${_required}")
    endif()
endforeach()
stage11d_extract_runtime_definition("rare abyss availability"
    "bool stage11d_rare_abyss_player_available(" _rare_abyss_ready_text)
foreach(_required IN ITEMS
        "player.hp > 0" "player.hurt_ticks == 0U"
        "player.hit_stop_ticks == 0U"
        "player.active_attack == combat::AttackId::none"
        "state.active_skill.id == skills::ActiveSkillId::none"
        "state.diagnostics.input_size == 0U")
    string(FIND "${_rare_abyss_ready_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing rare abyss availability token: ${_required}")
    endif()
endforeach()
foreach(_forbidden IN ITEMS
        ".request_active_skill_slot(" ".queue_action("
        "request_pickup(" "complete_pickup(")
    string(FIND "${_runtime_code}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected runtime input bypass: ${_forbidden}")
    endif()
endforeach()
string(REGEX MATCH
    "ground_items[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*=[^=]"
    _ground_mutation "${_runtime_code}")
if(_ground_mutation)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected snapshot mutation")
endif()
if(DEFINED STAGE11D_ABYSS_PHYSICAL_ONLY AND STAGE11D_ABYSS_PHYSICAL_ONLY)
    message(STATUS "Stage11D rare-abyss physical-input guard passed")
    return()
endif()

stage11d_extract_runtime_definition("fixed-step activation"
    "bool stage11d_validation_active(" _fixed_step_function)
stage11d_require_unique_token_depth("runtime fixed-step activation"
    "${_fixed_step_function}"
    "return config.stage11d_loot_validation" 1)
stage11d_extract_runtime_definition("abyss observer"
    "void observe_stage11d_abyss_claim(" _abyss_function)
stage11d_require_unique_token_depth("runtime abyss observer"
    "${_abyss_function}" "if (state.abyss_claim_requested)" 1)
stage11d_require_unique_token_depth("runtime abyss observer assignment"
    "${_abyss_function}" "state.abyss_claimed =" 2)
foreach(_required IN ITEMS
        "current.ground_items[index].item_id" "item_state.items"
        "!still_ground && now_owned")
    string(FIND "${_abyss_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime abyss observer token: ${_required}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "Stage11DLootValidationScenario" "show_all" "magic_or_better"
        "rare_only" "rare_only_abyss" "preview_cancel" "pickup_feedback"
        "platform::run_raylib_host(config)" "persistence::SaveStore"
        "settings::SettingsStore" "settings_store.save("
        "present_frame_and_maybe_capture" "stage11d_record_semantics"
        "renderer.draw(" "pickup_commit_generation" "drop_distance_milli"
        "build_room_monster_plan(" "kRoomMonsterGeneratorVersion"
        "monster_blueprint_hash" "ordinary_generator_version"
        "ordinary_initial_residents"
        "ordinary_drop_ordinals" "ordinary_drop_item_ids")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing production token: ${_required}")
    endif()
endforeach()
foreach(_forbidden IN ITEMS "build_encounter_plan(" "--search-ordinary")
    string(FIND "${_formal_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected obsolete fixture token: ${_forbidden}")
    endif()
endforeach()

string(FIND "${_header_text}" "enum class Stage11DLootValidationScenario" _enum_begin)
if(_enum_begin EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate scenario enum")
endif()
string(SUBSTRING "${_header_text}" ${_enum_begin} -1 _enum_tail)
string(FIND "${_enum_tail}" "};" _enum_end)
if(_enum_end EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate scenario enum")
endif()
math(EXPR _enum_length "${_enum_end} + 2")
string(SUBSTRING "${_enum_tail}" 0 ${_enum_length} _enum_text)
string(REGEX MATCHALL "[ \t\r\n]([a-z][a-z0-9_]*)[ \t\r\n]*[,}]" _enum_values "${_enum_text}")
list(LENGTH _enum_values _enum_count)
if(NOT _enum_count EQUAL 7)
    message(FATAL_ERROR "Stage11D loot evidence guard requires none plus exactly six scenarios")
endif()

foreach(_forbidden IN ITEMS
        "TestAccess" "snapshot_override" "set_snapshot(" "FakeRenderer"
        "fake_renderer" "request_pickup(" "complete_pickup(" "publish_pickup("
        "pause_menu.committed.loot_filter_mode ="
        "live_settings.loot_filter_mode =" "ground_items[0] ="
        "inventory_count =" "result=pass" "LoadImageFromScreen()"
        "remove_all(")
    string(FIND "${_formal_text}" "${_forbidden}" _formal_found)
    if(NOT _formal_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard rejected formal bypass: ${_forbidden}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "allowed_evidence_root(" "reset_evidence_root("
        "absolute.filename() != \"stage11d loot evidence\""
        "absolute == absolute.root_path()" "temp_directory_path("
        "/out/build/" "has_link_or_reparse_component("
        "FILE_ATTRIBUTE_REPARSE_POINT" "weakly_canonical("
        "remove_known_file(root, root / spec.image)"
        "remove_known_file(root, root / spec.summary)"
        "--root-safety-self-test"
        "stage11d-root-safety-sentinel.unknown"
        "return sentinel_preserved && known_removed")
    string(FIND "${_formal_text}" "${_required}" _cleanup_found)
    if(_cleanup_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D formal cleanup safety is incomplete: ${_required}")
    endif()
endforeach()

set(_semantic_ownership_text "${_host_text}\n${_report_text}")
foreach(_forbidden IN ITEMS
        "TestAccess" "snapshot_override" "set_snapshot(" "FakeRenderer"
        "fake_renderer" "request_pickup(" "complete_pickup("
        "publish_pickup(")
    string(FIND "${_semantic_ownership_text}" "${_forbidden}" _host_found)
    if(NOT _host_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D host-bypass-${_forbidden}")
    endif()
endforeach()

string(REGEX MATCH
    "pause_menu[ \t\r\n]*[.][ \t\r\n]*committed([^=;]*|)[=][^=]"
    _committed_assignment "${_stage11d_semantic_code}")
if(_committed_assignment)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct committed settings write")
endif()
string(REGEX MATCH "live_settings([^=;]*|)[=][^=]"
    _live_assignment "${_stage11d_semantic_code}")
if(_live_assignment)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct live settings write")
endif()
string(REGEX MATCH "result[ \t\r\n]*=[ \t\r\n]*pass"
    _direct_pass "${_stage11d_semantic_seam}")
if(_direct_pass)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct result pass")
endif()

string(REGEX REPLACE "[ \t\r\n]+" "" _host_normalized "${_host_text}")
stage11d_fold_cpp_phase2_splices(
    "${_settings_runtime_text}" _settings_runtime_phase2)
stage11d_unconditional_cpp_surface(
    "${_settings_runtime_phase2}" _settings_runtime_active
    _settings_runtime_lexical)
if(_settings_runtime_lexical MATCHES
        "(^|\n)[ \t]*(#|%:)[ \t]*(define|undef)([ \t\r\n]|$)")
    message(FATAL_ERROR
        "Stage11D loot evidence guard forbids settings runtime preprocessor macros")
endif()
set(_settings_settle_signature "bool HostSettingsRuntime::settle(")
stage11d_count_raw_token("${_settings_runtime_active}"
    "${_settings_settle_signature}" _settings_settle_active_count)
stage11d_count_raw_token("${_settings_runtime_lexical}"
    "${_settings_settle_signature}" _settings_settle_lexical_count)
if(NOT _settings_settle_active_count EQUAL 1
        OR NOT _settings_settle_lexical_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires one active and lexical settings settlement owner")
endif()
foreach(_settings_owner_surface IN ITEMS
        _settings_runtime_active _settings_runtime_lexical)
    string(FIND "${${_settings_owner_surface}}"
        "${_settings_settle_signature}" _settings_settle_position)
    stage11d_code_brace_depth("${${_settings_owner_surface}}"
        ${_settings_settle_position} _settings_settle_depth)
    if(NOT _settings_settle_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires a namespace-level settings settlement owner")
    endif()
endforeach()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_settings_runtime_active}" "${_settings_settle_signature}"
    _settings_settle_begin _settings_settle_open _settings_settle_end)
math(EXPR _settings_settle_length
    "${_settings_settle_end} - ${_settings_settle_begin} + 1")
string(SUBSTRING "${_settings_runtime_active}" ${_settings_settle_begin}
    ${_settings_settle_length} _settings_settle_function)
stage11d_mask_non_direct_executable_scopes(
    "${_settings_settle_function}" _settings_settle_direct)
string(REGEX REPLACE "[ \t\r\n]+" "" _settings_runtime_normalized
    "${_settings_runtime_active}")
string(REGEX REPLACE "[ \t\r\n]+" "" _settings_settle_normalized
    "${_settings_settle_direct}")
string(REGEX MATCHALL
    "this->live->loot_filter_mode=this->pause_menu->committed[.]loot_filter_mode"
    _canonical_live_settings_writes "${_settings_settle_normalized}")
list(LENGTH _canonical_live_settings_writes _canonical_live_settings_count)
if(NOT _canonical_live_settings_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires exactly two canonical production live-settings rollback writes")
endif()
string(REGEX MATCHALL "this->live->loot_filter_mode="
    _all_live_filter_writes "${_settings_settle_normalized}")
list(LENGTH _all_live_filter_writes _all_live_filter_write_count)
if(NOT _all_live_filter_write_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected extra live-settings filter write")
endif()
string(REGEX MATCHALL "([.]|->)loot_filter_mode="
    _settings_settle_filter_assignments "${_settings_settle_normalized}")
list(LENGTH _settings_settle_filter_assignments
    _settings_settle_filter_assignment_count)
if(NOT _settings_settle_filter_assignment_count EQUAL 6)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires exactly six production settings filter assignments")
endif()
string(REGEX MATCHALL "([.]|->)loot_filter_mode="
    _all_active_runtime_filter_assignments "${_settings_runtime_normalized}")
list(LENGTH _all_active_runtime_filter_assignments
    _all_active_runtime_filter_assignment_count)
if(NOT _all_active_runtime_filter_assignment_count EQUAL 6)
    message(FATAL_ERROR
        "Stage11D loot evidence guard forbids settings writes outside settlement")
endif()
string(SHA256 _settings_settle_contract_sha256
    "${_settings_settle_normalized}")
set(_expected_settings_settle_contract_sha256
    "e61f013c1387da1e64fcef26d696ce210132574d28a823edcb66dffde3c08f8e")
if(NOT _settings_settle_contract_sha256 STREQUAL
        _expected_settings_settle_contract_sha256)
    message(FATAL_ERROR
        "Stage11D settings settlement contract drifted: ${_settings_settle_contract_sha256}")
endif()
string(REGEX REPLACE "[ \t\r\n]+" "" _host_code_normalized
    "${_stage11d_host_active}")
string(REGEX MATCHALL "([.]|->)loot_filter_mode="
    _host_live_filter_writes "${_host_code_normalized}")
list(LENGTH _host_live_filter_writes _host_live_filter_write_count)
if(NOT _host_live_filter_write_count EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard forbids Host live-settings filter writes")
endif()
string(REPLACE ";" " " _host_identifier_surface
    "${_stage11d_host_active}")
string(REGEX MATCHALL "(^|[^A-Za-z0-9_])live_settings([^A-Za-z0-9_]|$)"
    _host_live_settings_identifiers "${_host_identifier_surface}")
list(LENGTH _host_live_settings_identifiers _host_live_settings_count)
if(NOT _host_live_settings_count EQUAL 6)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected Host live-settings alias/helper drift: ${_host_live_settings_count}")
endif()
string(REGEX MATCHALL "(^|[^A-Za-z0-9_])settings_runtime([^A-Za-z0-9_]|$)"
    _host_settings_runtime_identifiers "${_host_identifier_surface}")
list(LENGTH _host_settings_runtime_identifiers _host_settings_runtime_count)
if(NOT _host_settings_runtime_count EQUAL 3)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected Host settings-runtime helper drift: ${_host_settings_runtime_count}")
endif()
string(REGEX MATCHALL
    "(^|[^A-Za-z0-9_])HostSettingsRuntime([^A-Za-z0-9_]|$)"
    _host_settings_runtime_types "${_host_identifier_surface}")
list(LENGTH _host_settings_runtime_types _host_settings_runtime_type_count)
if(NOT _host_settings_runtime_type_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires exactly one Host settings coordinator")
endif()
foreach(_host_settings_context IN ITEMS
        "settings::SettingsDatalive_settings=committed_settings;"
        "HostSettingsRuntimesettings_runtime{&settings_notice,&pause_menu,&live_settings,&input_settings,&settings_store,settings_backend};"
        "audio_bus_levels(live_settings)"
        "loot_pickup_policy(live_settings.loot_filter_mode)"
        "?pause_menu.draft:live_settings;"
        "renderer_loot_filter_mode(pause_menu.screen,live_settings,pause_menu.draft)"
        "settings_runtime.consume_notice(pause_screen_before);"
        "settings_runtime.settle(")
    stage11d_count_raw_token("${_host_code_normalized}"
        "${_host_settings_context}" _host_settings_context_count)
    if(NOT _host_settings_context_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected Host settings context: ${_host_settings_context}")
    endif()
endforeach()

string(REGEX MATCH
    "ground_items[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*=[^=]"
    _host_ground_mutation "${_host_text}\n${_report_text}")
if(_host_ground_mutation)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected host snapshot mutation")
endif()

function(stage11d_require_ordered_host_tokens SURFACE EXPECTED_DEPTH)
    set(_previous -1)
    foreach(_token IN ITEMS ${ARGN})
        stage11d_count_raw_token("${SURFACE}" "${_token}" _count)
        if(NOT _count EQUAL 1)
            message(FATAL_ERROR
                "Stage11D loot evidence guard missing input stage: ${_token}")
        endif()
        string(FIND "${SURFACE}" "${_token}" _position)
        stage11d_code_brace_depth("${SURFACE}" ${_position} _depth)
        if(NOT _depth EQUAL EXPECTED_DEPTH)
            if(_token STREQUAL
                    "validation_runtime->inject_physical_edges(")
                message(FATAL_ERROR
                    "Stage11D loot evidence guard rejected host input call scope")
            endif()
            message(FATAL_ERROR
                "Stage11D loot evidence guard rejected host input chain scope: ${_token}")
        endif()
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR
                "Stage11D loot evidence guard rejected physical sample-map-submit order")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

stage11d_require_ordered_host_tokens("${_host_input_code}" 0
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "validation_runtime->inject_physical_edges("
    "HostFrameInput frame_input = map_host_frame_input(")
string(FIND "${_host_input_code}"
    "HostFrameInput frame_input = map_host_frame_input(" _host_map_position)
stage11d_count_raw_token("${_host_input_code}"
    "PauseCommand pause_command = update_pause_menu(" _host_pause_count)
string(FIND "${_host_input_code}"
    "PauseCommand pause_command = update_pause_menu(" _host_pause_position)
stage11d_count_raw_token("${_host_input_code}"
    "submit_frame_actions(*session, frame_input)" _host_submit_count)
string(FIND "${_host_input_code}"
    "submit_frame_actions(*session, frame_input)" _host_submit_position)
if(NOT _host_submit_count EQUAL 1 OR _host_submit_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing input stage: submit_frame_actions")
endif()
if(NOT _host_pause_count EQUAL 1 OR _host_pause_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing input stage: update_pause_menu")
endif()
stage11d_code_brace_depth("${_host_input_code}" ${_host_pause_position}
    _host_pause_depth)
if(NOT _host_pause_depth EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host pause call scope")
endif()
stage11d_code_brace_depth("${_host_input_code}" ${_host_submit_position}
    _host_submit_depth)
if(NOT _host_submit_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host submit scope")
endif()
if(_host_pause_position LESS _host_map_position
        OR _host_submit_position LESS _host_pause_position)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical sample-map-pause-submit order")
endif()

string(REGEX REPLACE "[ \t\r\n]+" "" _host_input_normalized
    "${_host_input_code}")
string(FIND "${_host_input_normalized}"
    "constPhysicalKeySnapshotstage17_physical_keys=validation_runtime->inject_physical_edges(sampled_physical_keys,input_settings,current,gameplay_rearm_was_required);"
    _host_input_binding)
if(_host_input_binding EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host input call binding")
endif()
stage11d_count_raw_token("${_host_input_code}"
    "inject_stage11d_physical_edges(" _host_direct_stage11d_input_count)
if(NOT _host_direct_stage11d_input_count EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host input call binding")
endif()

set(_host_validation_snapshot_signature
    "void HostValidationRuntime::observe_snapshot(")
stage11d_count_raw_token("${_host_validation_runtime_code}"
    "${_host_validation_snapshot_signature}"
    _host_validation_snapshot_count)
if(NOT _host_validation_snapshot_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing fixed-tick observation token: host_validation::observe_stage17_snapshot(")
endif()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_host_validation_runtime_code}"
    "${_host_validation_snapshot_signature}"
    _host_validation_snapshot_begin _host_validation_snapshot_open
    _host_validation_snapshot_end)
math(EXPR _host_validation_snapshot_length
    "${_host_validation_snapshot_end} - ${_host_validation_snapshot_begin} + 1")
string(SUBSTRING "${_host_validation_runtime_code}"
    ${_host_validation_snapshot_begin} ${_host_validation_snapshot_length}
    _host_validation_snapshot_function)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _host_validation_snapshot_normalized
    "${_host_validation_snapshot_function}")
set(_host_validation_snapshot_expected
    "voidHostValidationRuntime::observe_snapshot(constdungeon::DungeonSnapshot&snapshot)noexcept{host_validation::observe_stage17_snapshot(*impl_->config,impl_->states.stage17,snapshot);}")
if(NOT _host_validation_snapshot_normalized STREQUAL
        _host_validation_snapshot_expected)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing fixed-tick observation token: host_validation::observe_stage17_snapshot(")
endif()

stage11d_extract_marker_region("${_host_fixed_step_code}" host fixed_step
    _host_fixed_step_seam)
stage11d_require_unique_token_depth("host fixed-step activation call"
    "${_host_fixed_step_code}"
    "config.stage11d_loot_validation" 1)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_fixed_step_normalized
    "${_host_fixed_step_seam}")
string(FIND "${_host_fixed_step_normalized}"
    "||config.stage11d_loot_validation!=Stage11DLootValidationScenario::none"
    _host_fixed_step_call)
if(_host_fixed_step_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected fixed-step activation call")
endif()
string(FIND "${_host_text}"
    "host_validation::stage11d_validation_active" _private_activation_call)
if(NOT _private_activation_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard retained the private activation helper in Host")
endif()

stage11d_extract_marker_region("${_host_fixed_step_code}" host abyss_claim
    _host_abyss_seam)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_abyss_normalized
    "${_host_abyss_seam}")
string(FIND "${_host_abyss_normalized}"
    "validation_runtime->observe_post_fixed_tick(current,&session->item_state());"
    _host_abyss_call)
if(_host_abyss_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host abyss observer binding")
endif()
stage11d_count_raw_token("${_host_fixed_step_code}"
    "host_validation::observe_stage11d_abyss_claim("
    _host_direct_abyss_observer_count)
if(NOT _host_direct_abyss_observer_count EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected direct host abyss observer")
endif()

string(FIND "${_host_fixed_step_code}"
    "for (std::uint32_t step = 0; step < frame.steps; ++step) {"
    _host_fixed_tick_begin)
if(_host_fixed_tick_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host fixed-step loop")
endif()
string(SUBSTRING "${_host_fixed_step_code}" ${_host_fixed_tick_begin} -1
    _host_fixed_tick_loop)
set(_previous -1)
foreach(_token IN ITEMS
        "runtime.fixed_tick(" "session->snapshot(current);"
        "validation_runtime->observe_snapshot(current);"
        "validation_runtime->observe_post_fixed_tick("
        "drain_events(")
    stage11d_count_raw_token("${_host_fixed_tick_loop}" "${_token}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing fixed-tick observation token: ${_token}")
    endif()
    string(FIND "${_host_fixed_tick_loop}" "${_token}" _position)
    stage11d_code_brace_depth("${_host_fixed_tick_loop}" ${_position} _depth)
    if(NOT _depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host abyss observer scope")
    endif()
    if(NOT _previous EQUAL -1 AND _position LESS _previous)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected abyss claim observation order")
    endif()
    set(_previous ${_position})
endforeach()

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR NOT _present LESS _capture)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected pre-EndDrawing capture")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \t\r\n]*\\(" _capture_calls "${_host_text}")
list(LENGTH _capture_calls _capture_count)
if(NOT _capture_count EQUAL 1)
    message(FATAL_ERROR "Stage11D loot evidence guard requires one production capture helper")
endif()
string(FIND "${_host_text}" "make_combat_render_plan(" _host_second_plan)
if(NOT _host_second_plan EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected a second host render plan")
endif()
arpg_sanitize_cpp_source("${_renderer_text}" _renderer_code)
evidence_extract_cpp_function_block("${_renderer_text}"
    "CombatRenderPlan make_combat_render_plan(" _renderer_plan_definition)
evidence_extract_cpp_function_block("${_renderer_text}"
    "GroundLootView CombatRenderer::draw(" _renderer_draw_function)
foreach(_renderer_surface IN ITEMS _renderer_plan_definition
        _renderer_draw_function)
    stage11d_count_raw_token("${${_renderer_surface}}"
        "make_combat_render_plan(" _renderer_surface_count)
    if(NOT _renderer_surface_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires exactly one renderer plan in CombatRenderer::draw")
    endif()
endforeach()
stage11d_count_raw_token("${_renderer_code}" "make_combat_render_plan("
    _renderer_total_plan_count)
if(NOT _renderer_total_plan_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected renderer plan outside CombatRenderer::draw")
endif()
foreach(_required IN ITEMS
        "const GroundLootView ground_loot_view = [&]() noexcept {"
        "return GroundLootView{};" "return renderer.draw(")
    string(FIND "${_host_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing evidence binding: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS
        "validation_runtime->observe_ground_loot(current,pause_menu,"
        "validation_runtime->observe_presented_frame("
        "validation_runtime->observe_capture_result("
        "validation_runtime->write_summaries(")
    string(FIND "${_host_normalized}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing evidence binding: ${_required}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "LastWriteTimeUtc" "1280" "720" "duplicate screenshot hash"
        "committed PNG differs" "committed summary differs"
        "\$feature = Measure-Region \$bitmap"
        "snapshot_ids" "inventory_ids" "hidden-item semantics mismatch"
        "abyss_claimed" "pickup_commit_generation" "preview_visible_count"
        "ordinary_blueprint_hash" "progress_defeated"
        "drop_distance_milli"
        "ordinary production kill progress is invalid"
        "pickup notice region is blank" "result -eq 'pass'")
    string(FIND "${_validator_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence validator missing semantic check: ${_required}")
    endif()
endforeach()

arpg_sanitize_cpp_source("${_stage_header_text}" _stage11d_header_code)
foreach(_required IN ITEMS
        "std::int32_t player_hp{};"
        "std::int32_t player_max_hp{};"
        "bool player_damage_observed{};"
        "bool player_hp_sampled{};")
    stage11d_count_raw_token("${_stage11d_header_code}" "${_required}"
        _stage11d_player_state_count)
    if(NOT _stage11d_player_state_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires live player damage observation")
    endif()
endforeach()
evidence_extract_cpp_function_block("${_report_text}"
    "bool stage11d_target_visible(" _stage11d_target_visible_function)
stage11d_mask_non_direct_executable_scopes(
    "${_stage11d_target_visible_function}"
    _stage11d_target_visible_active)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _stage11d_target_visible_normalized
    "${_stage11d_target_visible_active}")
foreach(_required IN ITEMS
        "conststd::int32_tcurrent_hp=snapshot.combat->player.hp;"
        "conststd::int32_tcurrent_max_hp=snapshot.combat->player.max_hp;"
        "state.player_damage_observed=state.player_damage_observed||(state.player_hp_sampled&&current_max_hp==state.player_max_hp&&current_hp<state.player_hp);"
        "state.player_hp=current_hp;"
        "state.player_max_hp=current_max_hp;"
        "state.player_hp_sampled=true;"
        "state.player_hp=0;"
        "state.player_max_hp=0;"
        "state.player_hp_sampled=false;")
    string(FIND "${_stage11d_target_visible_normalized}" "${_required}"
        _stage11d_player_damage_observation)
    if(_stage11d_player_damage_observation EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires consecutive live player HP sampling")
    endif()
endforeach()
evidence_extract_cpp_function_block("${_report_text}"
    "void write_stage11d_loot_validation_summary(" _stage11d_summary_function)
string(REGEX REPLACE "[ \t\r\n]+" "" _stage11d_summary_normalized
    "${_stage11d_summary_function}")
foreach(_required IN ITEMS
        "state.player_hp" "state.player_max_hp"
        "state.player_damage_observed")
    stage11d_count_raw_token("${_stage11d_summary_normalized}"
        "${_required}" _stage11d_summary_player_count)
    if(NOT _stage11d_summary_player_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires consecutive live player HP sampling")
    endif()
endforeach()

string(REGEX REPLACE "#[^\r\n]*" "" _stage11d_validator_code
    "${_validator_text}")
string(REGEX REPLACE "[ \t\r\n]+" "" _stage11d_validator_normalized
    "${_stage11d_validator_code}")
foreach(_required IN ITEMS
        "$values.player_damage_observed-eq'1'"
        "[int]$values.progress_hp-gt0"
        "[int]$values.progress_hp-le[int]$values.progress_max_hp")
    stage11d_count_raw_token("${_stage11d_validator_normalized}"
        "${_required}" _stage11d_player_validator_count)
    if(NOT _stage11d_player_validator_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard requires live player damage observation")
    endif()
endforeach()
foreach(_forbidden IN ITEMS
        "$values.monster_damage_observed-eq'1'"
        "$defeated-ge[uint32]$manifest.ordinary_prefix_kills")
    string(FIND "${_stage11d_validator_normalized}" "${_forbidden}"
        _stage11d_forbidden_requirement)
    if(NOT _stage11d_forbidden_requirement EQUAL -1)
        if(_forbidden MATCHES "monster_damage")
            message(FATAL_ERROR
                "Stage11D loot evidence guard forbids diagnostic monster damage as a formal requirement")
        endif()
        message(FATAL_ERROR
            "Stage11D loot evidence guard forbids fixture prefix length as a formal kill requirement")
    endif()
endforeach()
if(NOT _stage11d_validator_ast_checked)
    execute_process(
        COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${_validator_ast_guard}" -ValidatorPath "${_validator}"
        RESULT_VARIABLE _stage11d_validator_ast_result
        OUTPUT_VARIABLE _stage11d_validator_ast_output
        ERROR_VARIABLE _stage11d_validator_ast_error)
endif()
if(NOT _stage11d_validator_ast_result EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard validator AST validation failed: ${_stage11d_validator_ast_output}\n${_stage11d_validator_ast_error}")
endif()

message(STATUS "Stage11D loot formal evidence guard passed")
