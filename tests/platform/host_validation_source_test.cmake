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

foreach(_target IN ITEMS
        "${_host_validation_header}"
        "${_host_validation_state}"
        "${_host_validation_runtime}")
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR
            "Host validation runtime target is missing: ${_target}")
    endif()
endforeach()

set(_host_source "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_raylib_cmake "${SOURCE_ROOT}/src/platform/raylib/CMakeLists.txt")
foreach(_target IN ITEMS "${_host_source}" "${_raylib_cmake}")
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR
            "Host validation integration target is missing: ${_target}")
    endif()
endforeach()

file(READ "${_host_validation_header}" _facade_text)
file(READ "${_host_validation_state}" _state_text)
file(READ "${_host_validation_runtime}" _runtime_text)
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

function(host_validation_exact_surface_valid SOURCE EXPECTED OUT_VALID)
    host_validation_normalize_cpp_surface("${SOURCE}" _actual)
    host_validation_normalize_cpp_surface("${EXPECTED}" _expected)
    if("${_actual}" STREQUAL "${_expected}")
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    endif()
endfunction()

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

function(host_validation_snapshot_bindings_valid_from_normalized
        NORMALIZED_SOURCE OUT_VALID)
    foreach(_pattern IN ITEMS
            "const[ ]+PhysicalKeySnapshot&[ ]+physical_keys[ ]*=[ ]*HostValidationStateAccess::death_input_snapshot\\([ ]*[*]validation_runtime[ ]*\\)"
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
    while(_copy_cursor LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_copy_cursor} -1 _tail)
        string(REGEX MATCH
            "\\][ \\t\\r\\n]*(\\([^{};]*\\))?[ \\t\\r\\n]*(mutable[ \\t\\r\\n]*)?(noexcept([ \\t\\r\\n]*\\([^{};]*\\))?[ \\t\\r\\n]*)?(->[^{;]*)?[ \\t\\r\\n]*\\{"
            _lambda_match "${_tail}")
        string(REGEX MATCH
            "(if|while)[ \\t\\r\\n]*(constexpr[ \\t\\r\\n]*)?\\([ \\t\\r\\n]*(false|0[uUlL]*|![ \\t\\r\\n]*true)[ \\t\\r\\n]*\\)[ \\t\\r\\n]*(do[ \\t\\r\\n]*)?([^{;]*\\{|[^{};]*;)"
            _dead_match "${_tail}")
        set(_scope_match "")
        set(_scope_relative -1)
        set(_scope_kind "")
        if(NOT _lambda_match STREQUAL "")
            string(FIND "${_tail}" "${_lambda_match}" _scope_relative)
            set(_scope_match "${_lambda_match}")
            set(_scope_kind lambda)
        endif()
        if(NOT _dead_match STREQUAL "")
            string(FIND "${_tail}" "${_dead_match}" _dead_relative)
            if(_scope_relative EQUAL -1 OR _dead_relative LESS _scope_relative)
                set(_scope_match "${_dead_match}")
                set(_scope_relative ${_dead_relative})
                set(_scope_kind dead-control)
            endif()
        endif()
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
    string(FIND "${_cmake_lower}" "${SOURCE}" _source)
    if(_library EQUAL -1 OR _source EQUAL -1 OR NOT _library LESS _source)
        message(FATAL_ERROR
            "Host validation runtime is not in the real top-level arpg_raylib source list")
    endif()
    string(SUBSTRING "${_cmake_lower}" 0 ${_source} _prefix)
    if(_prefix MATCHES "(^|\n)[ \t]*return[ \t]*\\(")
        message(FATAL_ERROR
            "Host validation runtime registration is unreachable after return")
    endif()
    arpg_cmake_code_line_and_paren_delta("${_prefix}" _paren_depth)
    if(NOT _paren_depth EQUAL 1)
        message(FATAL_ERROR
            "Host validation runtime registration is not a direct add_library argument")
    endif()
endfunction()

host_validation_unconditional_cpp_surface(
    "${_facade_text}" _facade _facade_lexical)
host_validation_unconditional_cpp_surface(
    "${_state_text}" _state _state_lexical)
host_validation_unconditional_cpp_surface(
    "${_runtime_text}" _runtime _runtime_lexical)
host_validation_unconditional_cpp_surface(
    "${_host_text}" _host _host_lexical)

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
    void observe_combat_event(const combat::CombatEvent&) noexcept;
    void observe_snapshot(const dungeon::DungeonSnapshot&) noexcept;
    void observe_inventory(const InventoryRenderer&,
        const dungeon::DungeonSnapshot&) noexcept;
    void observe_submitted_actions(const SubmittedFrameActions&) noexcept;

private:
    struct Impl;
    friend struct HostValidationStateAccess;
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
        "void observe_combat_event("
        "void observe_snapshot("
        "void observe_inventory("
        "void observe_submitted_actions("
        "struct Impl;"
        "friend struct HostValidationStateAccess;"
        "explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;"
        "std::unique_ptr<Impl> impl_;")
    host_validation_require_count("facade API" "${_facade_class}"
        "${_token}" 1)
endforeach()
host_validation_require_count("facade class" "${_facade}"
    "class HostValidationRuntime final" 1)
host_validation_require_count("facade friend-only transition seam" "${_facade}"
    "HostValidationStateAccess" 1)
host_validation_require_count("facade lexical friend-only transition seam"
    "${_facade_lexical}" "HostValidationStateAccess" 1)
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

# The internal state seam aggregates one of each existing Stage state and only
# exposes temporary reference accessors. State definitions stay in Stage heads.
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
        # Runtime references in the temporary accessors are qualified return
        # types, so only an unqualified declaration would be a second owner.
        string(REGEX MATCH
            "(^|[;{}])[ \t\r\n]*${_state_type}[ \t\r\n]+[A-Za-z_]"
            _runtime_direct_state_declaration "${_runtime_lexical}")
        if(NOT _runtime_direct_state_declaration STREQUAL "")
            message(FATAL_ERROR
                "Host validation runtime duplicates Stage state ownership: ${_state_type}")
        endif()
    endif()
endforeach()
host_validation_extract_sanitized_block("${_state}"
    "struct HostValidationStateAccess final" _state_access)
host_validation_extract_sanitized_block("${_state_lexical}"
    "struct HostValidationStateAccess final" _state_access_lexical)
set(_state_access_contract [=[
struct HostValidationStateAccess final {
    [[nodiscard]] static host_validation::Stage10ValidationState& stage10(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11ValidationState& stage11(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11BValidationState& stage11b(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11CHudValidationState& stage11c(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11DLootValidationState& stage11d(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage17SkillStonesValidationState& stage17(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static const PhysicalKeySnapshot& death_input_snapshot(
        const HostValidationRuntime&) noexcept;
}
]=])
host_validation_require_active_exact_definition(
    "temporary state-access exact seven-accessor API"
    "${_state}" "${_state_lexical}"
    "struct HostValidationStateAccess final" "${_state_access_contract}")
host_validation_expect_exact_mutation_rejected("extra temporary state accessor"
    "${_state_access_lexical}" "${_state_access_contract}"
    "const HostValidationRuntime&) noexcept;"
    "const HostValidationRuntime&) noexcept; static int forbidden_extra_accessor(HostValidationRuntime&) noexcept;")
foreach(_accessor IN ITEMS
        "stage10(" "stage11(" "stage11b(" "stage11c("
        "stage11d(" "stage17(" "death_input_snapshot(")
    host_validation_require_count("temporary state accessor"
        "${_state_access}" "${_accessor}" 1)
endforeach()
host_validation_require_count("state lexical transition declaration"
    "${_state_lexical}" "HostValidationStateAccess" 1)
foreach(_forbidden IN ITEMS "new (" "std::unique_ptr" "raylib.h")
    string(FIND "${_state_lexical}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Host validation state seam owns or allocates forbidden data: ${_forbidden}")
    endif()
endforeach()

# Impl/factory owns the six-state aggregate, one config pointer and one cached
# pre-Stage17 physical snapshot. Both task allocations are explicit nothrow.
host_validation_extract_sanitized_block("${_runtime}"
    "struct HostValidationRuntime::Impl final" _impl)
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "struct HostValidationRuntime::Impl final" _impl_lexical)
set(_impl_contract [=[
struct HostValidationRuntime::Impl final {
    explicit Impl(const RaylibHostConfig& host_config,
        settings::SettingsLoadStatus load_status) noexcept
        : config(&host_config) {
        states.stage11b.load_status = load_status;
    }

    const RaylibHostConfig* config{};
    HostValidationStates states{};
    PhysicalKeySnapshot death_input_snapshot{};
}
]=])
host_validation_require_active_exact_definition("Impl exact ownership"
    "${_runtime}" "${_runtime_lexical}"
    "struct HostValidationRuntime::Impl final" "${_impl_contract}")
host_validation_expect_exact_mutation_rejected("extra Impl owner"
    "${_impl_lexical}" "${_impl_contract}"
    "PhysicalKeySnapshot death_input_snapshot{};"
    "PhysicalKeySnapshot death_input_snapshot{}; int forbidden_extra_owner{};")
string(REPLACE "PhysicalKeySnapshot death_input_snapshot{};"
    "PhysicalKeySnapshot death_input_snapshot{}; int forbidden_extra_owner{};"
    _impl_active_extra "${_impl_contract}")
host_validation_expect_inactive_correct_active_mutation_rejected(
    "inactive correct Impl plus active extra owner"
    "struct HostValidationRuntime::Impl final" "${_impl_contract}"
    "${_impl_active_extra}")
host_validation_require_depth("Impl top-level owner" "${_runtime}"
    "struct HostValidationRuntime::Impl final" 1)
foreach(_field IN ITEMS
        "const RaylibHostConfig* config{};"
        "HostValidationStates states{};"
        "PhysicalKeySnapshot death_input_snapshot{};")
    host_validation_require_count("Impl ownership" "${_impl}" "${_field}" 1)
endforeach()
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

# The temporary transition shim is also exact at its seven implementation
# sites. This prevents a declaration-correct shim from returning detached,
# stale, or independently owned validation state.
set(_state_access_signature_stage10
    "host_validation::Stage10ValidationState& HostValidationStateAccess::stage10(")
set(_state_access_signature_stage11
    "host_validation::Stage11ValidationState& HostValidationStateAccess::stage11(")
set(_state_access_signature_stage11b
    "host_validation::Stage11BValidationState& HostValidationStateAccess::stage11b(")
set(_state_access_signature_stage11c
    "host_validation::Stage11CHudValidationState& HostValidationStateAccess::stage11c(")
set(_state_access_signature_stage11d
    "host_validation::Stage11DLootValidationState& HostValidationStateAccess::stage11d(")
set(_state_access_signature_stage17 [=[host_validation::Stage17SkillStonesValidationState&
HostValidationStateAccess::stage17(]=])
set(_state_access_signature_death
    "const PhysicalKeySnapshot& HostValidationStateAccess::death_input_snapshot(")

set(_state_access_contract_stage10 [=[
host_validation::Stage10ValidationState& HostValidationStateAccess::stage10(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage10;
}
]=])
set(_state_access_contract_stage11 [=[
host_validation::Stage11ValidationState& HostValidationStateAccess::stage11(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11;
}
]=])
set(_state_access_contract_stage11b [=[
host_validation::Stage11BValidationState& HostValidationStateAccess::stage11b(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11b;
}
]=])
set(_state_access_contract_stage11c [=[
host_validation::Stage11CHudValidationState& HostValidationStateAccess::stage11c(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11c;
}
]=])
set(_state_access_contract_stage11d [=[
host_validation::Stage11DLootValidationState& HostValidationStateAccess::stage11d(
    HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage11d;
}
]=])
set(_state_access_contract_stage17 [=[
host_validation::Stage17SkillStonesValidationState&
HostValidationStateAccess::stage17(HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->states.stage17;
}
]=])
set(_state_access_contract_death [=[
const PhysicalKeySnapshot& HostValidationStateAccess::death_input_snapshot(
    const HostValidationRuntime& runtime) noexcept {
    return runtime.impl_->death_input_snapshot;
}
]=])
foreach(_accessor IN ITEMS
        stage10 stage11 stage11b stage11c stage11d stage17 death)
    set(_signature_variable "_state_access_signature_${_accessor}")
    set(_contract_variable "_state_access_contract_${_accessor}")
    host_validation_require_active_exact_definition(
        "state-access ${_accessor} exact implementation"
        "${_runtime}" "${_runtime_lexical}"
        "${${_signature_variable}}" "${${_contract_variable}}")
endforeach()
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "${_state_access_signature_stage17}" _state_access_stage17_lexical)
host_validation_expect_exact_mutation_rejected(
    "stage17 accessor returns independent static state"
    "${_state_access_stage17_lexical}" "${_state_access_contract_stage17}"
    "return runtime.impl_->states.stage17;"
    "static host_validation::Stage17SkillStonesValidationState detached_state{}; return detached_state;")
host_validation_extract_sanitized_block("${_runtime_lexical}"
    "${_state_access_signature_death}" _state_access_death_lexical)
host_validation_expect_exact_mutation_rejected(
    "death accessor returns independent static snapshot"
    "${_state_access_death_lexical}" "${_state_access_contract_death}"
    "return runtime.impl_->death_input_snapshot;"
    "static PhysicalKeySnapshot detached_snapshot{}; return detached_snapshot;")

# Bind only the executable Host entry and drain helper. Comments, literals,
# inactive branches and cross-function decoys have already been removed.
host_validation_extract_sanitized_block("${_host}"
    "HostExitCode run_raylib_host(" _run_host)
host_validation_extract_sanitized_block("${_host}" "void drain_events(" _drain)
host_validation_require_depth("Host entry top-level definition" "${_host}"
    "HostExitCode run_raylib_host(" 1)
host_validation_require_depth("drain helper top-level definition" "${_host}"
    "void drain_events(" 2)
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
    "HostValidationStateAccess::death_input_snapshot("
    "gameplay_controls_physically_released("
    "stage17_physical_keys"
    "runtime.acknowledge_gameplay_rearmed();"
    "map_host_frame_input("
    "input_settings, stage17_physical_keys)")
host_validation_require_count("Host facade injection" "${_run_host}"
    "validation_runtime->inject_physical_edges(" 1)
host_validation_require_count("cached Stage11D accessor" "${_run_host}"
    "HostValidationStateAccess::death_input_snapshot(" 1)
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

# The temporary access shim is restricted to its declaration, implementation,
# and the Host transition surface.
file(GLOB _raylib_shim_surfaces LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/src/platform/raylib/*.h"
    "${SOURCE_ROOT}/src/platform/raylib/*.hpp"
    "${SOURCE_ROOT}/src/platform/raylib/*.cpp")
set(_shim_allowed_surfaces
    "${_host_validation_header}"
    "${_host_validation_state}"
    "${_host_source}"
    "${_host_validation_runtime}")
foreach(_source IN LISTS _raylib_shim_surfaces)
    list(FIND _shim_allowed_surfaces "${_source}" _allowed_index)
    if(NOT _allowed_index EQUAL -1)
        continue()
    endif()
    file(READ "${_source}" _source_text)
    string(FIND "${_source_text}" "HostValidationStateAccess" _raw_position)
    if(_raw_position EQUAL -1)
        continue()
    endif()
    host_validation_lexical_token_present(
        "${_source_text}" "HostValidationStateAccess" _shim_escaped)
    if(_shim_escaped)
        message(FATAL_ERROR
            "Host validation transition shim escaped its approved surfaces: ${_source}")
    endif()
endforeach()
set(_shim_header_escape_fixture [=[
#pragma once
#ifdef _WIN32
inline void forbidden_header_escape(HostValidationStateAccess& access);
#endif
]=])
host_validation_lexical_token_present(
    "${_shim_header_escape_fixture}" "HostValidationStateAccess"
    _shim_header_escape_detected)
if(NOT _shim_header_escape_detected)
    message(FATAL_ERROR
        "Host validation transition shim guard accepted a header escape fixture")
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
