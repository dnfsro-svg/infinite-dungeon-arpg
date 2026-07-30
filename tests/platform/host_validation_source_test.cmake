if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake_source_registration_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

set(_host_validation_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation.hpp")
set(_host_validation_state
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_state.hpp")
set(_host_validation_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_runtime.cpp")
set(_host_header_path "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_host_frame_gate
    "${SOURCE_ROOT}/src/platform/raylib/host_frame_gate.cpp")

foreach(_target IN ITEMS
        "${_host_validation_header}"
        "${_host_validation_state}"
        "${_host_validation_runtime}")
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR
            "Host validation runtime target is missing: ${_target}")
    endif()
endforeach()

if(NOT EXISTS "${_host_frame_gate}")
    message(FATAL_ERROR
        "T8A: Host frame gate source is missing: ${_host_frame_gate}")
endif()

set(_host_source "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_raylib_cmake "${SOURCE_ROOT}/src/platform/raylib/CMakeLists.txt")
foreach(_target IN ITEMS "${_host_header_path}" "${_host_source}" "${_raylib_cmake}")
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR
            "Host validation integration target is missing: ${_target}")
    endif()
endforeach()

file(READ "${_host_validation_header}" _facade_text)
file(READ "${_host_validation_state}" _state_text)
file(READ "${_host_validation_runtime}" _runtime_text)
file(READ "${_host_header_path}" _host_header_text)
file(READ "${_host_frame_gate}" _host_frame_gate_text)
file(READ "${_host_source}" _host_text)
file(READ "${_raylib_cmake}" _raylib_cmake_text)

# Translation-phase splices are folded by the shared lexer. Mask every
# conditional region after lexing so #if 0, #ifdef and spliced directives can
# never lend active ownership evidence to this architecture guard.
function(host_validation_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
    if(ARGC GREATER 2)
        set("${ARGV2}" "${_logical_source}" PARENT_SCOPE)
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

        set(_mask_line FALSE)
        if(_line MATCHES
                "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} + 1")
            set(_mask_line TRUE)
        elseif(_line MATCHES "^[ \t]*#[ \t]*endif([ \t\r\n]|$)")
            if(_conditional_depth EQUAL 0)
                message(FATAL_ERROR
                    "Host validation inactive preprocessor surface is unbalanced")
            endif()
            set(_mask_line TRUE)
            math(EXPR _conditional_depth "${_conditional_depth} - 1")
        elseif(_conditional_depth GREATER 0)
            set(_mask_line TRUE)
        endif()

        if(_mask_line)
            string(REGEX REPLACE "[^\r\n]" " " _line "${_line}")
        endif()
        string(APPEND _surface "${_line}")
        if(_newline EQUAL -1)
            break()
        endif()
        math(EXPR _cursor "${_cursor} + ${_line_length}")
    endwhile()
    if(NOT _conditional_depth EQUAL 0)
        message(FATAL_ERROR
            "Host validation inactive preprocessor surface is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

# All callers pass a surface that host_validation_unconditional_cpp_surface
# already sanitized. Extract directly so the shared helper does not sanitize
# the same translation unit again for every architecture assertion.
function(host_validation_extract_sanitized_block
        SANITIZED_SOURCE SIGNATURE OUT_BLOCK)
    evidence_find_cpp_function_bounds_in_sanitized(
        "${SANITIZED_SOURCE}" "${SIGNATURE}"
        _block_begin _block_open _block_end)
    math(EXPR _block_length "${_block_end} - ${_block_begin} + 1")
    string(SUBSTRING "${SANITIZED_SOURCE}"
        ${_block_begin} ${_block_length} _block)
    set(${OUT_BLOCK} "${_block}" PARENT_SCOPE)
endfunction()

function(host_validation_normalize_cpp_surface SOURCE OUT_NORMALIZED)
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized "${SOURCE}")
    string(STRIP "${_normalized}" _normalized)
    set("${OUT_NORMALIZED}" "${_normalized}" PARENT_SCOPE)
endfunction()

function(host_validation_canonicalize_cpp_surface SOURCE OUT_CANONICAL)
    host_validation_normalize_cpp_surface("${SOURCE}" _canonical)
    string(REGEX REPLACE " ([^A-Za-z0-9_])" "\\1" _canonical
        "${_canonical}")
    string(REGEX REPLACE "([^A-Za-z0-9_]) " "\\1" _canonical
        "${_canonical}")
    set("${OUT_CANONICAL}" "${_canonical}" PARENT_SCOPE)
endfunction()

function(host_validation_exact_surface_valid SOURCE EXPECTED OUT_VALID)
    host_validation_canonicalize_cpp_surface("${SOURCE}" _actual)
    host_validation_canonicalize_cpp_surface("${EXPECTED}" _expected)
    if("${_actual}" STREQUAL "${_expected}")
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    endif()
endfunction()

set(_host_validation_formatting_contract [=[
decision.stage17_capture_path = host_validation::stage17_capture_path(
    *impl_->config, impl_->states.stage17);
]=])
set(_host_validation_formatting_variant [=[
decision.stage17_capture_path=host_validation::stage17_capture_path(*impl_->config,impl_->states.stage17);
]=])
host_validation_exact_surface_valid(
    "${_host_validation_formatting_variant}"
    "${_host_validation_formatting_contract}"
    _host_validation_harmless_formatting_valid)
if(NOT _host_validation_harmless_formatting_valid)
    message(FATAL_ERROR
        "Host validation exact-surface guard rejected harmless punctuation whitespace")
endif()

function(host_validation_require_exact_surface LABEL SOURCE EXPECTED)
    host_validation_exact_surface_valid("${SOURCE}" "${EXPECTED}" _valid)
    if(NOT _valid)
        message(FATAL_ERROR
            "Host validation ${LABEL} has an extra or altered member/statement")
    endif()
endfunction()

function(host_validation_expect_exact_mutation_rejected
        LABEL SOURCE EXPECTED OLD_FRAGMENT NEW_FRAGMENT)
    string(REPLACE "${OLD_FRAGMENT}" "${NEW_FRAGMENT}" _mutation "${SOURCE}")
    if("${_mutation}" STREQUAL "${SOURCE}")
        message(FATAL_ERROR
            "Host validation exact-surface mutation anchor is missing: ${LABEL}")
    endif()
    host_validation_exact_surface_valid("${_mutation}" "${EXPECTED}" _valid)
    if(_valid)
        message(FATAL_ERROR
            "Host validation exact-surface guard accepted mutation: ${LABEL}")
    endif()
endfunction()

function(host_validation_count_regex SOURCE PATTERN OUT_COUNT)
    string(REGEX MATCHALL "${PATTERN}" _matches "${SOURCE}")
    list(LENGTH _matches _count)
    set("${OUT_COUNT}" ${_count} PARENT_SCOPE)
endfunction()

function(host_validation_require_regex_count LABEL SOURCE PATTERN EXPECTED)
    host_validation_count_regex("${SOURCE}" "${PATTERN}" _actual)
    if(NOT _actual EQUAL EXPECTED)
        message(FATAL_ERROR
            "Host validation ${LABEL}: expected ${EXPECTED}, found ${_actual}")
    endif()
endfunction()

function(host_validation_snapshot_bindings_valid_from_normalized
        NORMALIZED_SOURCE OUT_VALID)
    foreach(_pattern IN ITEMS
            "const[ ]+PhysicalKeySnapshot&[ ]+physical_keys[ ]*=[ ]*validation_runtime->death_input_snapshot\\([ ]*\\)"
            "gameplay_controls_physically_released\\([ ]*stage17_physical_keys[ ]*\\)"
            "map_host_frame_input\\([ ]*input_settings,[ ]*stage17_physical_keys[ ]*\\)"
            "host_death_input_gate\\([ ]*death_saving,[ ]*death_pending,[ ]*frame_input[.]keys,[ ]*physical_keys[ ]*\\)"
            "if[ ]*\\([ ]*pause_was_open[ ]*&&[ ]*physical_keys[.]mouse_left[ ]*\\)"
            "hit_test_pause_row\\([ ]*layout,[ ]*physical_keys[.]mouse_position[ ]*\\)"
            "![ ]*physical_keys[.]focus_lost"
            "PauseInput[ ]+pause_input[ ]*=[ ]*pause_input_from_snapshot\\([ ]*physical_keys,[ ]*escape_consumed,[ ]*pause_menu[.]screen[ ]*==[ ]*PauseScreen::capture_binding[ ]*\\)")
        host_validation_count_regex("${NORMALIZED_SOURCE}" "${_pattern}"
            _match_count)
        if(NOT _match_count EQUAL 1)
            set("${OUT_VALID}" FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    host_validation_count_regex("${NORMALIZED_SOURCE}"
        "(^|[^A-Za-z0-9_])stage17_physical_keys([^A-Za-z0-9_]|$)"
        _stage17_snapshot_count)
    host_validation_count_regex("${NORMALIZED_SOURCE}"
        "(^|[^A-Za-z0-9_])physical_keys([^A-Za-z0-9_]|$)"
        _cached_snapshot_count)
    host_validation_count_regex("${NORMALIZED_SOURCE}"
        "pause_input_from_snapshot\\(" _pause_input_call_count)
    if(NOT _stage17_snapshot_count EQUAL 3
            OR NOT _cached_snapshot_count EQUAL 6
            OR NOT _pause_input_call_count EQUAL 1)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VALID}" TRUE PARENT_SCOPE)
endfunction()

function(host_validation_expect_snapshot_mutation_rejected
        LABEL NORMALIZED_SOURCE OLD_FRAGMENT NEW_FRAGMENT)
    string(REPLACE "${OLD_FRAGMENT}" "${NEW_FRAGMENT}"
        _mutation "${NORMALIZED_SOURCE}")
    if("${_mutation}" STREQUAL "${NORMALIZED_SOURCE}")
        message(FATAL_ERROR
            "Host validation snapshot mutation anchor is missing: ${LABEL}")
    endif()
    host_validation_snapshot_bindings_valid_from_normalized(
        "${_mutation}" _valid)
    if(_valid)
        message(FATAL_ERROR
            "Host validation snapshot boundary accepted mutation: ${LABEL}")
    endif()
endfunction()

function(host_validation_lexical_token_present SOURCE TOKEN OUT_PRESENT)
    arpg_sanitize_cpp_source("${SOURCE}" _lexical)
    string(FIND "${_lexical}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        set("${OUT_PRESENT}" FALSE PARENT_SCOPE)
    else()
        set("${OUT_PRESENT}" TRUE PARENT_SCOPE)
    endif()
endfunction()

function(host_validation_count_token SOURCE TOKEN OUT_COUNT)
    set(_tail "${SOURCE}")
    set(_count 0)
    while(TRUE)
        string(FIND "${_tail}" "${TOKEN}" _position)
        if(_position EQUAL -1)
            break()
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${_tail}" ${_after} -1 _tail)
        math(EXPR _count "${_count} + 1")
    endwhile()
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(host_validation_require_count LABEL SOURCE TOKEN EXPECTED)
    host_validation_count_token("${SOURCE}" "${TOKEN}" _actual)
    if(NOT _actual EQUAL EXPECTED)
        message(FATAL_ERROR
            "Host validation ${LABEL}: expected ${EXPECTED}, found ${_actual}: ${TOKEN}")
    endif()
endfunction()

function(host_validation_count_identifier SOURCE IDENTIFIER OUT_COUNT)
    string(REGEX REPLACE "[^A-Za-z0-9_]" " " _identifier_surface
        "${SOURCE}")
    string(REGEX REPLACE "[ ]+" " " _identifier_surface
        "${_identifier_surface}")
    string(STRIP "${_identifier_surface}" _identifier_surface)
    if(_identifier_surface STREQUAL "")
        set(${OUT_COUNT} 0 PARENT_SCOPE)
        return()
    endif()
    set(_identifier_surface " ${_identifier_surface} ")
    host_validation_count_token("${_identifier_surface}"
        " ${IDENTIFIER} " _identifier_count)
    set(${OUT_COUNT} ${_identifier_count} PARENT_SCOPE)
endfunction()

function(host_validation_require_identifier_count
        LABEL SOURCE IDENTIFIER EXPECTED)
    host_validation_count_identifier("${SOURCE}" "${IDENTIFIER}" _actual)
    if(NOT _actual EQUAL EXPECTED)
        message(FATAL_ERROR
            "Host validation ${LABEL}: expected ${EXPECTED}, found ${_actual}: ${IDENTIFIER}")
    endif()
endfunction()

function(host_validation_require_preprocessor_macro_free
        LABEL SOURCE)
    string(REGEX MATCH
        "(^|\n)[ \t]*#[ \t]*(define|undef)([ \t]|$)"
        _mutation_match "${SOURCE}")
    if(NOT _mutation_match STREQUAL "")
        message(FATAL_ERROR
            "Host validation ${LABEL} contains a forbidden #define or #undef")
    endif()
endfunction()

function(host_validation_fold_cpp_phase2_splices SOURCE OUT_SOURCE)
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

function(host_validation_runtime_allocation_surface_valid
        RUNTIME_SOURCE CREATE_SOURCE OUT_VALID)
    host_validation_normalize_cpp_surface("${RUNTIME_SOURCE}"
        _runtime_normalized)
    host_validation_normalize_cpp_surface("${CREATE_SOURCE}"
        _create_normalized)
    host_validation_count_token("${_runtime_normalized}" "new "
        _runtime_new_count)
    host_validation_count_token("${_runtime_normalized}"
        "new (std::nothrow)" _runtime_nothrow_count)
    host_validation_count_token("${_create_normalized}" "new "
        _create_new_count)
    host_validation_count_token("${_create_normalized}"
        "new (std::nothrow) Impl{" _impl_new_count)
    host_validation_count_token("${_create_normalized}"
        "new (std::nothrow) HostValidationRuntime{" _runtime_owner_new_count)
    if(_runtime_new_count EQUAL 2
            AND _runtime_nothrow_count EQUAL 2
            AND _create_new_count EQUAL 2
            AND _impl_new_count EQUAL 1
            AND _runtime_owner_new_count EQUAL 1)
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    endif()
endfunction()

function(host_validation_active_exact_definition_valid
        ACTIVE_SOURCE LEXICAL_SOURCE SIGNATURE EXPECTED OUT_VALID)
    host_validation_count_token("${ACTIVE_SOURCE}" "${SIGNATURE}"
        _active_count)
    host_validation_count_token("${LEXICAL_SOURCE}" "${SIGNATURE}"
        _lexical_count)
    if(NOT _active_count EQUAL 1 OR NOT _lexical_count EQUAL 1)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    host_validation_extract_sanitized_block(
        "${ACTIVE_SOURCE}" "${SIGNATURE}" _active_block)
    host_validation_extract_sanitized_block(
        "${LEXICAL_SOURCE}" "${SIGNATURE}" _lexical_block)
    host_validation_exact_surface_valid(
        "${_active_block}" "${EXPECTED}" _active_valid)
    host_validation_exact_surface_valid(
        "${_lexical_block}" "${EXPECTED}" _lexical_valid)
    if(_active_valid AND _lexical_valid)
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    endif()
endfunction()

function(host_validation_require_active_exact_definition
        LABEL ACTIVE_SOURCE LEXICAL_SOURCE SIGNATURE EXPECTED)
    host_validation_active_exact_definition_valid(
        "${ACTIVE_SOURCE}" "${LEXICAL_SOURCE}" "${SIGNATURE}"
        "${EXPECTED}" _valid)
    if(NOT _valid)
        message(FATAL_ERROR
            "Host validation ${LABEL} is not one unique active exact definition")
    endif()
endfunction()

function(host_validation_expect_inactive_correct_active_mutation_rejected
        LABEL SIGNATURE EXPECTED ACTIVE_MUTATION)
    if("${ACTIVE_MUTATION}" STREQUAL "${EXPECTED}")
        message(FATAL_ERROR
            "Host validation inactive-first mutation did not alter active code: ${LABEL}")
    endif()
    set(_fixture "#if 0\n${EXPECTED}\n#endif\n${ACTIVE_MUTATION}\n")
    host_validation_unconditional_cpp_surface(
        "${_fixture}" _fixture_active _fixture_lexical)
    host_validation_active_exact_definition_valid(
        "${_fixture_active}" "${_fixture_lexical}" "${SIGNATURE}"
        "${EXPECTED}" _valid)
    if(_valid)
        message(FATAL_ERROR
            "Host validation active-definition guard borrowed #if 0 evidence: ${LABEL}")
    endif()
endfunction()

function(host_validation_find_balanced_scope_end
        SOURCE OPEN_INDEX OUT_END OUT_VALID)
    string(LENGTH "${SOURCE}" _source_length)
    if(OPEN_INDEX LESS 0 OR OPEN_INDEX GREATER_EQUAL _source_length)
        set("${OUT_END}" -1 PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" ${OPEN_INDEX} 1 _open_character)
    if(NOT _open_character STREQUAL "{")
        set("${OUT_END}" -1 PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    set(_cursor ${OPEN_INDEX})
    set(_depth 0)
    while(_cursor LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_cursor} -1 _tail)
        string(FIND "${_tail}" "{" _next_open)
        string(FIND "${_tail}" "}" _next_close)
        if(_next_close EQUAL -1)
            set("${OUT_END}" -1 PARENT_SCOPE)
            set("${OUT_VALID}" FALSE PARENT_SCOPE)
            return()
        endif()
        if(NOT _next_open EQUAL -1 AND _next_open LESS _next_close)
            math(EXPR _cursor "${_cursor} + ${_next_open} + 1")
            math(EXPR _depth "${_depth} + 1")
        else()
            math(EXPR _close_index "${_cursor} + ${_next_close}")
            math(EXPR _depth "${_depth} - 1")
            if(_depth EQUAL 0)
                set("${OUT_END}" ${_close_index} PARENT_SCOPE)
                set("${OUT_VALID}" TRUE PARENT_SCOPE)
                return()
            endif()
            math(EXPR _cursor "${_close_index} + 1")
        endif()
    endwhile()
    set("${OUT_END}" -1 PARENT_SCOPE)
    set("${OUT_VALID}" FALSE PARENT_SCOPE)
endfunction()

function(host_validation_mask_non_direct_executable_scopes SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" _source_length)
    set(_masked "")
    set(_copy_cursor 0)
    set(_dead_condition
        "(false|0[uUlL]*|![ \\t\\r\\n]*true|1[uUlL]*[ \\t\\r\\n]*==[ \\t\\r\\n]*0[uUlL]*|0[uUlL]*[ \\t\\r\\n]*==[ \\t\\r\\n]*1[uUlL]*)")
    while(_copy_cursor LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_copy_cursor} -1 _tail)
        string(REGEX MATCH
            "\\][ \\t\\r\\n]*(\\([^{};]*\\))?[ \\t\\r\\n]*(mutable[ \\t\\r\\n]*)?(noexcept([ \\t\\r\\n]*\\([^{};]*\\))?[ \\t\\r\\n]*)?(->[^{;]*)?[ \\t\\r\\n]*\\{"
            _lambda_match "${_tail}")
        string(REGEX MATCH
            "(if|while)[ \\t\\r\\n]*(constexpr[ \\t\\r\\n]*)?\\([ \\t\\r\\n]*${_dead_condition}[ \\t\\r\\n]*\\)[ \\t\\r\\n]*(do[ \\t\\r\\n]*)?([^{;]*\\{|[^{};]*;)"
            _dead_branch_match "${_tail}")
        string(REGEX MATCH
            "for[ \\t\\r\\n]*\\([ \\t\\r\\n]*;[ \\t\\r\\n]*${_dead_condition}[ \\t\\r\\n]*;[^)]*\\)[ \\t\\r\\n]*([^{;]*\\{|[^{};]*;)"
            _dead_for_match "${_tail}")
        set(_scope_match "")
        set(_scope_relative -1)
        set(_scope_kind "")
        if(NOT _lambda_match STREQUAL "")
            string(FIND "${_tail}" "${_lambda_match}" _scope_relative)
            set(_scope_match "${_lambda_match}")
            set(_scope_kind lambda)
        endif()
        foreach(_dead_match_name IN ITEMS
                _dead_branch_match _dead_for_match)
            set(_dead_match "${${_dead_match_name}}")
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
            host_validation_find_balanced_scope_end(
                "${SOURCE}" ${_open_index} _scope_end _scope_valid)
            if(NOT _scope_valid)
                message(FATAL_ERROR
                    "Host validation direct-scope fixture has no closing brace")
            endif()
        endif()
        math(EXPR _copy_cursor "${_scope_end} + 1")
    endwhile()
    set("${OUT_SOURCE}" "${_masked}" PARENT_SCOPE)
endfunction()

function(host_validation_run_host_snapshot_bindings_valid
        SANITIZED_RUN_HOST OUT_VALID)
    host_validation_mask_non_direct_executable_scopes(
        "${SANITIZED_RUN_HOST}" _direct_run_host)
    host_validation_count_token("${_direct_run_host}"
        "pause_input_from_snapshot(" _total_pause_input_calls)
    if(NOT _total_pause_input_calls EQUAL 1)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        if(ARGC GREATER 2)
            set("${ARGV2}" "" PARENT_SCOPE)
        endif()
        return()
    endif()
    string(FIND "${_direct_run_host}"
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        _boundary_begin)
    string(FIND "${_direct_run_host}" "const PauseCommand pause_command ="
        _boundary_end)
    if(_boundary_begin EQUAL -1 OR _boundary_end EQUAL -1
            OR NOT _boundary_begin LESS _boundary_end)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        if(ARGC GREATER 2)
            set("${ARGV2}" "" PARENT_SCOPE)
        endif()
        return()
    endif()
    math(EXPR _boundary_length "${_boundary_end} - ${_boundary_begin}")
    string(SUBSTRING "${_direct_run_host}" ${_boundary_begin}
        ${_boundary_length} _boundary)
    host_validation_normalize_cpp_surface("${_boundary}" _normalized)
    host_validation_snapshot_bindings_valid_from_normalized(
        "${_normalized}" _valid)
    set("${OUT_VALID}" ${_valid} PARENT_SCOPE)
    if(ARGC GREATER 2)
        set("${ARGV2}" "${_normalized}" PARENT_SCOPE)
    endif()
endfunction()

function(host_validation_expect_run_host_snapshot_mutation_rejected
        LABEL RUN_HOST OLD_FRAGMENT NEW_FRAGMENT)
    string(REPLACE "${OLD_FRAGMENT}" "${NEW_FRAGMENT}" _mutation "${RUN_HOST}")
    if("${_mutation}" STREQUAL "${RUN_HOST}")
        message(FATAL_ERROR
            "Host validation run-host mutation anchor is missing: ${LABEL}")
    endif()
    host_validation_run_host_snapshot_bindings_valid("${_mutation}" _valid)
    if(_valid)
        message(FATAL_ERROR
            "Host validation run-host scope guard accepted mutation: ${LABEL}")
    endif()
endfunction()

function(host_validation_require_order LABEL SOURCE)
    set(_tail "${SOURCE}")
    foreach(_token IN LISTS ARGN)
        string(FIND "${_tail}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR
                "Host validation ${LABEL} token is missing or reordered: ${_token}")
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${_tail}" ${_after} -1 _tail)
    endforeach()
endfunction()

function(host_validation_require_canonical_count
        LABEL SOURCE EXPECTED EXPECTED_COUNT)
    host_validation_canonicalize_cpp_surface("${SOURCE}" _canonical_source)
    host_validation_canonicalize_cpp_surface("${EXPECTED}" _canonical_expected)
    host_validation_require_count("${LABEL}" "${_canonical_source}"
        "${_canonical_expected}" ${EXPECTED_COUNT})
endfunction()

function(host_validation_require_canonical_order LABEL SOURCE)
    host_validation_canonicalize_cpp_surface("${SOURCE}" _canonical_source)
    set(_tail "${_canonical_source}")
    foreach(_token IN LISTS ARGN)
        host_validation_canonicalize_cpp_surface(
            "${_token}" _canonical_token)
        string(FIND "${_tail}" "${_canonical_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR
                "Host validation ${LABEL} token is missing or reordered: ${_token}")
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${_tail}" ${_after} -1 _tail)
    endforeach()
endfunction()

function(host_validation_require_canonical_direct_statement
        LABEL SOURCE EXPECTED EXPECTED_DEPTH)
    host_validation_canonicalize_cpp_surface("${SOURCE}" _canonical_source)
    host_validation_canonicalize_cpp_surface("${EXPECTED}" _canonical_expected)
    host_validation_count_token("${_canonical_source}"
        "${_canonical_expected}" _statement_count)
    string(FIND "${_canonical_source}"
        "${_canonical_expected}" _statement_position)
    if(NOT _statement_count EQUAL 1 OR _statement_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation ${LABEL} is missing or duplicated")
    endif()
    host_validation_brace_depth("${_canonical_source}"
        ${_statement_position} _statement_depth)
    if(NOT _statement_depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Host validation ${LABEL} is outside direct executable scope")
    endif()
endfunction()

function(host_validation_brace_depth SURFACE POSITION OUT_DEPTH)
    string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    string(REGEX REPLACE "[^{}]" "" _braces "${_prefix}")
    string(LENGTH "${_braces}" _brace_length)
    set(_depth 0)
    if(_brace_length GREATER 0)
        math(EXPR _brace_last "${_brace_length} - 1")
        foreach(_index RANGE 0 ${_brace_last})
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

function(host_validation_require_depth LABEL SURFACE TOKEN EXPECTED)
    host_validation_require_count("${LABEL} uniqueness" "${SURFACE}"
        "${TOKEN}" 1)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    host_validation_brace_depth("${SURFACE}" ${_position} _actual)
    if(NOT _actual EQUAL EXPECTED)
        message(FATAL_ERROR
            "Host validation ${LABEL} has wrong brace depth: expected=${EXPECTED}, actual=${_actual}")
    endif()
endfunction()

function(host_validation_reject_return_before LABEL SURFACE TOKEN)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation ${LABEL} anchor is missing: ${TOKEN}")
    endif()
    string(SUBSTRING "${SURFACE}" 0 ${_position} _prefix)
    if(_prefix MATCHES "(^|[;{}])[ \t\r\n]*return([ \t\r\n;{(]|$)")
        message(FATAL_ERROR
            "Host validation ${LABEL} is unreachable after an early return")
    endif()
endfunction()

function(host_validation_assert_top_level_registration CMAKE_TEXT SOURCE)
    arpg_cmake_count_arpg_raylib_source("${CMAKE_TEXT}" "${SOURCE}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Host validation runtime must have one direct arpg_raylib registration: ${SOURCE}")
    endif()

    arpg_cmake_code_surface("${CMAKE_TEXT}" _cmake_code)
    string(TOLOWER "${_cmake_code}" _cmake_lower)
    string(FIND "${_cmake_lower}" "add_library(arpg_raylib static" _library)
    string(TOLOWER "${SOURCE}" _source_lower)
    string(REPLACE "." "[.]" _source_pattern "${_source_lower}")
    string(REGEX MATCH
        "(^|\n)[ \t]*${_source_pattern}[ \t]*(\n|$)"
        _source_line "${_cmake_lower}")
    string(FIND "${_cmake_lower}" "${_source_line}" _source_line_position)
    string(FIND "${_source_line}" "${_source_lower}" _source_in_line)
    if(_library EQUAL -1 OR _source_line STREQUAL ""
            OR _source_line_position EQUAL -1 OR _source_in_line EQUAL -1)
        message(FATAL_ERROR
            "Host validation source is missing from the primary arpg_raylib list")
    endif()
    math(EXPR _source "${_source_line_position} + ${_source_in_line}")
    string(SUBSTRING "${_cmake_lower}" 0 ${_library} _library_prefix)
    string(REGEX REPLACE "[ \t\r\n]" ""
        _library_prefix_compact "${_library_prefix}")
    string(SUBSTRING "${_cmake_lower}" ${_library} -1 _library_tail)
    string(FIND "${_library_tail}" ")" _library_close_relative)
    if(NOT _library_prefix_compact STREQUAL ""
            OR _library_close_relative EQUAL -1)
        message(FATAL_ERROR
            "Host validation primary arpg_raylib list is not a real top-level command")
    endif()
    math(EXPR _library_close "${_library} + ${_library_close_relative}")
    if(NOT _library LESS _source OR NOT _source LESS _library_close)
        message(FATAL_ERROR
            "Host validation source is outside the primary arpg_raylib list")
    endif()
    string(SUBSTRING "${_cmake_lower}" 0 ${_source} _prefix)
    if(_prefix MATCHES "(^|\n)[ \t]*return[ \t]*\\(")
        message(FATAL_ERROR
            "Host validation runtime registration is unreachable after return")
    endif()
    arpg_cmake_code_line_and_paren_delta("${_prefix}" _paren_depth)
    if(NOT _paren_depth EQUAL 1)
        message(FATAL_ERROR
            "Host validation source registration is not a direct add_library argument")
    endif()
endfunction()

host_validation_unconditional_cpp_surface(
    "${_facade_text}" _facade _facade_lexical)
host_validation_unconditional_cpp_surface(
    "${_state_text}" _state _state_lexical)
host_validation_unconditional_cpp_surface(
    "${_runtime_text}" _runtime _runtime_lexical)
host_validation_unconditional_cpp_surface(
    "${_host_header_text}" _host_header _host_header_lexical)
arpg_sanitize_cpp_source(
    "${_host_frame_gate_text}" _host_frame_gate_lexical)
host_validation_unconditional_cpp_surface(
    "${_host_text}" _host _host_lexical)

function(host_validation_expect_dead_control_hidden LABEL PREFIX SUFFIX)
    set(_decision_anchor [=[decision.validation_complete = stage10_reached || stage11_reached
        || stage11b_reached || stage11c_reached
        || stage11d_reached || stage17_reached;]=])
    host_validation_extract_sanitized_block("${_runtime}"
        "PresentationDecision HostValidationRuntime::observe_presented_frame("
        _decision_owner)
    string(REPLACE "${_decision_anchor}"
        "${PREFIX}${_decision_anchor}${SUFFIX}"
        _mutation "${_decision_owner}")
    if("${_mutation}" STREQUAL "${_decision_owner}")
        message(FATAL_ERROR
            "Host validation dead-control mutation anchor is missing: ${LABEL}")
    endif()
    host_validation_mask_non_direct_executable_scopes(
        "${_mutation}" _direct_mutation)
    host_validation_canonicalize_cpp_surface(
        "${_direct_mutation}" _canonical_mutation)
    host_validation_canonicalize_cpp_surface(
        "${_decision_anchor}" _canonical_anchor)
    host_validation_count_token("${_canonical_mutation}"
        "${_canonical_anchor}" _surviving_anchor_count)
    if(NOT _surviving_anchor_count EQUAL 0)
        message(FATAL_ERROR
            "Host validation direct-scope guard accepted dead control: ${LABEL}")
    endif()
endfunction()

host_validation_expect_dead_control_hidden(
    "for-false-braced" "for (\; false\;) {" "}")
host_validation_expect_dead_control_hidden(
    "for-false-unbraced" "for (\; false\;) " "")
host_validation_expect_dead_control_hidden(
    "for-zero-braced" "for (\; 0\;) {" "}")
host_validation_expect_dead_control_hidden(
    "for-zero-unbraced" "for (\; 0\;) " "")
host_validation_expect_dead_control_hidden(
    "if-equality-braced" "if (1 == 0) {" "}")
host_validation_expect_dead_control_hidden(
    "if-equality-unbraced" "if (1 == 0) " "")
host_validation_expect_dead_control_hidden(
    "while-equality-braced" "while (1 == 0) {" "}")
host_validation_expect_dead_control_hidden(
    "while-equality-unbraced" "while (1 == 0) " "")

# The public facade is intentionally raylib-free and exposes only the approved
# non-owning API plus one unique_ptr PImpl member.
foreach(_forbidden IN ITEMS
        "raylib.h" "host_validation_state.hpp" "host_validation_stage")
    string(FIND "${_facade_lexical}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation facade leaks a private or raylib dependency: ${_forbidden}")
    endif()
endforeach()
host_validation_extract_sanitized_block("${_facade}"
    "class HostValidationRuntime final" _facade_class)
host_validation_extract_sanitized_block("${_facade_lexical}"
    "class HostValidationRuntime final" _facade_class_lexical)
set(_capture_owner_contract [=[
enum class CaptureOwner : std::uint8_t {
    none,
    generic_validation,
    stage17,
}
]=])
set(_presentation_decision_contract [=[
struct PresentationDecision final {
    bool validation_complete{};
    bool generic_capture_visible{};
    bool generic_capture_complete{};
    CaptureOwner capture_owner{CaptureOwner::none};
    std::optional<std::string> stage17_capture_path{};
}
]=])
host_validation_require_active_exact_definition(
    "capture-owner exact API" "${_facade}" "${_facade_lexical}"
    "enum class CaptureOwner : std::uint8_t" "${_capture_owner_contract}")
host_validation_require_active_exact_definition(
    "presentation-decision exact API" "${_facade}" "${_facade_lexical}"
    "struct PresentationDecision final" "${_presentation_decision_contract}")
set(_facade_class_contract [=[
class HostValidationRuntime final {
public:
    static std::unique_ptr<HostValidationRuntime> create(
        const RaylibHostConfig&,
        settings::SettingsLoadStatus) noexcept;
    ~HostValidationRuntime();
    void set_render_readiness(bool cjk_font_ready,
        bool active_skill_atlases_ready) noexcept;
    PhysicalKeySnapshot inject_physical_edges(
        PhysicalKeySnapshot, const settings::SettingsData&,
        const dungeon::DungeonSnapshot&,
        bool gameplay_rearm_required) noexcept;
    [[nodiscard]] bool should_continue_death(
        const dungeon::DungeonSnapshot&) const noexcept;
    combat::MovementInput fixed_step_movement(
        dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
        combat::MovementInput production_input) noexcept;
    void observe_fixed_tick() noexcept;
    void observe_death_continue_result(
        dungeon::RequestResult) noexcept;
    void observe_post_fixed_tick(
        const dungeon::DungeonSnapshot&,
        const items::ItemOwnershipState*) noexcept;
    [[nodiscard]] bool fixed_step_target_reached(
        const dungeon::DungeonSnapshot&) const noexcept;
    [[nodiscard]] const PhysicalKeySnapshot&
    death_input_snapshot() const noexcept;
    void observe_pause_transition(bool was_open, bool is_open) noexcept;
    void prepare_hud_snapshot(dungeon::DungeonSnapshot&) noexcept;
    void observe_hud(const dungeon::DungeonSnapshot&,
        const HudViewModel&, HudNoticeView, bool draw_debug,
        int screen_width, int screen_height) noexcept;
    void observe_active_skill_draw(const dungeon::DungeonSnapshot&,
        const ActiveSkillDrawRuntimeStatus&) noexcept;
    void observe_ground_loot(const dungeon::DungeonSnapshot&,
        const PauseMenuState&, const DungeonRenderStatus&,
        settings::LootFilterMode, const GroundLootView&, HudNoticeView,
        const items::ItemOwnershipState*, int screen_width,
        int screen_height) noexcept;
    [[nodiscard]] PresentationDecision observe_presented_frame(
        const dungeon::DungeonSnapshot&, const PauseMenuState&,
        bool pause_cjk_ready) noexcept;
    void observe_capture_result(CaptureOwner, bool succeeded) noexcept;
    void write_summaries(CleanShutdownState,
        const PauseMenuState&) noexcept;
    void observe_combat_event(const combat::CombatEvent&) noexcept;
    void observe_snapshot(const dungeon::DungeonSnapshot&) noexcept;
    void observe_inventory(const InventoryRenderer&,
        const dungeon::DungeonSnapshot&) noexcept;
    void observe_submitted_actions(const SubmittedFrameActions&) noexcept;

private:
    struct Impl;
    explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
}
]=])
host_validation_require_active_exact_definition("facade exact API"
    "${_facade}" "${_facade_lexical}"
    "class HostValidationRuntime final" "${_facade_class_contract}")
host_validation_expect_exact_mutation_rejected("extra facade public API"
    "${_facade_class_lexical}" "${_facade_class_contract}"
    "private:"
    "void forbidden_extra_public_api() noexcept; private:")
foreach(_token IN ITEMS
        "static std::unique_ptr<HostValidationRuntime> create("
        "settings::SettingsLoadStatus) noexcept;"
        "~HostValidationRuntime();"
        "void set_render_readiness("
        "PhysicalKeySnapshot inject_physical_edges("
        "[[nodiscard]] bool should_continue_death("
        "combat::MovementInput fixed_step_movement("
        "void observe_fixed_tick() noexcept;"
        "void observe_death_continue_result("
        "void observe_post_fixed_tick("
        "[[nodiscard]] bool fixed_step_target_reached("
        "[[nodiscard]] const PhysicalKeySnapshot&"
        "death_input_snapshot() const noexcept;"
        "void observe_pause_transition("
        "void prepare_hud_snapshot("
        "void observe_hud("
        "void observe_active_skill_draw("
        "void observe_ground_loot("
        "[[nodiscard]] PresentationDecision observe_presented_frame("
        "void observe_capture_result("
        "void write_summaries("
        "void observe_combat_event("
        "void observe_snapshot("
        "void observe_inventory("
        "void observe_submitted_actions("
        "struct Impl;"
        "explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;"
        "std::unique_ptr<Impl> impl_;")
    host_validation_require_count("facade API" "${_facade_class}"
        "${_token}" 1)
endforeach()

# Keep the facade header raylib-free while making every new by-reference/value
# type available through one exact forward declaration in active code.  These
# checks deliberately follow the class contract so the pre-implementation RED
# is the missing facade API rather than a secondary forward-declaration error.
set(_combat_forward_contract [=[
namespace arpg::combat {
struct CombatEvent;
struct MovementInput;
}
]=])
set(_dungeon_forward_contract [=[
namespace arpg::dungeon {
class DungeonSession;
struct DungeonSnapshot;
enum class RequestResult : std::uint8_t;
}
]=])
set(_items_forward_contract [=[
namespace arpg::items {
struct ItemOwnershipState;
}
]=])
set(_settings_forward_contract [=[
namespace arpg::settings {
enum class LootFilterMode : std::uint8_t;
enum class SettingsLoadStatus : std::uint8_t;
struct SettingsData;
}
]=])
host_validation_require_active_exact_definition(
    "combat facade forward declarations" "${_facade}" "${_facade_lexical}"
    "namespace arpg::combat {" "${_combat_forward_contract}")
host_validation_require_active_exact_definition(
    "dungeon facade forward declarations" "${_facade}" "${_facade_lexical}"
    "namespace arpg::dungeon {" "${_dungeon_forward_contract}")
host_validation_require_active_exact_definition(
    "item facade forward declaration" "${_facade}" "${_facade_lexical}"
    "namespace arpg::items {" "${_items_forward_contract}")
host_validation_require_active_exact_definition(
    "settings facade forward declarations" "${_facade}" "${_facade_lexical}"
    "namespace arpg::settings {" "${_settings_forward_contract}")
foreach(_declaration IN ITEMS
        "struct ActiveSkillDrawRuntimeStatus;"
        "enum class CleanShutdownState : std::uint8_t;"
        "struct DungeonRenderStatus;"
        "struct GroundLootView;"
        "struct HudNoticeView;"
        "struct HudViewModel;"
        "struct PauseMenuState;")
    host_validation_require_count("platform facade forward declaration"
        "${_facade}" "${_declaration}" 1)
    host_validation_require_count("lexical platform facade forward declaration"
        "${_facade_lexical}" "${_declaration}" 1)
endforeach()
foreach(_include IN ITEMS "#include <optional>" "#include <string>")
    host_validation_require_count("active facade standard include"
        "${_facade}" "${_include}" 1)
    host_validation_require_count("lexical facade standard include"
        "${_facade_lexical}" "${_include}" 1)
endforeach()
host_validation_require_count("facade class" "${_facade}"
    "class HostValidationRuntime final" 1)
host_validation_require_count("removed facade transition seam" "${_facade}"
    "HostValidationStateAccess" 0)
host_validation_require_count("removed lexical facade transition seam"
    "${_facade_lexical}" "HostValidationStateAccess" 0)
host_validation_require_count("facade PImpl owner" "${_facade_class}"
    "std::unique_ptr<Impl>" 2)
foreach(_forbidden IN ITEMS
        "Stage10ValidationState" "Stage11ValidationState"
        "Stage11BValidationState" "Stage11CHudValidationState"
        "Stage11DLootValidationState" "Stage17SkillStonesValidationState")
    string(FIND "${_facade_lexical}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation facade leaks Stage state: ${_forbidden}")
    endif()
endforeach()

# The internal state seam aggregates one of each existing Stage state.  Task 7C
# removes the temporary reference-access shim entirely; Stage definitions stay
# in their existing owner headers.
host_validation_extract_sanitized_block("${_state}"
    "struct HostValidationStates final" _aggregate)
host_validation_extract_sanitized_block("${_state_lexical}"
    "struct HostValidationStates final" _aggregate_lexical)
set(_aggregate_contract [=[
struct HostValidationStates final {
    host_validation::Stage10ValidationState stage10{};
    host_validation::Stage11ValidationState stage11{};
    host_validation::Stage11BValidationState stage11b{};
    host_validation::Stage11CHudValidationState stage11c{};
    host_validation::Stage11DLootValidationState stage11d{};
    host_validation::Stage17SkillStonesValidationState stage17{};
}
]=])
host_validation_require_active_exact_definition("state aggregate exact ownership"
    "${_state}" "${_state_lexical}"
    "struct HostValidationStates final" "${_aggregate_contract}")
host_validation_expect_exact_mutation_rejected("extra aggregate owner"
    "${_aggregate_lexical}" "${_aggregate_contract}"
    "host_validation::Stage17SkillStonesValidationState stage17{};"
    "host_validation::Stage17SkillStonesValidationState stage17{}; int forbidden_extra_owner{};")
string(REPLACE
    "host_validation::Stage17SkillStonesValidationState stage17{};"
    "host_validation::Stage17SkillStonesValidationState stage17{}; int forbidden_extra_owner{};"
    _aggregate_active_extra "${_aggregate_contract}")
host_validation_expect_inactive_correct_active_mutation_rejected(
    "inactive correct aggregate plus active extra owner"
    "struct HostValidationStates final" "${_aggregate_contract}"
    "${_aggregate_active_extra}")
foreach(_field IN ITEMS
        "Stage10ValidationState stage10{};"
        "Stage11ValidationState stage11{};"
        "Stage11BValidationState stage11b{};"
        "Stage11CHudValidationState stage11c{};"
        "Stage11DLootValidationState stage11d{};"
        "Stage17SkillStonesValidationState stage17{};")
    host_validation_require_count("state aggregate" "${_aggregate}"
        "${_field}" 1)
endforeach()

set(_stage_state_headers
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage10_11.hpp"
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.hpp"
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.hpp"
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp"
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage17.hpp")
foreach(_header IN LISTS _stage_state_headers)
    if(NOT EXISTS "${_header}")
        message(FATAL_ERROR
            "Host validation Stage state owner is missing: ${_header}")
    endif()
    file(READ "${_header}" _stage_header_text)
    host_validation_unconditional_cpp_surface(
        "${_stage_header_text}" _stage_header_active)
    string(APPEND _stage_headers_active "\n${_stage_header_active}")
endforeach()
foreach(_state_type IN ITEMS
        Stage10ValidationState Stage11ValidationState
        Stage11BValidationState Stage11CHudValidationState
        Stage11DLootValidationState Stage17SkillStonesValidationState)
    host_validation_require_count("Stage state definition ownership"
        "${_stage_headers_active}" "struct ${_state_type} final" 1)
    host_validation_require_count("aggregate state instance"
        "${_aggregate}" "${_state_type} " 1)
    string(FIND "${_runtime_lexical}" "${_state_type} "
        _runtime_direct_state)
    if(NOT _runtime_direct_state EQUAL -1)
        # Runtime algorithms may mention qualified Stage types, so only an
        # unqualified declaration would be a second state owner.
        string(REGEX MATCH
            "(^|[;{}])[ \t\r\n]*${_state_type}[ \t\r\n]+[A-Za-z_]"
            _runtime_direct_state_declaration "${_runtime_lexical}")
        if(NOT _runtime_direct_state_declaration STREQUAL "")
            message(FATAL_ERROR
                "Host validation runtime duplicates Stage state ownership: ${_state_type}")
        endif()
    endif()
endforeach()
host_validation_require_count("removed active state-access shim"
    "${_state}" "HostValidationStateAccess" 0)
host_validation_require_count("removed lexical state-access shim"
    "${_state_lexical}" "HostValidationStateAccess" 0)
foreach(_forbidden IN ITEMS "new (" "std::unique_ptr" "raylib.h")
    string(FIND "${_state_lexical}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation state seam owns or allocates forbidden data: ${_forbidden}")
    endif()
endforeach()

# Impl/factory owns the six-state aggregate, one config pointer, the cached
# pre-Stage17 physical snapshot, and the generic-capture completion latch.
# Task 7C needs short-lived pending presentation facts too, so this guard keeps
# their private names/layout flexible while freezing every brief-owned field.
host_validation_extract_sanitized_block("${_runtime}"
    "struct HostValidationRuntime::Impl final" _impl)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "struct HostValidationRuntime::Impl final" _impl_lexical)
host_validation_require_depth("Impl top-level owner" "${_runtime}"
    "struct HostValidationRuntime::Impl final" 1)
foreach(_field IN ITEMS
        "const RaylibHostConfig* config{};"
        "HostValidationStates states{};"
        "PhysicalKeySnapshot death_input_snapshot{};"
        "bool generic_capture_complete{};")
    host_validation_require_count("Impl ownership" "${_impl}" "${_field}" 1)
endforeach()
host_validation_require_count("Impl constructor" "${_impl}"
    "explicit Impl(" 1)
host_validation_require_order("Impl stable owner initialization" "${_impl}"
    "explicit Impl(const RaylibHostConfig& host_config,"
    "settings::SettingsLoadStatus load_status) noexcept"
    ": config(&host_config)"
    "states.stage11b.load_status = load_status;"
    "const RaylibHostConfig* config{};"
    "HostValidationStates states{};"
    "PhysicalKeySnapshot death_input_snapshot{};"
    "bool generic_capture_complete{};")
foreach(_forbidden IN ITEMS "std::vector" "std::shared_ptr" "make_unique")
    string(FIND "${_runtime_lexical}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation runtime introduces forbidden ownership: ${_forbidden}")
    endif()
endforeach()
host_validation_extract_sanitized_block("${_runtime}"
    "std::unique_ptr<HostValidationRuntime> HostValidationRuntime::create("
    _create)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "std::unique_ptr<HostValidationRuntime> HostValidationRuntime::create("
    _create_lexical)
host_validation_require_depth("factory top-level definition" "${_runtime}"
    "std::unique_ptr<HostValidationRuntime> HostValidationRuntime::create(" 1)
host_validation_require_count("nothrow allocations" "${_create}"
    "new (std::nothrow)" 2)
host_validation_require_order("factory allocation/failure order" "${_create}"
    "std::unique_ptr<Impl> impl{"
    "new (std::nothrow) Impl{config, load_status}"
    "if (impl == nullptr) return {};"
    "std::unique_ptr<HostValidationRuntime> runtime{"
    "new (std::nothrow) HostValidationRuntime{std::move(impl)}"
    "if (runtime == nullptr) return {};"
    "return runtime;")
foreach(_factory_token IN ITEMS
        "std::unique_ptr<Impl> impl{"
        "if (impl == nullptr) return {};"
        "std::unique_ptr<HostValidationRuntime> runtime{"
        "if (runtime == nullptr) return {};"
        "return runtime;")
    host_validation_require_depth("direct factory statement" "${_create}"
        "${_factory_token}" 1)
endforeach()
host_validation_reject_return_before("first factory allocation" "${_create}"
    "new (std::nothrow) Impl{config, load_status}")
host_validation_runtime_allocation_surface_valid(
    "${_runtime_lexical}" "${_create_lexical}" _allocation_surface_valid)
if(NOT _allocation_surface_valid)
    message(FATAL_ERROR
        "Host validation runtime allocations are not exactly the two factory-owned nothrow allocations")
endif()
set(_extra_allocation_runtime "${_runtime_lexical}")
string(APPEND _extra_allocation_runtime
    "\nvoid forbidden_extra_allocation() { auto* value = new HostValidationRuntime; }\n")
host_validation_runtime_allocation_surface_valid(
    "${_extra_allocation_runtime}" "${_create_lexical}"
    _extra_allocation_valid)
if(_extra_allocation_valid)
    message(FATAL_ERROR
        "Host validation allocation guard accepted create-external new")
endif()

host_validation_extract_sanitized_block("${_runtime}"
    "void HostValidationRuntime::set_render_readiness(" _readiness)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "void HostValidationRuntime::set_render_readiness(" _readiness_lexical)
host_validation_require_depth("readiness top-level definition" "${_runtime}"
    "void HostValidationRuntime::set_render_readiness(" 1)
host_validation_require_order("render readiness" "${_readiness}"
    "states.stage11c.cjk_font_ready = cjk_font_ready;"
    "states.stage17.active_skill_atlases_ready = active_skill_atlases_ready;")
foreach(_readiness_token IN ITEMS
        "states.stage11c.cjk_font_ready = cjk_font_ready;"
        "states.stage17.active_skill_atlases_ready = active_skill_atlases_ready;")
    host_validation_require_depth("direct readiness assignment"
        "${_readiness}" "${_readiness_token}" 1)
endforeach()
foreach(_forbidden IN ITEMS "new (" "std::string" "TextFormat" "make_unique")
    string(FIND "${_readiness_lexical}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation render readiness allocates or formats: ${_forbidden}")
    endif()
endforeach()

host_validation_extract_sanitized_block("${_runtime}"
    "PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges("
    _inject)
host_validation_require_depth("input top-level definition" "${_runtime}"
    "PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges(" 1)
host_validation_require_order("runtime input composition" "${_inject}"
    "inject_stage11b_physical_edges("
    "inject_stage11c_physical_edges("
    "inject_stage11d_physical_edges("
    "death_input_snapshot = stage11d_physical_keys;"
    "stage17.suspend_injection = gameplay_rearm_required;"
    "return host_validation::inject_stage17_physical_edges(")
foreach(_injector IN ITEMS
        "inject_stage11b_physical_edges("
        "inject_stage11c_physical_edges("
        "inject_stage11d_physical_edges("
        "inject_stage17_physical_edges(")
    host_validation_require_count("runtime injector uniqueness" "${_inject}"
        "${_injector}" 1)
endforeach()
host_validation_require_count("runtime injection return" "${_inject}"
    "return " 1)
foreach(_inject_token IN ITEMS
        "inject_stage11b_physical_edges("
        "inject_stage11c_physical_edges("
        "inject_stage11d_physical_edges("
        "death_input_snapshot = stage11d_physical_keys;"
        "stage17.suspend_injection = gameplay_rearm_required;"
        "return host_validation::inject_stage17_physical_edges(")
    host_validation_require_depth("direct input composition" "${_inject}"
        "${_inject_token}" 1)
endforeach()
host_validation_reject_return_before("input composition" "${_inject}"
    "inject_stage11b_physical_edges(")

foreach(_method IN ITEMS
        "observe_combat_event" "observe_snapshot" "observe_inventory")
    host_validation_require_depth("${_method} top-level definition"
        "${_runtime}" "void HostValidationRuntime::${_method}(" 1)
    host_validation_extract_sanitized_block("${_runtime}"
        "void HostValidationRuntime::${_method}(" _observer)
    host_validation_require_count("${_method} Stage17 delegation"
        "${_observer}" "observe_stage17_" 1)
    host_validation_require_depth("${_method} direct Stage17 delegation"
        "${_observer}" "observe_stage17_" 1)
endforeach()
host_validation_extract_sanitized_block("${_runtime}"
    "void HostValidationRuntime::observe_submitted_actions(" _submitted)
host_validation_require_depth("submitted observer top-level definition"
    "${_runtime}" "void HostValidationRuntime::observe_submitted_actions(" 1)
host_validation_require_order("submitted action observation/counting"
    "${_submitted}"
    "observe_stage17_submitted_actions("
    "injected_frame == 28U"
    "old_attack_checked = true;"
    "old_attack_count +="
    "submitted_actions.combat[0] ? 1U : 0U;"
    "injected_frame == 29U"
    "new_attack_count +="
    "submitted_actions.combat[0] ? 1U : 0U;")
host_validation_require_count("submitted action combat evidence"
    "${_submitted}" "submitted_actions.combat[0] ? 1U : 0U;" 2)
host_validation_require_depth("submitted Stage17 delegation" "${_submitted}"
    "observe_stage17_submitted_actions(" 1)
host_validation_reject_return_before("submitted Stage17 delegation"
    "${_submitted}" "observe_stage17_submitted_actions(")

# These bounded lexical blocks preserve conditional code while avoiding another
# whole-file lexer pass. Exact normalized contracts bind every delegate argument
# and make unreachable/dead control-flow additions observable.
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "void HostValidationRuntime::observe_combat_event(" _combat_observer_lexical)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "void HostValidationRuntime::observe_snapshot(" _snapshot_observer_lexical)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "void HostValidationRuntime::observe_inventory(" _inventory_observer_lexical)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "void HostValidationRuntime::observe_submitted_actions(" _submitted_lexical)

set(_readiness_contract [=[
void HostValidationRuntime::set_render_readiness(
    bool cjk_font_ready, bool active_skill_atlases_ready) noexcept {
    impl_->states.stage11c.cjk_font_ready = cjk_font_ready;
    impl_->states.stage17.active_skill_atlases_ready = active_skill_atlases_ready;
}
]=])
set(_combat_observer_contract [=[
void HostValidationRuntime::observe_combat_event(
    const combat::CombatEvent& event) noexcept {
    host_validation::observe_stage17_combat_event(&impl_->states.stage17, event);
}
]=])
set(_snapshot_observer_contract [=[
void HostValidationRuntime::observe_snapshot(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    host_validation::observe_stage17_snapshot(
        *impl_->config, impl_->states.stage17, snapshot);
}
]=])
set(_inventory_observer_contract [=[
void HostValidationRuntime::observe_inventory(
    const InventoryRenderer& inventory,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    host_validation::observe_stage17_inventory(
        *impl_->config, impl_->states.stage17, inventory, snapshot);
}
]=])
set(_submitted_observer_contract [=[
void HostValidationRuntime::observe_submitted_actions(
    const SubmittedFrameActions& submitted_actions) noexcept {
    host_validation::observe_stage17_submitted_actions(
        *impl_->config, impl_->states.stage17, submitted_actions);
    if (impl_->config->stage11b_validation
            == Stage11BValidationScenario::rebound_attack) {
        if (impl_->states.stage11b.injected_frame == 28U) {
            impl_->states.stage11b.old_attack_checked = true;
            impl_->states.stage11b.old_attack_count +=
                submitted_actions.combat[0] ? 1U : 0U;
        } else if (impl_->states.stage11b.injected_frame == 29U) {
            impl_->states.stage11b.new_attack_count +=
                submitted_actions.combat[0] ? 1U : 0U;
        }
    }
}
]=])
host_validation_require_active_exact_definition(
    "render-readiness exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::set_render_readiness("
    "${_readiness_contract}")
host_validation_require_active_exact_definition(
    "combat observer exact delegation"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_combat_event("
    "${_combat_observer_contract}")
host_validation_require_active_exact_definition(
    "snapshot observer exact delegation"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_snapshot("
    "${_snapshot_observer_contract}")
host_validation_require_active_exact_definition(
    "inventory observer exact delegation"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_inventory("
    "${_inventory_observer_contract}")
host_validation_require_active_exact_definition(
    "submitted observer exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_submitted_actions("
    "${_submitted_observer_contract}")

# Task 7B moves only validation decisions and state observations behind the
# facade.  Exact active-plus-lexical definitions prevent a correct copy in a
# comment, literal, inactive branch, lambda, or second scope from masking the
# real owner.
set(_should_continue_death_contract [=[
bool HostValidationRuntime::should_continue_death(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    const auto scenario = impl_->config->stage11_validation;
    const bool validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    return pending && validation_continue
        && !impl_->states.stage11.continue_requested;
}
]=])
set(_fixed_step_movement_contract [=[
combat::MovementInput HostValidationRuntime::fixed_step_movement(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    combat::MovementInput production_input) noexcept {
    if (impl_->config->stage11_validation
            != Stage11ValidationScenario::none) {
        return host_validation::stage11_validation_input(
            session, snapshot, *impl_->config, impl_->states.stage11);
    }
    if (impl_->config->stage10_validation
            != Stage10ValidationScenario::none) {
        return host_validation::stage10_validation_input(
            session, snapshot, *impl_->config, impl_->states.stage10);
    }
    return production_input;
}
]=])
set(_observe_fixed_tick_contract [=[
void HostValidationRuntime::observe_fixed_tick() noexcept {
    ++impl_->states.stage11b.fixed_ticks;
}
]=])
set(_observe_death_continue_result_contract [=[
void HostValidationRuntime::observe_death_continue_result(
    dungeon::RequestResult result) noexcept {
    const auto scenario = impl_->config->stage11_validation;
    const bool validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    if (validation_continue
            && result != dungeon::RequestResult::rejected) {
        impl_->states.stage11.continue_requested = true;
    }
}
]=])
set(_observe_post_fixed_tick_contract [=[
void HostValidationRuntime::observe_post_fixed_tick(
    const dungeon::DungeonSnapshot& snapshot,
    const items::ItemOwnershipState* ownership) noexcept {
    if (ownership == nullptr) return;
    host_validation::observe_stage11d_abyss_claim(
        impl_->states.stage11d, snapshot, *ownership);
}
]=])
set(_fixed_step_target_reached_contract [=[
bool HostValidationRuntime::fixed_step_target_reached(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    return host_validation::stage10_validation_reached(
        snapshot, *impl_->config, impl_->states.stage10)
        || host_validation::stage11_validation_reached(
            snapshot, *impl_->config, impl_->states.stage11);
}
]=])

function(host_validation_task7b_owner_namespace_valid
        SURFACE OUT_NAMESPACE OUT_VALID)
    set(_namespace_signature "namespace arpg::platform {")
    host_validation_count_token(
        "${SURFACE}" "${_namespace_signature}" _namespace_count)
    if(NOT _namespace_count EQUAL 1)
        set(${OUT_NAMESPACE} "" PARENT_SCOPE)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    string(FIND "${SURFACE}" "${_namespace_signature}" _namespace_position)
    host_validation_brace_depth(
        "${SURFACE}" ${_namespace_position} _namespace_depth)
    string(SUBSTRING "${SURFACE}" 0 ${_namespace_position}
        _namespace_prefix)
    set(_namespace_line_prefix "\n${_namespace_prefix}")
    if(NOT _namespace_depth EQUAL 0
            OR NOT _namespace_line_prefix MATCHES "\n[ \t]*$")
        set(${OUT_NAMESPACE} "" PARENT_SCOPE)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    host_validation_extract_sanitized_block(
        "${SURFACE}" "${_namespace_signature}" _namespace)
    set(${OUT_NAMESPACE} "${_namespace}" PARENT_SCOPE)
    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

function(host_validation_task7b_method_valid
        ACTIVE LEXICAL ACTIVE_NAMESPACE LEXICAL_NAMESPACE
        SIGNATURE EXPECTED OUT_VALID)
    host_validation_active_exact_definition_valid(
        "${ACTIVE}" "${LEXICAL}" "${SIGNATURE}" "${EXPECTED}"
        _exact_valid)
    if(NOT _exact_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    host_validation_count_token(
        "${ACTIVE_NAMESPACE}" "${SIGNATURE}" _active_owner_count)
    host_validation_count_token(
        "${LEXICAL_NAMESPACE}" "${SIGNATURE}" _lexical_owner_count)
    if(NOT _active_owner_count EQUAL 1
            OR NOT _lexical_owner_count EQUAL 1)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    string(FIND "${ACTIVE}" "${SIGNATURE}" _position)
    host_validation_brace_depth("${ACTIVE}" ${_position} _depth)
    if(NOT _depth EQUAL 1)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${OUT_VALID} TRUE PARENT_SCOPE)
endfunction()

# Production and every full-source mutation below use this same non-fatal
# owner validator.  It proves exact active-plus-lexical bodies and direct
# namespace ownership for all six Task 7B methods.
function(host_validation_task7b_runtime_contracts_valid
        RAW_RUNTIME OUT_VALID OUT_ERROR)
    host_validation_unconditional_cpp_surface(
        "${RAW_RUNTIME}" _active _lexical)
    host_validation_task7b_owner_namespace_valid(
        "${_active}" _active_namespace _active_namespace_valid)
    host_validation_task7b_owner_namespace_valid(
        "${_lexical}" _lexical_namespace _lexical_namespace_valid)
    if(NOT _active_namespace_valid OR NOT _lexical_namespace_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "arpg::platform namespace" PARENT_SCOPE)
        return()
    endif()
    host_validation_task7b_method_valid(
        "${_active}" "${_lexical}"
        "${_active_namespace}" "${_lexical_namespace}"
        "bool HostValidationRuntime::should_continue_death("
        "${_should_continue_death_contract}" _method_valid)
    if(NOT _method_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "should_continue_death" PARENT_SCOPE)
        return()
    endif()
    host_validation_task7b_method_valid(
        "${_active}" "${_lexical}"
        "${_active_namespace}" "${_lexical_namespace}"
        "combat::MovementInput HostValidationRuntime::fixed_step_movement("
        "${_fixed_step_movement_contract}" _method_valid)
    if(NOT _method_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "fixed_step_movement" PARENT_SCOPE)
        return()
    endif()
    host_validation_task7b_method_valid(
        "${_active}" "${_lexical}"
        "${_active_namespace}" "${_lexical_namespace}"
        "void HostValidationRuntime::observe_fixed_tick("
        "${_observe_fixed_tick_contract}" _method_valid)
    if(NOT _method_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "observe_fixed_tick" PARENT_SCOPE)
        return()
    endif()
    host_validation_task7b_method_valid(
        "${_active}" "${_lexical}"
        "${_active_namespace}" "${_lexical_namespace}"
        "void HostValidationRuntime::observe_death_continue_result("
        "${_observe_death_continue_result_contract}" _method_valid)
    if(NOT _method_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "observe_death_continue_result" PARENT_SCOPE)
        return()
    endif()
    host_validation_task7b_method_valid(
        "${_active}" "${_lexical}"
        "${_active_namespace}" "${_lexical_namespace}"
        "void HostValidationRuntime::observe_post_fixed_tick("
        "${_observe_post_fixed_tick_contract}" _method_valid)
    if(NOT _method_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "observe_post_fixed_tick" PARENT_SCOPE)
        return()
    endif()
    host_validation_task7b_method_valid(
        "${_active}" "${_lexical}"
        "${_active_namespace}" "${_lexical_namespace}"
        "bool HostValidationRuntime::fixed_step_target_reached("
        "${_fixed_step_target_reached_contract}" _method_valid)
    if(NOT _method_valid)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        set(${OUT_ERROR} "fixed_step_target_reached" PARENT_SCOPE)
        return()
    endif()
    set(${OUT_VALID} TRUE PARENT_SCOPE)
    set(${OUT_ERROR} "" PARENT_SCOPE)
endfunction()

host_validation_task7b_runtime_contracts_valid(
    "${_runtime_text}" _task7b_runtime_valid _task7b_runtime_error)
if(NOT _task7b_runtime_valid)
    message(FATAL_ERROR
        "Host validation Task 7B shared runtime validator rejected: ${_task7b_runtime_error}")
endif()

host_validation_require_depth("should_continue_death top-level definition"
    "${_runtime}" "bool HostValidationRuntime::should_continue_death(" 1)
host_validation_require_active_exact_definition(
    "should_continue_death exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "bool HostValidationRuntime::should_continue_death("
    "${_should_continue_death_contract}")
host_validation_require_depth("fixed_step_movement top-level definition"
    "${_runtime}"
    "combat::MovementInput HostValidationRuntime::fixed_step_movement(" 1)
host_validation_require_active_exact_definition(
    "fixed_step_movement exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "combat::MovementInput HostValidationRuntime::fixed_step_movement("
    "${_fixed_step_movement_contract}")
foreach(_method IN ITEMS
        observe_fixed_tick observe_death_continue_result observe_post_fixed_tick)
    host_validation_require_depth("${_method} top-level definition"
        "${_runtime}" "void HostValidationRuntime::${_method}(" 1)
endforeach()
host_validation_require_active_exact_definition(
    "observe_fixed_tick exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_fixed_tick("
    "${_observe_fixed_tick_contract}")
host_validation_require_active_exact_definition(
    "observe_death_continue_result exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_death_continue_result("
    "${_observe_death_continue_result_contract}")
host_validation_require_active_exact_definition(
    "observe_post_fixed_tick exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_post_fixed_tick("
    "${_observe_post_fixed_tick_contract}")
host_validation_require_depth("fixed_step_target_reached top-level definition"
    "${_runtime}"
    "bool HostValidationRuntime::fixed_step_target_reached(" 1)
host_validation_require_active_exact_definition(
    "fixed_step_target_reached exact reachable contract"
    "${_runtime}" "${_runtime_lexical}"
    "bool HostValidationRuntime::fixed_step_target_reached("
    "${_fixed_step_target_reached_contract}")

# Full-source mutation checks state the behavioral breaks guarded by the exact
# definitions.  In particular, Stage11 selection is a direct return rather
# than a zero-value fallback, and every non-rejected continue result (including
# faulted) records the one validation request.
function(host_validation_expect_task7b_runtime_mutation_rejected
        LABEL OLD_FRAGMENT NEW_FRAGMENT)
    string(REPLACE "${OLD_FRAGMENT}" "${NEW_FRAGMENT}"
        _mutation "${_runtime_text}")
    if(_mutation STREQUAL _runtime_text)
        message(FATAL_ERROR
            "Host validation Task 7B runtime mutation anchor is missing: ${LABEL}")
    endif()
    host_validation_task7b_runtime_contracts_valid(
        "${_mutation}" _mutation_valid _mutation_error)
    if(_mutation_valid)
        message(FATAL_ERROR
            "Host validation shared Task 7B runtime validator accepted mutation: ${LABEL}")
    endif()
endfunction()

host_validation_expect_task7b_runtime_mutation_rejected(
    "Stage11 zero-output fallback into Stage10"
    "return host_validation::stage11_validation_input("
    "const auto stage11_input = host_validation::stage11_validation_input(")
host_validation_expect_task7b_runtime_mutation_rejected(
    "fixed-tick observer duplicate increment"
    "++impl_->states.stage11b.fixed_ticks;"
    "++impl_->states.stage11b.fixed_ticks; ++impl_->states.stage11b.fixed_ticks;")
host_validation_expect_task7b_runtime_mutation_rejected(
    "death observer accepts only accepted instead of every non-rejected result"
    "result != dungeon::RequestResult::rejected"
    "result == dungeon::RequestResult::accepted")
host_validation_expect_task7b_runtime_mutation_rejected(
    "death observer submits the gameplay transaction"
    "const auto scenario ="
    "impl_->request_death_continue(); const auto scenario =")
host_validation_expect_task7b_runtime_mutation_rejected(
    "post-tick observer snapshots again"
    "if (ownership == nullptr) return;"
    "impl_->session->snapshot(snapshot); if (ownership == nullptr) return;")
host_validation_expect_task7b_runtime_mutation_rejected(
    "post-tick observer uses nullable ownership"
    "snapshot, *ownership);"
    "snapshot, *static_cast<const items::ItemOwnershipState*>(nullptr));")
host_validation_expect_task7b_runtime_mutation_rejected(
    "target test reverses Stage10 and Stage11 short circuit"
    "host_validation::stage10_validation_reached("
    "host_validation::stage11_validation_reached(")

function(host_validation_expect_task7b_runtime_fixture_rejected LABEL FIXTURE)
    host_validation_task7b_runtime_contracts_valid(
        "${FIXTURE}" _fixture_valid _fixture_error)
    if(_fixture_valid)
        message(FATAL_ERROR
            "Host validation shared Task 7B runtime validator accepted fixture: ${LABEL}")
    endif()
    if(ARGC GREATER 2 AND NOT _fixture_error STREQUAL "${ARGV2}")
        message(FATAL_ERROR
            "Host validation Task 7B fixture was rejected for the wrong reason: ${LABEL}: ${_fixture_error}")
    endif()
endfunction()

string(REPLACE "namespace arpg::platform {"
    "namespace task7b_wrong_platform {"
    _runtime_wrong_namespace_fixture "${_runtime_text}")
if(_runtime_wrong_namespace_fixture STREQUAL _runtime_text)
    message(FATAL_ERROR
        "Host validation Task 7B wrong-namespace fixture anchor is missing")
endif()
string(APPEND _runtime_wrong_namespace_fixture
    "\nnamespace arpg::platform {\n}\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definitions in another top-level namespace"
    "${_runtime_wrong_namespace_fixture}" "should_continue_death")

set(_runtime_duplicate_namespace_fixture
    "${_runtime_text}\nnamespace arpg::platform {\n}\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "duplicate active owner namespace"
    "${_runtime_duplicate_namespace_fixture}" "arpg::platform namespace")

string(REPLACE "namespace arpg::platform {"
    "namespace arpg::platform_extra {"
    _runtime_prefixed_namespace_fixture "${_runtime_text}")
if(_runtime_prefixed_namespace_fixture STREQUAL _runtime_text)
    message(FATAL_ERROR
        "Host validation Task 7B prefixed-namespace fixture anchor is missing")
endif()
host_validation_expect_task7b_runtime_fixture_rejected(
    "prefixed owner namespace"
    "${_runtime_prefixed_namespace_fixture}" "arpg::platform namespace")

string(REPLACE "bool HostValidationRuntime::should_continue_death("
    "bool HostValidationRuntime::should_continue_death_removed("
    _runtime_without_should_continue "${_runtime_text}")
if(_runtime_without_should_continue STREQUAL _runtime_text)
    message(FATAL_ERROR
        "Host validation Task 7B hidden-owner fixture anchor is missing")
endif()
set(_runtime_comment_fixture
    "${_runtime_without_should_continue}\n/*${_should_continue_death_contract}*/\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definition only in comment" "${_runtime_comment_fixture}")
set(_runtime_string_fixture
    "${_runtime_without_should_continue}\nconstexpr const char* task7b_definition_decoy = \"bool HostValidationRuntime::should_continue_death(\";\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct signature only in normal string" "${_runtime_string_fixture}")
set(_runtime_raw_fixture
    "${_runtime_without_should_continue}\nconstexpr const char* task7b_raw_decoy = R\"task7b(${_should_continue_death_contract})task7b\";\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definition only in raw string" "${_runtime_raw_fixture}")
set(_runtime_inactive_fixture
    "${_runtime_without_should_continue}\n#if 0\n${_should_continue_death_contract}\n#endif\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definition only in inactive code" "${_runtime_inactive_fixture}")
set(_runtime_lambda_fixture
    "${_runtime_without_should_continue}\nvoid task7b_lambda_owner() { auto decoy = [] { ${_should_continue_death_contract} }; }\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definition only in lambda scope" "${_runtime_lambda_fixture}")
set(_runtime_dead_fixture
    "${_runtime_without_should_continue}\nvoid task7b_dead_owner() { if (false) { ${_should_continue_death_contract} } }\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definition only in dead scope" "${_runtime_dead_fixture}")
set(_runtime_cross_scope_fixture
    "${_runtime_without_should_continue}\n${_should_continue_death_contract}\n")
host_validation_expect_task7b_runtime_fixture_rejected(
    "correct definition outside the owner namespace"
    "${_runtime_cross_scope_fixture}")

host_validation_expect_exact_mutation_rejected("readiness unbraced dead-if"
    "${_readiness_lexical}" "${_readiness_contract}"
    "impl_->states.stage11c.cjk_font_ready = cjk_font_ready;"
    "if (false) impl_->states.stage11c.cjk_font_ready = cjk_font_ready;")
host_validation_expect_exact_mutation_rejected("combat null state argument"
    "${_combat_observer_lexical}" "${_combat_observer_contract}"
    "&impl_->states.stage17, event" "nullptr, event")
host_validation_expect_exact_mutation_rejected("combat unbraced dead-if"
    "${_combat_observer_lexical}" "${_combat_observer_contract}"
    "host_validation::observe_stage17_combat_event(&impl_->states.stage17, event);"
    "if (false) host_validation::observe_stage17_combat_event(&impl_->states.stage17, event);")
string(REPLACE "&impl_->states.stage17, event" "nullptr, event"
    _combat_active_null "${_combat_observer_contract}")
host_validation_expect_inactive_correct_active_mutation_rejected(
    "inactive correct observer plus active null argument"
    "void HostValidationRuntime::observe_combat_event("
    "${_combat_observer_contract}" "${_combat_active_null}")
string(REPLACE
    "host_validation::observe_stage17_combat_event(&impl_->states.stage17, event);"
    "if (false) host_validation::observe_stage17_combat_event(&impl_->states.stage17, event);"
    _combat_active_dead "${_combat_observer_contract}")
host_validation_expect_inactive_correct_active_mutation_rejected(
    "inactive correct observer plus active dead delegation"
    "void HostValidationRuntime::observe_combat_event("
    "${_combat_observer_contract}" "${_combat_active_dead}")
host_validation_expect_exact_mutation_rejected("snapshot wrong argument"
    "${_snapshot_observer_lexical}" "${_snapshot_observer_contract}"
    "impl_->states.stage17, snapshot);"
    "impl_->states.stage17, dungeon::DungeonSnapshot{});")
host_validation_expect_exact_mutation_rejected("inventory wrong argument"
    "${_inventory_observer_lexical}" "${_inventory_observer_contract}"
    "inventory, snapshot);" "inventory, dungeon::DungeonSnapshot{});")
host_validation_expect_exact_mutation_rejected("submitted wrong argument"
    "${_submitted_lexical}" "${_submitted_observer_contract}"
    "impl_->states.stage17, submitted_actions);"
    "impl_->states.stage17, SubmittedFrameActions{});")
host_validation_expect_exact_mutation_rejected("submitted false-and bypass"
    "${_submitted_lexical}" "${_submitted_observer_contract}"
    "if (impl_->config->stage11b_validation"
    "if (false && impl_->config->stage11b_validation")
host_validation_expect_exact_mutation_rejected("submitted early return"
    "${_submitted_lexical}" "${_submitted_observer_contract}"
    "host_validation::observe_stage17_submitted_actions("
    "return; host_validation::observe_stage17_submitted_actions(")

# The public death snapshot getter is the only exposed view into validation
# state.  Its exact return keeps death/pause on the Stage11D-after,
# Stage17-before snapshot rather than the injected Stage17 return value.
set(_death_snapshot_signature [=[const PhysicalKeySnapshot&
HostValidationRuntime::death_input_snapshot(]=])
set(_death_snapshot_contract [=[
const PhysicalKeySnapshot&
HostValidationRuntime::death_input_snapshot() const noexcept {
    return impl_->death_input_snapshot;
}
]=])
host_validation_require_active_exact_definition(
    "public death snapshot exact implementation"
    "${_runtime}" "${_runtime_lexical}"
    "${_death_snapshot_signature}" "${_death_snapshot_contract}")
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "${_death_snapshot_signature}" _death_snapshot_lexical)
host_validation_expect_exact_mutation_rejected(
    "public death getter returns detached snapshot"
    "${_death_snapshot_lexical}" "${_death_snapshot_contract}"
    "return impl_->death_input_snapshot;"
    "static PhysicalKeySnapshot detached_snapshot{}; return detached_snapshot;")

# Every Task 7C facade method has one real, unconditional, namespace-level
# runtime definition.  Stage-specific guards lock the detailed algorithms;
# this central guard locks the exact public signatures and ownership surface.
foreach(_method IN ITEMS
        observe_pause_transition
        prepare_hud_snapshot
        observe_hud
        observe_active_skill_draw
        observe_ground_loot
        observe_presented_frame
        observe_capture_result
        write_summaries)
    host_validation_require_count("Task 7C active runtime definition"
        "${_runtime}" "HostValidationRuntime::${_method}(" 1)
    host_validation_require_count("Task 7C lexical runtime definition"
        "${_runtime_lexical}" "HostValidationRuntime::${_method}(" 1)
    host_validation_require_depth("Task 7C runtime definition"
        "${_runtime}" "HostValidationRuntime::${_method}(" 1)
endforeach()
host_validation_normalize_cpp_surface("${_runtime}" _runtime_normalized)
host_validation_require_regex_count("pause-transition exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::observe_pause_transition\\([ ]*bool[ ]+was_open,[ ]*bool[ ]+is_open[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("HUD preparation exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::prepare_hud_snapshot\\([ ]*dungeon::DungeonSnapshot&[ ]+snapshot[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("HUD observer exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::observe_hud\\([ ]*const[ ]+dungeon::DungeonSnapshot&[ ]+snapshot,[ ]*const[ ]+HudViewModel&[ ]+model,[ ]*HudNoticeView[ ]+notices,[ ]*bool[ ]+draw_debug,[ ]*int[ ]+screen_width,[ ]*int[ ]+screen_height[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("active-skill observer exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::observe_active_skill_draw\\([ ]*const[ ]+dungeon::DungeonSnapshot&[ ]+snapshot,[ ]*const[ ]+ActiveSkillDrawRuntimeStatus&[ ]+draw_status[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("ground-loot observer exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::observe_ground_loot\\([ ]*const[ ]+dungeon::DungeonSnapshot&[ ]+snapshot,[ ]*const[ ]+PauseMenuState&[ ]+pause_menu,[ ]*const[ ]+DungeonRenderStatus&[ ]+render_status,[ ]*settings::LootFilterMode[ ]+loot_filter,[ ]*const[ ]+GroundLootView&[ ]+ground_loot_view,[ ]*HudNoticeView[ ]+notices,[ ]*const[ ]+items::ItemOwnershipState[*][ ]+ownership,[ ]*int[ ]+screen_width,[ ]*int[ ]+screen_height[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("presented observer exact definition"
    "${_runtime_normalized}"
    "PresentationDecision[ ]+HostValidationRuntime::observe_presented_frame\\([ ]*const[ ]+dungeon::DungeonSnapshot&[ ]+snapshot,[ ]*const[ ]+PauseMenuState&[ ]+pause_menu,[ ]*bool[ ]+pause_cjk_ready[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("capture-result exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::observe_capture_result\\([ ]*CaptureOwner[ ]+owner,[ ]*bool[ ]+succeeded[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_regex_count("summary exact definition"
    "${_runtime_normalized}"
    "void[ ]+HostValidationRuntime::write_summaries\\([ ]*CleanShutdownState[ ]+clean_shutdown_state,[ ]*const[ ]+PauseMenuState&[ ]+pause_menu[ ]*\\)[ ]+noexcept[ ]*\\{" 1)
host_validation_require_count("removed runtime transition shim"
    "${_runtime}" "HostValidationStateAccess" 0)
host_validation_require_count("removed lexical runtime transition shim"
    "${_runtime_lexical}" "HostValidationStateAccess" 0)

# Freeze the decision values at the pre-capture point.  Private pending-field
# names stay implementation details, but the six completion values, visibility
# formula, prior generic result, single Stage17 query, owner priority and return
# value are the public observable contract.
host_validation_extract_sanitized_block("${_runtime}"
    "PresentationDecision HostValidationRuntime::observe_presented_frame("
    _presented_observer)
host_validation_mask_non_direct_executable_scopes(
    "${_presented_observer}" _presented_observer_direct)
host_validation_require_canonical_order("presented decision construction"
    "${_presented_observer_direct}"
    "const bool stage10_reached ="
    "const bool stage11_reached ="
    "const bool stage11b_reached ="
    "const bool stage11c_reached ="
    "const bool stage11d_reached ="
    "const bool stage17_reached ="
    "const bool stage11b_visible_capture ="
    "const bool stage11b_paused_visible_capture ="
    "PresentationDecision decision{};"
    "decision.validation_complete ="
    "decision.generic_capture_visible ="
    "decision.generic_capture_complete = impl_->generic_capture_complete;"
    "decision.stage17_capture_path = host_validation::stage17_capture_path("
    "decision.capture_owner = CaptureOwner::stage17;"
    "decision.capture_owner = CaptureOwner::generic_validation;"
    "return decision;")
foreach(_exact_decision IN ITEMS
        "decision.validation_complete = stage10_reached || stage11_reached || stage11b_reached || stage11c_reached || stage11d_reached || stage17_reached;"
        "decision.generic_capture_visible = decision.validation_complete || impl_->states.stage11d.target_visible || stage11b_visible_capture || stage11b_paused_visible_capture;"
        "decision.generic_capture_complete = impl_->generic_capture_complete;"
        "decision.stage17_capture_path = host_validation::stage17_capture_path( *impl_->config, impl_->states.stage17);"
        "if (decision.stage17_capture_path.has_value()) { decision.capture_owner = CaptureOwner::stage17; } else if (decision.generic_capture_visible && !decision.generic_capture_complete && impl_->config->validation_capture_file.has_value()) { decision.capture_owner = CaptureOwner::generic_validation; }"
        "return decision;")
    host_validation_require_canonical_direct_statement(
        "exact presented decision" "${_presented_observer_direct}"
        "${_exact_decision}" 1)
endforeach()
set(_stage17_path_multiline [=[decision.stage17_capture_path = host_validation::stage17_capture_path(
        *impl_->config, impl_->states.stage17);]=])
set(_stage17_path_compact [=[decision.stage17_capture_path=host_validation::stage17_capture_path(*impl_->config,impl_->states.stage17);]=])
string(REPLACE "${_stage17_path_multiline}" "${_stage17_path_compact}"
    _presented_harmless_formatting "${_presented_observer_direct}")
if("${_presented_harmless_formatting}" STREQUAL
        "${_presented_observer_direct}")
    message(FATAL_ERROR
        "Host validation Stage17 formatting mutation anchor is missing")
endif()
host_validation_require_canonical_direct_statement(
    "harmless compact Stage17 path formatting"
    "${_presented_harmless_formatting}" "${_stage17_path_compact}" 1)
host_validation_require_count("single Stage17 path query"
    "${_presented_observer}" "host_validation::stage17_capture_path(" 1)

host_validation_extract_sanitized_block("${_runtime}"
    "void HostValidationRuntime::observe_capture_result("
    _capture_result_observer)
host_validation_mask_non_direct_executable_scopes(
    "${_capture_result_observer}" _capture_result_direct)
host_validation_normalize_cpp_surface(
    "${_capture_result_direct}" _capture_result_normalized)
host_validation_require_canonical_count("generic capture completion promotion"
    "${_capture_result_direct}"
    "impl_->generic_capture_complete = true;" 1)
host_validation_require_canonical_count("paused capture exact assignment"
    "${_capture_result_direct}"
    "impl_->states.stage11b.pause_capture_while_paused = impl_->pending_stage11b_paused_visible_capture;" 1)
if(_capture_result_normalized MATCHES
        "pause_capture_while_paused[ ]*=[^;]*[|][|]")
    message(FATAL_ERROR
        "Host validation capture result OR-latches paused capture state")
endif()
host_validation_require_count("Stage17 capture completion callback"
    "${_capture_result_observer}"
    "host_validation::mark_stage17_capture_complete(" 1)

host_validation_extract_sanitized_block("${_runtime}"
    "void HostValidationRuntime::write_summaries(" _summary_writer)
host_validation_mask_non_direct_executable_scopes(
    "${_summary_writer}" _summary_writer_direct)
host_validation_require_canonical_order("summary writer exact order"
    "${_summary_writer_direct}"
    "impl_->states.stage17.clean_shutdown_exact_ready = clean_shutdown_state == CleanShutdownState::ready;"
    "host_validation::write_stage11b_validation_summary("
    "host_validation::write_stage11c_hud_validation_summary("
    "host_validation::write_stage11d_loot_validation_summary("
    "host_validation::write_stage17_validation_summary(")
foreach(_summary_call IN ITEMS
        "host_validation::write_stage11b_validation_summary("
        "host_validation::write_stage11c_hud_validation_summary("
        "host_validation::write_stage11d_loot_validation_summary("
        "host_validation::write_stage17_validation_summary(")
    host_validation_require_count("unique summary writer call"
        "${_summary_writer_direct}" "${_summary_call}" 1)
endforeach()

# Bind only the executable Host entry and drain helper. Comments, literals,
# inactive branches and cross-function decoys have already been removed.
host_validation_extract_sanitized_block("${_host}"
    "HostExitCode run_raylib_host(" _run_host)
host_validation_extract_sanitized_block("${_host}" "void drain_events(" _drain)
host_validation_mask_non_direct_executable_scopes(
    "${_run_host}" _run_host_direct)
host_validation_require_depth("Host entry top-level definition" "${_host}"
    "HostExitCode run_raylib_host(" 1)
host_validation_require_depth("drain helper top-level definition" "${_host}"
    "void drain_events(" 2)

# Task 8A keeps the public gate contract in the Host facade, gives the exact
# implementation to one dedicated translation unit, and leaves Host with calls
# only.  Both active and all-branch lexical views are checked so inactive or
# string/comment decoys cannot lend ownership evidence.
set(_host_frame_gate_signature "HostFrameGateResult gate_host_frame(")
host_validation_require_depth("Task 8A frame gate declaration"
    "${_host_header}" "${_host_frame_gate_signature}" 1)
host_validation_require_count("Task 8A lexical frame gate declaration"
    "${_host_header_lexical}" "${_host_frame_gate_signature}" 1)
host_validation_require_identifier_count(
    "Task 8A lexical frame gate declaration identifier"
    "${_host_header_lexical}" "gate_host_frame" 1)
host_validation_require_preprocessor_macro_free(
    "Task 8A header" "${_host_header_lexical}")
host_validation_normalize_cpp_surface(
    "${_host_header_lexical}" _host_header_lexical_normalized)
set(_host_frame_gate_declaration_pattern
    "HostFrameGateResult[ ]+gate_host_frame\\([ ]*core::FixedStepRunner&([ ]*[A-Za-z_][A-Za-z0-9_]*)?[ ]*,[ ]*bool&([ ]*[A-Za-z_][A-Za-z0-9_]*)?[ ]*,[ ]*bool([ ]+[A-Za-z_][A-Za-z0-9_]*)?[ ]*,[ ]*double([ ]+[A-Za-z_][A-Za-z0-9_]*)?[ ]*\\)[ ]*noexcept")
if(NOT _host_header_lexical_normalized MATCHES
        "${_host_frame_gate_declaration_pattern}[ ]*;")
    message(FATAL_ERROR
        "Host validation Task 8A lexical declaration has the wrong interface")
endif()

set(_host_frame_gate_contract [=[
HostFrameGateResult gate_host_frame(
    core::FixedStepRunner& fixed_step,
    bool& pause_latched,
    bool paused,
    double frame_seconds) noexcept {
    if (paused) {
        if (!pause_latched) fixed_step.clear_accumulator();
        pause_latched = true;
        return {};
    }
    pause_latched = false;
    return {true, fixed_step.advance(frame_seconds)};
}
]=])
string(CONCAT _host_frame_gate_translation_unit_contract
    "#include \"raylib_host.hpp\"\n\n"
    "namespace arpg::platform {\n\n"
    "${_host_frame_gate_contract}\n"
    "}  // namespace arpg::platform\n")
arpg_sanitize_cpp_source(
    "${_host_frame_gate_translation_unit_contract}"
    _host_frame_gate_expected_lexical)
host_validation_require_exact_surface(
    "Task 8A lexical owner translation unit"
    "${_host_frame_gate_lexical}" "${_host_frame_gate_expected_lexical}")
host_validation_require_count("Task 8A owner public include"
    "${_host_frame_gate_text}" "#include \"raylib_host.hpp\"" 1)
host_validation_require_count("Task 8A run loop frame gate calls"
    "${_run_host_direct}" "gate_host_frame(" 2)
host_validation_require_identifier_count(
    "Task 8A lexical Host frame gate identifiers"
    "${_host_lexical}" "gate_host_frame" 2)
host_validation_require_preprocessor_macro_free(
    "Task 8A Host" "${_host_lexical}")

file(GLOB_RECURSE _task8a_project_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/src/*.h" "${SOURCE_ROOT}/src/*.hpp"
    "${SOURCE_ROOT}/src/*.cpp")
file(REAL_PATH "${_host_header_path}" _task8a_host_header_real)
file(REAL_PATH "${_host_frame_gate}" _task8a_host_frame_gate_real)
file(REAL_PATH "${_host_source}" _task8a_host_source_real)
foreach(_task8a_source IN LISTS _task8a_project_sources)
    file(REAL_PATH "${_task8a_source}" _task8a_source_real)
    if("${_task8a_source_real}" STREQUAL "${_task8a_host_header_real}"
            OR "${_task8a_source_real}" STREQUAL "${_task8a_host_frame_gate_real}"
            OR "${_task8a_source_real}" STREQUAL "${_task8a_host_source_real}")
        continue()
    endif()
    file(READ "${_task8a_source}" _task8a_source_text)
    host_validation_fold_cpp_phase2_splices(
        "${_task8a_source_text}" _task8a_source_phase2)
    if(NOT _task8a_source_phase2 MATCHES
            "gate_host_frame|##|%:%:")
        continue()
    endif()
    arpg_sanitize_cpp_source("${_task8a_source_text}"
        _task8a_source_lexical)
    if(_task8a_source_lexical MATCHES "##|%:%:")
        message(FATAL_ERROR
            "Host validation Task 8A forbids project token-paste macros: ${_task8a_source}")
    endif()
    host_validation_require_identifier_count(
        "Task 8A foreign frame gate ownership: ${_task8a_source}"
        "${_task8a_source_lexical}" "gate_host_frame" 0)
endforeach()

# Scope the existing Task 7B ownership boundary to the executable death segment
# and the real fixed-step loop. Task 7C additionally removes every remaining
# presentation/capture/summary Stage owner from Host below.
string(FIND "${_run_host}"
    "DeathInputGate death_gate = host_death_input_gate(" _death_segment_begin)
string(FIND "${_run_host}" "bool escape_consumed = false;"
    _death_segment_end)
if(_death_segment_begin EQUAL -1 OR _death_segment_end EQUAL -1
        OR NOT _death_segment_begin LESS _death_segment_end)
    message(FATAL_ERROR
        "Host validation cannot isolate the Task 7B death-continue segment")
endif()
math(EXPR _death_segment_length
    "${_death_segment_end} - ${_death_segment_begin}")
string(SUBSTRING "${_run_host}" ${_death_segment_begin}
    ${_death_segment_length} _death_segment)
host_validation_extract_sanitized_block("${_run_host}"
    "for (std::uint32_t step = 0; step < frame.steps; ++step)"
    _task7b_fixed_step_loop)

foreach(_death_call IN ITEMS
        "validation_runtime->should_continue_death(current)"
        "runtime.request_death_continue()"
        "validation_runtime->observe_death_continue_result(")
    host_validation_require_count("Task 7B death data flow"
        "${_death_segment}" "${_death_call}" 1)
endforeach()
host_validation_require_depth("direct death decision facade call"
    "${_death_segment}"
    "validation_runtime->should_continue_death(current)" 0)
host_validation_require_depth("direct death-continue transaction"
    "${_death_segment}" "runtime.request_death_continue()" 1)
host_validation_require_depth("direct death-result observer"
    "${_death_segment}"
    "validation_runtime->observe_death_continue_result(" 1)
foreach(_old_death_owner IN ITEMS
        "validation_continue" "stage11_validation_state.continue_requested")
    host_validation_require_count("removed Host death owner"
        "${_death_segment}" "${_old_death_owner}" 0)
endforeach()

foreach(_fixed_call IN ITEMS
        "validation_runtime->fixed_step_movement("
        "validation_runtime->observe_fixed_tick()"
        "validation_runtime->observe_post_fixed_tick("
        "validation_runtime->fixed_step_target_reached(")
    host_validation_require_count("Task 7B fixed-step facade flow"
        "${_task7b_fixed_step_loop}" "${_fixed_call}" 1)
endforeach()
host_validation_require_depth("fixed-step movement RHS"
    "${_task7b_fixed_step_loop}"
    "validation_runtime->fixed_step_movement(" 2)
foreach(_direct_fixed_call IN ITEMS
        "validation_runtime->observe_fixed_tick()"
        "validation_runtime->observe_post_fixed_tick("
        "validation_runtime->fixed_step_target_reached(")
    host_validation_require_depth("direct Task 7B fixed-step facade call"
        "${_task7b_fixed_step_loop}" "${_direct_fixed_call}" 1)
endforeach()
foreach(_old_fixed_owner IN ITEMS
        "host_validation::stage11_validation_input("
        "host_validation::stage10_validation_input("
        "host_validation::stage10_validation_reached("
        "host_validation::stage11_validation_reached("
        "stage11b_validation_state.fixed_ticks"
        "host_validation::observe_stage11d_abyss_claim(")
    host_validation_require_count("removed Host fixed-step owner"
        "${_task7b_fixed_step_loop}" "${_old_fixed_owner}" 0)
endforeach()

foreach(_old_call IN ITEMS
        "inject_stage11b_physical_edges("
        "inject_stage11c_physical_edges("
        "inject_stage11d_physical_edges("
        "inject_stage17_physical_edges("
        "observe_stage17_combat_event("
        "observe_stage17_snapshot("
        "observe_stage17_inventory("
        "observe_stage17_submitted_actions(")
    string(FIND "${_host_lexical}" "${_old_call}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation Host still calls migrated implementation: ${_old_call}")
    endif()
endforeach()
foreach(_old_count IN ITEMS
        "submitted_actions.combat[0]" "old_attack_checked = true"
        "old_attack_count +=" "new_attack_count +=")
    string(FIND "${_host_lexical}" "${_old_count}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation Host still owns Stage11B submitted counting: ${_old_count}")
    endif()
endforeach()
string(FIND "${_host_lexical}" "struct HostValidationStates final"
    _old_aggregate)
if(NOT _old_aggregate EQUAL -1)
    message(FATAL_ERROR "HostValidationStates remains in raylib_host.cpp")
endif()
string(FIND "${_host_lexical}" "HostValidationStates" _old_state_owner)
if(NOT _old_state_owner EQUAL -1)
    message(FATAL_ERROR "Host still owns HostValidationStates directly")
endif()

# Host may include and call only the public validation facade. It must not know
# a Stage state type/field, the removed transition shim, or any Stage
# presentation/capture/report implementation after Task 7C.
foreach(_forbidden_host_token IN ITEMS
        "HostValidationStateAccess"
        "host_validation::"
        "host_validation_"
        "Stage10ValidationState"
        "Stage11ValidationState"
        "Stage11BValidationState"
        "Stage11CHudValidationState"
        "Stage11DLootValidationState"
        "Stage17SkillStonesValidationState"
        "stage10_validation_state"
        "stage11_validation_state"
        "stage11b_validation_state"
        "stage11c_validation_state"
        "stage11d_validation_state"
        "stage17_validation_state"
        "stage10_validation_captured"
        "observe_stage17_draw_runtime("
        "stage17_capture_path("
        "mark_stage17_capture_complete("
        "stage17_validation_complete("
        "stage11d_target_visible("
        "stage11d_record_semantics("
        "stage11d_validation_active("
        "write_stage11b_validation_summary("
        "write_stage11c_hud_validation_summary("
        "write_stage11d_loot_validation_summary("
        "write_stage17_validation_summary(")
    host_validation_require_count("removed direct Host validation owner"
        "${_host_lexical}" "${_forbidden_host_token}" 0)
endforeach()

# Every new facade seam is consumed once by the ordinary Host path. Detailed
# relative-position and adversarial ownership mutations live in the dedicated
# sequence and Stage guards; these assertions prevent omitted/discarded facade
# integration in the central architecture guard.
foreach(_task7c_call IN ITEMS
        "validation_runtime->observe_pause_transition("
        "validation_runtime->prepare_hud_snapshot("
        "validation_runtime->observe_hud("
        "validation_runtime->observe_active_skill_draw("
        "validation_runtime->observe_ground_loot("
        "validation_runtime->observe_presented_frame("
        "validation_runtime->observe_capture_result("
        "validation_runtime->write_summaries(")
    host_validation_require_count("Task 7C Host facade call"
        "${_run_host}" "${_task7c_call}" 1)
    host_validation_require_count("direct Task 7C Host facade call"
        "${_run_host_direct}" "${_task7c_call}" 1)
endforeach()
host_validation_require_count("typed presented decision owner" "${_run_host}"
    "const PresentationDecision decision =" 1)
host_validation_require_count("direct typed presented decision owner"
    "${_run_host_direct}" "const PresentationDecision decision =" 1)
host_validation_normalize_cpp_surface(
    "${_run_host_direct}" _run_host_normalized)
host_validation_require_regex_count("pause-transition Host binding"
    "${_run_host_normalized}"
    "validation_runtime->observe_pause_transition\\([ ]*pause_was_open,[ ]*pause_open[ ]*\\)" 1)
host_validation_require_regex_count("HUD preparation Host binding"
    "${_run_host_normalized}"
    "validation_runtime->prepare_hud_snapshot\\([ ]*current[ ]*\\)" 1)
host_validation_require_regex_count("HUD observer Host binding"
    "${_run_host_normalized}"
    "validation_runtime->observe_hud\\([ ]*current,[ ]*renderer[.]hud_model\\([ ]*\\),[ ]*renderer[.]hud_notice_view\\([ ]*\\),[ ]*draw_debug,[ ]*GetScreenWidth\\([ ]*\\),[ ]*GetScreenHeight\\([ ]*\\)[ ]*\\)" 1)
host_validation_require_regex_count("active-skill observer Host binding"
    "${_run_host_normalized}"
    "validation_runtime->observe_active_skill_draw\\([ ]*presented_snapshot,[ ]*renderer[.]active_skill_draw_status\\([ ]*\\)[ ]*\\)" 1)
host_validation_require_regex_count("ground-loot observer Host binding"
    "${_run_host_normalized}"
    "validation_runtime->observe_ground_loot\\([ ]*current,[ ]*pause_menu,[ ]*runtime[.]render_status\\([ ]*\\),[ ]*presented_loot_filter,[ ]*ground_loot_view,[ ]*renderer[.]hud_notice_view\\([ ]*\\),[ ]*runtime[.]item_state\\([ ]*\\),[ ]*GetScreenWidth\\([ ]*\\),[ ]*GetScreenHeight\\([ ]*\\)[ ]*\\)" 1)
host_validation_require_regex_count("presented-decision Host binding"
    "${_run_host_normalized}"
    "const PresentationDecision decision[ ]*=[ ]*validation_runtime->observe_presented_frame\\([ ]*current,[ ]*pause_menu,[ ]*pause_cjk_ready[ ]*\\)" 1)
host_validation_require_regex_count("capture-result Host binding"
    "${_run_host_normalized}"
    "validation_runtime->observe_capture_result\\([ ]*selected_capture_owner,[ ]*selected_validation_capture_succeeded[ ]*\\)" 1)
host_validation_require_regex_count("summary Host binding"
    "${_run_host_normalized}"
    "validation_runtime->write_summaries\\([ ]*runtime[.]clean_shutdown_state\\([ ]*\\),[ ]*pause_menu[ ]*\\)" 1)
host_validation_require_canonical_count("same-frame generic completion formula"
    "${_run_host_direct}"
    "const bool effective_generic_complete = decision.generic_capture_complete || selected_generic_capture_succeeded_now" 1)
host_validation_require_canonical_count("same-frame validation exit predicate"
    "${_run_host_direct}"
    "if (decision.validation_complete && (!config.validation_capture_file.has_value() || effective_generic_complete)) { begin_clean_exit(); }" 1)
host_validation_require_canonical_order("Task 7C Host lifecycle"
    "${_run_host_direct}"
    "validation_runtime->observe_pause_transition("
    "const float frame_seconds = GetFrameTime();"
    "audio.update("
    "validation_runtime->prepare_hud_snapshot("
    "presented_snapshot = current;"
    "renderer.observe_presented_hud_frame("
    "validation_runtime->observe_hud("
    "BeginDrawing();"
    "validation_runtime->observe_active_skill_draw("
    "validation_runtime->observe_ground_loot("
    "const PresentationDecision decision ="
    "std::optional<std::string> capture_path ="
    "const bool selected_validation_capture_succeeded ="
    "validation_runtime->observe_capture_result("
    "const bool effective_generic_complete ="
    "validation_runtime->write_summaries("
    "audio.shutdown();")

host_validation_require_order("Host initialization" "${_run_host}"
    "HostValidationRuntime::create(config, loaded.status)"
    "if (validation_runtime == nullptr)"
    "return HostExitCode::save_initialization_failed;"
    "InitWindow("
    "renderer.initialize_resources()"
    "validation_runtime->set_render_readiness("
    "hud_resources_ready, renderer.active_skill_assets_ready()")
host_validation_require_count("Host runtime creation" "${_run_host}"
    "HostValidationRuntime::create(" 1)
host_validation_require_count("Host render readiness" "${_run_host}"
    "validation_runtime->set_render_readiness(" 1)

host_validation_require_order("Host physical input split" "${_run_host}"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "const bool gameplay_rearm_was_required ="
    "runtime.gameplay_rearm_required();"
    "const PhysicalKeySnapshot stage17_physical_keys ="
    "validation_runtime->inject_physical_edges("
    "const PhysicalKeySnapshot& physical_keys ="
    "validation_runtime->death_input_snapshot("
    "gameplay_controls_physically_released("
    "stage17_physical_keys"
    "runtime.acknowledge_gameplay_rearmed();"
    "map_host_frame_input("
    "input_settings, stage17_physical_keys)")
host_validation_require_count("Host facade injection" "${_run_host}"
    "validation_runtime->inject_physical_edges(" 1)
host_validation_require_count("public cached Stage11D accessor" "${_run_host}"
    "validation_runtime->death_input_snapshot(" 1)
foreach(_cached_consumer IN ITEMS
        "frame_input.keys, physical_keys)"
        "pause_was_open && physical_keys.mouse_left"
        "!physical_keys.focus_lost"
        "pause_input_from_snapshot(")
    string(FIND "${_run_host}" "${_cached_consumer}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation cached Stage11D consumer is missing: ${_cached_consumer}")
    endif()
endforeach()

host_validation_run_host_snapshot_bindings_valid(
    "${_run_host}" _snapshot_bindings_valid _input_boundary_normalized)
if(NOT _snapshot_bindings_valid)
    message(FATAL_ERROR
        "Host validation dual-snapshot consumers are not bound to their exact sources")
endif()
host_validation_expect_snapshot_mutation_rejected("rearm uses cached snapshot"
    "${_input_boundary_normalized}"
    "stage17_physical_keys)) {" "physical_keys)) {")
host_validation_expect_snapshot_mutation_rejected("mapping uses cached snapshot"
    "${_input_boundary_normalized}"
    "input_settings, stage17_physical_keys);"
    "input_settings, physical_keys);")
host_validation_expect_snapshot_mutation_rejected("death gate uses Stage17 snapshot"
    "${_input_boundary_normalized}"
    "frame_input.keys, physical_keys);"
    "frame_input.keys, stage17_physical_keys);")
host_validation_expect_snapshot_mutation_rejected("pause click uses Stage17 snapshot"
    "${_input_boundary_normalized}"
    "physical_keys.mouse_left" "stage17_physical_keys.mouse_left")
host_validation_expect_snapshot_mutation_rejected("pause hit-test uses Stage17 snapshot"
    "${_input_boundary_normalized}"
    "physical_keys.mouse_position" "stage17_physical_keys.mouse_position")
host_validation_expect_snapshot_mutation_rejected("focus uses Stage17 snapshot"
    "${_input_boundary_normalized}"
    "physical_keys.focus_lost" "stage17_physical_keys.focus_lost")
host_validation_expect_snapshot_mutation_rejected("pause input uses Stage17 snapshot"
    "${_input_boundary_normalized}"
    "physical_keys, escape_consumed,"
    "stage17_physical_keys, escape_consumed,")
set(_real_pause_input_call [=[PauseInput pause_input = pause_input_from_snapshot(
                physical_keys, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding);]=])
set(_discarded_pause_input_result [=[static_cast<void>(pause_input_from_snapshot(
                physical_keys, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding));
            PauseInput pause_input{};]=])
host_validation_expect_run_host_snapshot_mutation_rejected(
    "discarded correct pause-input result plus default binding" "${_run_host}"
    "${_real_pause_input_call}" "${_discarded_pause_input_result}")
set(_wrong_pause_input_with_lambda_decoy [=[PauseInput pause_input = pause_input_from_snapshot(
                PhysicalKeySnapshot{}, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding);
            const auto unused_pause_input_decoy = [&]() noexcept {
                return pause_input_from_snapshot(
                    physical_keys, escape_consumed,
                    pause_menu.screen == PauseScreen::capture_binding);
            };]=])
host_validation_expect_run_host_snapshot_mutation_rejected(
    "wrong real pause snapshot plus uncalled lambda decoy" "${_run_host}"
    "${_real_pause_input_call}" "${_wrong_pause_input_with_lambda_decoy}")
set(_wrong_pause_input_with_dead_decoy [=[PauseInput pause_input = pause_input_from_snapshot(
                PhysicalKeySnapshot{}, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding);
            if (false) {
                static_cast<void>(pause_input_from_snapshot(
                    physical_keys, escape_consumed,
                    pause_menu.screen == PauseScreen::capture_binding));
            }]=])
host_validation_expect_run_host_snapshot_mutation_rejected(
    "wrong real pause snapshot plus dead-scope decoy" "${_run_host}"
    "${_real_pause_input_call}" "${_wrong_pause_input_with_dead_decoy}")

host_validation_require_count("Host snapshot observations" "${_run_host}"
    "validation_runtime->observe_snapshot(" 3)
host_validation_require_count("Host inventory observation" "${_run_host}"
    "validation_runtime->observe_inventory(" 1)
host_validation_require_count("Host submitted observation" "${_run_host}"
    "validation_runtime->observe_submitted_actions(" 1)
host_validation_require_count("Host drain call sites" "${_run_host}"
    "drain_events(" 6)
host_validation_require_count("Host drain runtime arguments" "${_run_host}"
    "validation_runtime.get());" 6)
host_validation_require_order("combat-event fanout" "${_drain}"
    "validation_runtime->observe_combat_event(*event);"
    "renderer.consume_event(*event);"
    "feedback.consume(*event);"
    "audio.consume_event(*event);")
host_validation_require_count("drain runtime pointer" "${_drain}"
    "HostValidationRuntime* validation_runtime" 1)
foreach(_fanout_token IN ITEMS
        "validation_runtime->observe_combat_event(*event);"
        "renderer.consume_event(*event);"
        "feedback.consume(*event);"
        "audio.consume_event(*event);")
    host_validation_require_depth("direct combat-event fanout" "${_drain}"
        "${_fanout_token}" 2)
endforeach()

# Prove the critical direct Host statements are not borrowed from lambdas or
# nested scopes. run_raylib_host has a function body and one top-level try.
foreach(_direct_token IN ITEMS
        "HostValidationRuntime::create(config, loaded.status)"
        "validation_runtime->set_render_readiness(")
    host_validation_require_depth("direct Host statement" "${_run_host}"
        "${_direct_token}" 2)
endforeach()

# Task 7C removes the temporary access shim from the entire raylib platform
# surface, including inactive preprocessor branches. Comments and literals do
# not count as code, but no declaration, definition, or use may remain.
file(GLOB _raylib_shim_surfaces LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/src/platform/raylib/*.h"
    "${SOURCE_ROOT}/src/platform/raylib/*.hpp"
    "${SOURCE_ROOT}/src/platform/raylib/*.cpp")
foreach(_source IN LISTS _raylib_shim_surfaces)
    file(READ "${_source}" _source_text)
    host_validation_lexical_token_present(
        "${_source_text}" "HostValidationStateAccess" _shim_remains)
    if(_shim_remains)
        message(FATAL_ERROR
            "Host validation removed transition shim remains: ${_source}")
    endif()
endforeach()
set(_removed_shim_fixture [=[
#pragma once
#ifdef _WIN32
inline void forbidden_header_escape(HostValidationStateAccess& access);
#endif
]=])
host_validation_lexical_token_present(
    "${_removed_shim_fixture}" "HostValidationStateAccess"
    _removed_shim_detected)
if(NOT _removed_shim_detected)
    message(FATAL_ERROR
        "Host validation removed-shim guard accepted a header escape fixture")
endif()

# Domain modules cannot learn about the platform validation facade or any
# host_validation implementation namespace/header.
foreach(_module IN ITEMS core combat dungeon persistence)
    file(GLOB_RECURSE _module_sources LIST_DIRECTORIES FALSE
        "${SOURCE_ROOT}/src/${_module}/*.h"
        "${SOURCE_ROOT}/src/${_module}/*.hpp"
        "${SOURCE_ROOT}/src/${_module}/*.cpp")
    foreach(_source IN LISTS _module_sources)
        file(READ "${_source}" _source_text)
        if(NOT _source_text MATCHES
                "host_validation[.]hpp|host_validation_[A-Za-z0-9_]*|host_validation::|HostValidationRuntime")
            continue()
        endif()
        arpg_sanitize_cpp_source("${_source_text}" _source_lexical)
        if(_source_lexical MATCHES
                "host_validation[.]hpp|host_validation_[A-Za-z0-9_]*|host_validation::|HostValidationRuntime")
            message(FATAL_ERROR
                "Domain source depends on Host validation internals: ${_source}")
        endif()
    endforeach()
endforeach()

host_validation_assert_top_level_registration(
    "${_raylib_cmake_text}" "host_validation_runtime.cpp")
host_validation_assert_top_level_registration(
    "${_raylib_cmake_text}" "host_frame_gate.cpp")

# Pressure-test both views: conditional code cannot lend positive evidence,
# while forbidden dependencies in any conditional branch remain visible to
# negative scans after comments and ordinary literals are removed.
set(_lexical_fixture [=[
// host_validation_hidden_comment
const char* text = "host_validation_hidden_string";
const char* raw = R"tag(host_validation_hidden_raw)tag";
#if 0
host_validation_hidden_inactive;
#endif
#ifdef _WIN32
#include "host_validation.hpp"
host_validation_forbidden_conditional_dependency;
#endif
host_validation_visible;
]=])
host_validation_unconditional_cpp_surface("${_lexical_fixture}"
    _lexical_surface _lexical_all_branches)
foreach(_hidden IN ITEMS
        "host_validation_hidden_comment"
        "host_validation_hidden_string"
        "host_validation_hidden_raw"
        "host_validation_hidden_inactive"
        "host_validation.hpp"
        "host_validation_forbidden_conditional_dependency")
    string(FIND "${_lexical_surface}" "${_hidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation lexical guard accepted decoy: ${_hidden}")
    endif()
endforeach()
host_validation_require_count("lexical active self-check"
    "${_lexical_surface}" "host_validation_visible" 1)
foreach(_conditional_forbidden IN ITEMS
        "host_validation_hidden_inactive"
        "host_validation.hpp"
        "host_validation_forbidden_conditional_dependency")
    string(FIND "${_lexical_all_branches}"
        "${_conditional_forbidden}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation lexical negative scan lost conditional dependency: ${_conditional_forbidden}")
    endif()
endforeach()

message(STATUS "Host validation facade/runtime source boundaries verified")
