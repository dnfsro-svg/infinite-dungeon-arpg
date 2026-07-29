if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake_source_registration_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

# The shared lexer performs translation phase 2 first, so LF/CRLF spliced
# directives are logical lines before this conservative unconditional policy.
function(stage17_mask_cpp_conditionals SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
    string(LENGTH "${_logical_source}" _source_length)
    set(_scan 0)
    set(_conditional_depth 0)
    set(_surface "")
    while(_scan LESS _source_length)
        string(SUBSTRING "${_logical_source}" ${_scan} -1 _tail)
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
                    "Host validation Stage17 preprocessor conditional is unbalanced")
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
        math(EXPR _scan "${_scan} + ${_line_length}")
    endwhile()
    if(NOT _conditional_depth EQUAL 0)
        message(FATAL_ERROR
            "Host validation Stage17 preprocessor conditional is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

# Preserve only the named string literals as identifier sentinels before the
# shared lexer removes strings/comments; then apply the existing conditional
# masker.  This makes literal evidence active-code evidence rather than raw
# text that can be borrowed from a comment or #if 0 branch.
function(stage17_active_report_literal_surface SOURCE OUT_SURFACE)
    set(_prepared "${SOURCE}")
    string(REPLACE "\"01-new-default-1280x720.png\"" "stage17_literal_01"
        _prepared "${_prepared}")
    string(REPLACE "\"02-draw-slash-windup-1280x720.png\"" "stage17_literal_02"
        _prepared "${_prepared}")
    string(REPLACE "\"03-draw-slash-hit-1280x720.png\"" "stage17_literal_03"
        _prepared "${_prepared}")
    string(REPLACE "\"04-storm-ground-array-1280x720.png\"" "stage17_literal_04"
        _prepared "${_prepared}")
    string(REPLACE "\"05-storm-aerial-array-1280x720.png\"" "stage17_literal_05"
        _prepared "${_prepared}")
    string(REPLACE "\"06-storm-finisher-1280x720.png\"" "stage17_literal_06"
        _prepared "${_prepared}")
    string(REPLACE "\"07-restarted-loadout-1280x720.png\"" "stage17_literal_07"
        _prepared "${_prepared}")
    string(REPLACE "\"clean_shutdown_exact_ready=\""
        "stage17_literal_clean_shutdown" _prepared "${_prepared}")
    string(REPLACE "\"result=\"" "stage17_literal_result"
        _prepared "${_prepared}")
    stage17_mask_cpp_conditionals("${_prepared}" _active)
    set(${OUT_SURFACE} "${_active}" PARENT_SCOPE)
endfunction()

function(stage17_assert_literal_once LABEL SURFACE SENTINEL)
    string(FIND "${SURFACE}" "${SENTINEL}" _first)
    if(_first EQUAL -1)
        message(FATAL_ERROR "Stage17 literal is missing from ${LABEL}: ${SENTINEL}")
    endif()
    math(EXPR _after "${_first} + 1")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${SENTINEL}" _second)
    if(NOT _second EQUAL -1)
        message(FATAL_ERROR "Stage17 literal is duplicated in ${LABEL}: ${SENTINEL}")
    endif()
endfunction()

function(stage17_assert_literal_absent LABEL SURFACE SENTINEL)
    string(FIND "${SURFACE}" "${SENTINEL}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "Stage17 literal has invalid active owner ${LABEL}: ${SENTINEL}")
    endif()
endfunction()

function(stage17_assert_spliced_conditional LABEL EOL)
    string(ASCII 92 _backslash)
    set(_fixture
        "#${_backslash}${EOL}if 0${EOL}${LABEL}_hidden;${EOL}#${_backslash}${EOL}endif${EOL}${LABEL}_visible;")
    stage17_mask_cpp_conditionals("${_fixture}" _active)
    string(FIND "${_active}" "${LABEL}_hidden" _hidden)
    string(FIND "${_active}" "${LABEL}_visible" _visible)
    if(NOT _hidden EQUAL -1 OR _visible EQUAL -1)
        message(FATAL_ERROR
            "Stage17 conditional masker failed ${LABEL} phase-2 splice self-check")
    endif()
endfunction()

string(ASCII 10 _stage17_lf)
string(ASCII 13 _stage17_cr)
stage17_assert_spliced_conditional(stage17_spliced_lf "${_stage17_lf}")
stage17_assert_spliced_conditional(stage17_spliced_crlf
    "${_stage17_cr}${_stage17_lf}")

set(_stage17_named_mutation_consumed FALSE)

set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
if(NOT EXISTS "${_host}")
    message(FATAL_ERROR "Host validation sequence target is missing: ${_host}")
endif()

file(READ "${_host}" _host_text)
if(DEFINED STAGE17_SEQUENCE_MUTATION)
    set(_host_before_mutation "${_host_text}")
    if(STAGE17_SEQUENCE_MUTATION STREQUAL
            "loop_top_snapshot_unbraced_dead_branch")
        string(REPLACE
            "            }\n            validation_runtime->observe_snapshot(current);"
            "            }\n            if (false)\n                validation_runtime->observe_snapshot(current);"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "draw_observer_before_renderer_draw")
        set(_draw_observer [=[            observe_stage17_draw_runtime(config, *stage17_validation_state,
                presented_snapshot, renderer.active_skill_draw_status());
]=])
        string(REPLACE "${_draw_observer}" "" _host_text "${_host_text}")
        string(REPLACE
            "            const GroundLootView ground_loot_view = [&]() noexcept {"
            "${_draw_observer}            const GroundLootView ground_loot_view = [&]() noexcept {"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "drain_missing_stage17_state")
        string(REPLACE "validation_runtime.get());" "nullptr);"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "combat_observer_unbraced_dead_branch")
        string(REPLACE
            "        validation_runtime->observe_combat_event(*event);"
            "        if (false)\n            validation_runtime->observe_combat_event(*event);"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "inventory_observer_unbraced_dead_branch")
        string(REPLACE
            "            validation_runtime->observe_inventory(inventory, current);"
            "            if (false)\n                validation_runtime->observe_inventory(inventory, current);"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "submitted_observer_unbraced_dead_branch")
        string(REPLACE
            "                validation_runtime->observe_submitted_actions(submitted_actions);"
            "                if (false)\n                    validation_runtime->observe_submitted_actions(submitted_actions);"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "fixed_snapshot_unbraced_dead_branch")
        string(REPLACE
            "                session->snapshot(current);\n                validation_runtime->observe_snapshot(current);"
            "                session->snapshot(current);\n                if (false)\n                    validation_runtime->observe_snapshot(current);"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "draw_observer_unbraced_dead_branch")
        string(REPLACE
            "            observe_stage17_draw_runtime(config, *stage17_validation_state,"
            "            if (false)\n                observe_stage17_draw_runtime(config, *stage17_validation_state,"
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "inactive_retained_report_if_false")
        string(REPLACE
            "[[nodiscard]] bool stage17_draw_runtime_valid("
            "#if false\nbool stage17_draw_runtime_valid() noexcept { return true; }\n#endif\n[[nodiscard]] bool stage17_draw_runtime_valid_removed("
            _host_text "${_host_text}")
        set(_stage17_named_mutation_consumed TRUE)
    endif()
    if(_stage17_named_mutation_consumed
            AND _host_text STREQUAL _host_before_mutation)
        message(FATAL_ERROR
            "Stage17 named host mutation anchor is missing: ${STAGE17_SEQUENCE_MUTATION}")
    endif()
endif()
stage17_mask_cpp_conditionals("${_host_text}" _host_active_text)
evidence_find_cpp_code_token("${_host_active_text}"
    "HostExitCode run_raylib_host("
    _runtime_candidate)
if(_runtime_candidate EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard missing run_raylib_host candidate")
endif()
string(SUBSTRING "${_host_active_text}" ${_runtime_candidate} -1
    _host_runtime_candidate)
evidence_extract_cpp_function_block("${_host_runtime_candidate}"
    "HostExitCode run_raylib_host(" _host_runtime)
set(_sanitized "${_host_runtime}")

set(_input_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.hpp")
set(_input_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.cpp")
set(_navigation_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_navigation.hpp")
set(_navigation_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_navigation.cpp")
set(_stage10_11_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage10_11.hpp")
set(_stage10_11_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage10_11.cpp")
set(_stage11b_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.hpp")
if(DEFINED STAGE11B_HEADER_OVERRIDE)
    set(_stage11b_header "${STAGE11B_HEADER_OVERRIDE}")
endif()
set(_stage11b_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.cpp")
set(_stage11c_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.hpp")
if(DEFINED STAGE11C_HEADER_OVERRIDE)
    set(_stage11c_header "${STAGE11C_HEADER_OVERRIDE}")
endif()
set(_stage11c_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.cpp")
if(DEFINED STAGE11C_SOURCE_OVERRIDE)
    set(_stage11c_source "${STAGE11C_SOURCE_OVERRIDE}")
endif()
set(_stage11d_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp")
if(DEFINED STAGE11D_HEADER_OVERRIDE)
    set(_stage11d_header "${STAGE11D_HEADER_OVERRIDE}")
endif()
set(_stage11d_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_runtime.cpp")
if(DEFINED STAGE11D_RUNTIME_OVERRIDE)
    set(_stage11d_runtime "${STAGE11D_RUNTIME_OVERRIDE}")
endif()
set(_stage11d_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
if(DEFINED STAGE11D_REPORT_OVERRIDE)
    set(_stage11d_report "${STAGE11D_REPORT_OVERRIDE}")
endif()
set(_stage17_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage17.hpp")
if(DEFINED STAGE17_HEADER_OVERRIDE)
    set(_stage17_header "${STAGE17_HEADER_OVERRIDE}")
endif()
set(_stage17_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage17_runtime.cpp")
if(DEFINED STAGE17_RUNTIME_OVERRIDE)
    set(_stage17_runtime "${STAGE17_RUNTIME_OVERRIDE}")
endif()
set(_stage17_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage17_report.cpp")
if(NOT EXISTS "${_stage17_report}")
    message(FATAL_ERROR
        "Stage17 report source is required: ${_stage17_report}")
endif()
set(_raylib_cmake "${SOURCE_ROOT}/src/platform/raylib/CMakeLists.txt")
if(DEFINED CMAKE_OVERRIDE)
    set(_raylib_cmake "${CMAKE_OVERRIDE}")
endif()
foreach(_required IN ITEMS
        "${_input_header}" "${_input_source}"
        "${_navigation_header}" "${_navigation_source}"
        "${_stage10_11_header}" "${_stage10_11_source}"
        "${_stage11b_header}" "${_stage11b_source}"
        "${_stage11c_header}" "${_stage11c_source}"
        "${_stage11d_header}" "${_stage11d_runtime}"
        "${_stage11d_report}" "${_stage17_header}"
        "${_stage17_runtime}" "${_stage17_report}" "${_raylib_cmake}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Host validation boundary target is missing: ${_required}")
    endif()
endforeach()
file(READ "${_input_header}" _input_header_text)
file(READ "${_input_source}" _input_source_text)
file(READ "${_navigation_header}" _navigation_header_text)
file(READ "${_navigation_source}" _navigation_source_text)
file(READ "${_stage10_11_header}" _stage10_11_header_text)
file(READ "${_stage10_11_source}" _stage10_11_source_text)
file(READ "${_stage11b_header}" _stage11b_header_text)
file(READ "${_stage11b_source}" _stage11b_source_text)
file(READ "${_stage11c_header}" _stage11c_header_text)
file(READ "${_stage11c_source}" _stage11c_source_text)
file(READ "${_stage11d_header}" _stage11d_header_text)
file(READ "${_stage11d_runtime}" _stage11d_runtime_text)
file(READ "${_stage11d_report}" _stage11d_report_text)
file(READ "${_stage17_header}" _stage17_header_text)
file(READ "${_stage17_runtime}" _stage17_runtime_text)
file(READ "${_stage17_report}" _stage17_report_text)
file(READ "${_raylib_cmake}" _raylib_cmake_text)

if(DEFINED STAGE17_SEQUENCE_MUTATION)
    if(STAGE17_SEQUENCE_MUTATION STREQUAL "inactive_public_runtime_if_0"
            OR STAGE17_SEQUENCE_MUTATION STREQUAL
                "inactive_public_runtime_spliced_if_lf"
            OR STAGE17_SEQUENCE_MUTATION STREQUAL
                "inactive_public_runtime_spliced_if_crlf")
        set(_runtime_before_mutation "${_stage17_runtime_text}")
        set(_inactive_open "#if 0\n")
        set(_inactive_close "#endif")
        if(STAGE17_SEQUENCE_MUTATION MATCHES "spliced_if_(lf|crlf)$")
            string(ASCII 92 _backslash)
            set(_eol "${_stage17_lf}")
            if(STAGE17_SEQUENCE_MUTATION MATCHES "crlf$")
                set(_eol "${_stage17_cr}${_stage17_lf}")
            endif()
            set(_inactive_open "#${_backslash}${_eol}if 0${_eol}")
            set(_inactive_close "#${_backslash}${_eol}endif")
        endif()
        string(REPLACE "void observe_stage17_draw_runtime("
            "void observe_stage17_draw_runtime_removed("
            _stage17_runtime_text "${_stage17_runtime_text}")
        string(REPLACE
            "namespace arpg::platform::host_validation {"
            "namespace arpg::platform::host_validation {\n${_inactive_open}void observe_stage17_draw_runtime() noexcept {}\n${_inactive_close}"
            _stage17_runtime_text "${_stage17_runtime_text}")
        if(_stage17_runtime_text STREQUAL _runtime_before_mutation)
            message(FATAL_ERROR
                "Stage17 named runtime mutation anchor is missing: ${STAGE17_SEQUENCE_MUTATION}")
        endif()
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "cmake_top_level_return_before_registration")
        string(PREPEND _raylib_cmake_text "return()\n")
        set(_stage17_named_mutation_consumed TRUE)
    elseif(STAGE17_SEQUENCE_MUTATION STREQUAL
            "cmake_macro_return_before_registration")
        string(PREPEND _raylib_cmake_text
            "macro(stage17_stop_registration)\n    return()\nendmacro()\nstage17_stop_registration()\n")
        set(_stage17_named_mutation_consumed TRUE)
    endif()
    if(NOT _stage17_named_mutation_consumed)
        message(FATAL_ERROR
            "Unknown Stage17 sequence mutation: ${STAGE17_SEQUENCE_MUTATION}")
    endif()
endif()

stage17_mask_cpp_conditionals("${_stage17_header_text}"
    _stage17_header_active_text)
stage17_mask_cpp_conditionals("${_stage17_runtime_text}"
    _stage17_runtime_active_text)
stage17_mask_cpp_conditionals("${_stage17_report_text}"
    _stage17_report_active_text)

function(stage11d_find_host_code_token TOKEN OUT_POSITION)
    string(FIND "${_host_text}" "${TOKEN}" _raw_position)
    if(_raw_position EQUAL -1)
        set(${OUT_POSITION} -1 PARENT_SCOPE)
        return()
    endif()
    evidence_find_cpp_code_token("${_host_text}" "${TOKEN}" _code_position)
    set(${OUT_POSITION} ${_code_position} PARENT_SCOPE)
endfunction()

function(assert_unique_cpp_definition LABEL SURFACE TOKEN)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation ${LABEL} definition is missing: ${TOKEN}")
    endif()
    string(LENGTH "${TOKEN}" _token_length)
    math(EXPR _after "${_position} + ${_token_length}")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR
            "Host validation ${LABEL} definition is duplicated: ${TOKEN}")
    endif()
    string(SUBSTRING "${SURFACE}" ${_position} -1 _tail)
    string(FIND "${_tail}" "{" _open)
    string(FIND "${_tail}" ";" _semicolon)
    if(_open EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _open))
        message(FATAL_ERROR
            "Host validation ${LABEL} is only a forward declaration: ${TOKEN}")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${SURFACE}" "${TOKEN}"
        _function_begin _function_open _function_end)
endfunction()

function(cpp_token_brace_depth SURFACE POSITION OUTPUT)
    if(POSITION EQUAL 0)
        set(${OUTPUT} 0 PARENT_SCOPE)
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
    set(${OUTPUT} ${_depth} PARENT_SCOPE)
endfunction()

function(assert_cpp_definition_in_scope LABEL SURFACE TOKEN EXPECTED_DEPTH
        SCOPE_OPEN SCOPE_END)
    assert_unique_cpp_definition("${LABEL}" "${SURFACE}" "${TOKEN}")
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position LESS_EQUAL SCOPE_OPEN OR _position GREATER_EQUAL SCOPE_END)
        message(FATAL_ERROR
            "Host validation ${LABEL} is outside its required namespace: ${TOKEN}")
    endif()
    cpp_token_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Host validation ${LABEL} is outside its direct namespace scope: ${TOKEN}")
    endif()
endfunction()

assert_unique_cpp_definition("host runtime" "${_host_runtime}"
    "HostExitCode run_raylib_host(")

foreach(_header_text IN ITEMS
        "${_input_header_text}" "${_navigation_header_text}"
        "${_stage10_11_header_text}" "${_stage11b_header_text}"
        "${_stage11c_header_text}" "${_stage11d_header_text}")
    if(_header_text MATCHES "raylib[.]h|renderer|persistence|test")
        message(FATAL_ERROR "Host validation boundary header has a forbidden dependency")
    endif()
endforeach()

evidence_sanitize_cpp_for_scan("${_stage17_header_active_text}"
    _stage17_header_code)
evidence_sanitize_cpp_for_scan("${_stage17_runtime_active_text}"
    _stage17_runtime_code)
evidence_sanitize_cpp_for_scan("${_stage17_report_active_text}"
    _stage17_report_code)
stage17_active_report_literal_surface("${_stage17_report_text}"
    _stage17_report_literal_surface)
if(NOT DEFINED HOST_OVERRIDE)
    stage17_active_report_literal_surface("${_host_text}"
        _stage17_host_literal_surface)
    stage17_active_report_literal_surface("${_stage17_runtime_text}"
        _stage17_runtime_literal_surface)
    stage17_active_report_literal_surface("${_stage17_header_text}"
        _stage17_header_literal_surface)
endif()
if(_stage17_header_code MATCHES
        "raylib[.]h|Rectangle|Vector2|GetScreenWidth|GetScreenHeight")
    message(FATAL_ERROR
        "Host validation Stage17 header leaks raylib implementation types")
endif()

set(_stage17_namespace_token "namespace arpg::platform::host_validation")
string(FIND "${_stage17_runtime_code}" "${_stage17_namespace_token}"
    _stage17_namespace_begin)
if(_stage17_namespace_begin EQUAL -1)
    message(FATAL_ERROR
        "Host validation Stage17 runtime namespace is missing")
endif()
string(LENGTH "${_stage17_namespace_token}" _stage17_namespace_token_length)
math(EXPR _stage17_namespace_after
    "${_stage17_namespace_begin} + ${_stage17_namespace_token_length}")
string(SUBSTRING "${_stage17_runtime_code}" ${_stage17_namespace_after} -1
    _stage17_namespace_remainder)
string(FIND "${_stage17_namespace_remainder}" "${_stage17_namespace_token}"
    _stage17_namespace_duplicate)
if(NOT _stage17_namespace_duplicate EQUAL -1)
    message(FATAL_ERROR
        "Host validation Stage17 runtime namespace is duplicated")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_stage17_runtime_code}"
    "${_stage17_namespace_token}" _stage17_namespace_begin
    _stage17_namespace_open _stage17_namespace_end)

set(_stage17_private_namespace_token "namespace {")
string(FIND "${_stage17_runtime_code}" "${_stage17_private_namespace_token}"
    _stage17_private_namespace_begin)
if(_stage17_private_namespace_begin EQUAL -1
        OR _stage17_private_namespace_begin LESS_EQUAL _stage17_namespace_open
        OR _stage17_private_namespace_begin GREATER_EQUAL _stage17_namespace_end)
    message(FATAL_ERROR
        "Host validation Stage17 runtime anonymous namespace is missing")
endif()
string(LENGTH "${_stage17_private_namespace_token}"
    _stage17_private_namespace_token_length)
math(EXPR _stage17_private_namespace_after
    "${_stage17_private_namespace_begin} + ${_stage17_private_namespace_token_length}")
string(SUBSTRING "${_stage17_runtime_code}"
    ${_stage17_private_namespace_after} -1 _stage17_private_remainder)
string(FIND "${_stage17_private_remainder}"
    "${_stage17_private_namespace_token}" _stage17_private_duplicate)
if(NOT _stage17_private_duplicate EQUAL -1)
    message(FATAL_ERROR
        "Host validation Stage17 runtime anonymous namespace is duplicated")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_stage17_runtime_code}"
    "${_stage17_private_namespace_token}" _stage17_private_namespace_begin
    _stage17_private_namespace_open _stage17_private_namespace_end)

foreach(_stage17_state_token IN ITEMS
        "enum class Stage17ValidationStep"
        "enum class Stage17Capture"
        "struct Stage17SkillDrawRuntimeEvidence final"
        "struct Stage17SkillStonesValidationState final")
    string(FIND "${_stage17_header_code}" "${_stage17_state_token}"
        _stage17_header_state)
    if(_stage17_header_state EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage17 shared state is missing: ${_stage17_state_token}")
    endif()
    string(FIND "${_host_active_text}" "${_stage17_state_token}"
        _stage17_host_state)
    if(NOT _stage17_host_state EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage17 shared state remains in raylib_host.cpp: ${_stage17_state_token}")
    endif()
endforeach()

foreach(_stage17_runtime_definition IN ITEMS
        "void observe_stage17_draw_runtime("
        "void observe_stage17_combat_event("
        "PhysicalKeySnapshot inject_stage17_physical_edges("
        "void observe_stage17_submitted_actions("
        "void observe_stage17_snapshot("
        "void observe_stage17_inventory(")
    assert_cpp_definition_in_scope("Stage17 public runtime"
        "${_stage17_runtime_code}" "${_stage17_runtime_definition}" 1
        ${_stage17_namespace_open} ${_stage17_namespace_end})
    string(FIND "${_host_active_text}" "${_stage17_runtime_definition}"
        _stage17_host_definition)
    if(NOT _stage17_host_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage17 runtime definition remains in raylib_host.cpp: ${_stage17_runtime_definition}")
    endif()
    string(FIND "${_stage17_header_code}" "${_stage17_runtime_definition}"
        _stage17_declaration)
    if(_stage17_declaration EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage17 runtime declaration is missing: ${_stage17_runtime_definition}")
    endif()
endforeach()

foreach(_stage17_private_definition IN ITEMS
        "void stage17_press_action("
        "void stage17_apply_movement("
        "struct Stage17IsolatedStormTarget final"
        "Stage17IsolatedStormTarget stage17_isolated_storm_target("
        "bool stage17_prepare_isolated_storm("
        "Vector2 stage17_center("
        "void stage17_click("
        "bool stage17_same_point("
        "bool stage17_all_cooldowns_zero("
        "bool stage17_final_loadout(")
    assert_cpp_definition_in_scope("Stage17 private runtime helper"
        "${_stage17_runtime_code}" "${_stage17_private_definition}" 2
        ${_stage17_private_namespace_open} ${_stage17_private_namespace_end})
endforeach()

set(_stage17_report_namespace_token "namespace arpg::platform::host_validation")
evidence_find_cpp_function_bounds_in_sanitized("${_stage17_report_code}"
    "${_stage17_report_namespace_token}" _stage17_report_namespace_begin
    _stage17_report_namespace_open _stage17_report_namespace_end)
if(_stage17_report_namespace_begin EQUAL -1)
    message(FATAL_ERROR "Host validation Stage17 report namespace is missing")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_stage17_report_code}"
    "namespace {" _stage17_report_private_begin _stage17_report_private_open
    _stage17_report_private_end)
if(_stage17_report_private_begin EQUAL -1)
    message(FATAL_ERROR "Host validation Stage17 report anonymous namespace is missing")
endif()
math(EXPR _stage17_report_after_first_private
    "${_stage17_report_private_end} + 1")
string(SUBSTRING "${_stage17_report_code}" ${_stage17_report_after_first_private}
    -1 _stage17_report_after_first_private_text)
string(FIND "${_stage17_report_after_first_private_text}" "namespace {"
    _stage17_report_second_private_relative)
if(_stage17_report_second_private_relative EQUAL -1)
    message(FATAL_ERROR "Host validation Stage17 report second anonymous namespace is missing")
endif()
math(EXPR _stage17_report_second_private_begin
    "${_stage17_report_after_first_private} + ${_stage17_report_second_private_relative}")
string(SUBSTRING "${_stage17_report_code}"
    ${_stage17_report_second_private_begin} -1 _stage17_report_second_private_tail)
evidence_find_cpp_function_bounds_in_sanitized(
    "${_stage17_report_second_private_tail}" "namespace {"
    _stage17_report_second_private_tail_begin
    _stage17_report_second_private_tail_open _stage17_report_second_private_tail_end)
math(EXPR _stage17_report_second_private_open
    "${_stage17_report_second_private_begin} + ${_stage17_report_second_private_tail_open}")
math(EXPR _stage17_report_second_private_end
    "${_stage17_report_second_private_begin} + ${_stage17_report_second_private_tail_end}")
foreach(_anonymous_range IN ITEMS
        "${_stage17_report_private_begin};${_stage17_report_private_open};${_stage17_report_private_end}"
        "${_stage17_report_second_private_begin};${_stage17_report_second_private_open};${_stage17_report_second_private_end}")
    list(GET _anonymous_range 0 _anonymous_begin)
    list(GET _anonymous_range 1 _anonymous_open)
    list(GET _anonymous_range 2 _anonymous_end)
    if(_anonymous_begin LESS_EQUAL _stage17_report_namespace_open
            OR _anonymous_end GREATER_EQUAL _stage17_report_namespace_end)
        message(FATAL_ERROR
            "Stage17 report anonymous namespace is outside host_validation")
    endif()
    cpp_token_brace_depth("${_stage17_report_code}" ${_anonymous_begin}
        _anonymous_depth)
    if(NOT _anonymous_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage17 report anonymous namespace is not direct host_validation scope")
    endif()
endforeach()

string(FIND "${_stage17_header_code}" "namespace arpg::platform {"
    _stage17_header_platform_begin)
if(_stage17_header_platform_begin EQUAL -1)
    message(FATAL_ERROR "Stage17 report header platform namespace is missing")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_stage17_header_code}"
    "namespace arpg::platform {" _stage17_header_platform_begin
    _stage17_header_platform_open _stage17_header_platform_end)
string(FIND "${_stage17_header_code}" "namespace host_validation {"
    _stage17_header_namespace_begin)
if(_stage17_header_namespace_begin EQUAL -1)
    message(FATAL_ERROR "Stage17 report header namespace is missing")
endif()
evidence_find_cpp_function_bounds_in_sanitized("${_stage17_header_code}"
    "namespace host_validation {" _stage17_header_namespace_begin
    _stage17_header_namespace_open _stage17_header_namespace_end)
if(_stage17_header_namespace_begin LESS_EQUAL _stage17_header_platform_open
        OR _stage17_header_namespace_end GREATER_EQUAL _stage17_header_platform_end)
    message(FATAL_ERROR
        "Stage17 report header host_validation namespace is outside arpg::platform")
endif()
cpp_token_brace_depth("${_stage17_header_code}"
    ${_stage17_header_namespace_begin} _stage17_header_namespace_depth)
if(NOT _stage17_header_namespace_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage17 report header host_validation namespace is not direct arpg::platform scope")
endif()

function(assert_unique_stage17_report_header_declaration TOKEN)
    string(FIND "${_stage17_header_code}" "${TOKEN}" _declaration)
    if(_declaration EQUAL -1)
        message(FATAL_ERROR "Stage17 report declaration missing: ${TOKEN}")
    endif()
    string(LENGTH "${TOKEN}" _token_length)
    math(EXPR _after "${_declaration} + ${_token_length}")
    string(SUBSTRING "${_stage17_header_code}" ${_after} -1 _tail)
    string(FIND "${_tail}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "Stage17 report declaration duplicated: ${TOKEN}")
    endif()
    string(FIND "${_tail}" ";" _semicolon)
    string(FIND "${_tail}" "{" _open)
    if(_semicolon EQUAL -1 OR (NOT _open EQUAL -1 AND _open LESS _semicolon))
        message(FATAL_ERROR "Stage17 report declaration is not header-only: ${TOKEN}")
    endif()
    if(_declaration LESS_EQUAL _stage17_header_namespace_open
            OR _declaration GREATER_EQUAL _stage17_header_namespace_end)
        message(FATAL_ERROR "Stage17 report declaration is outside host_validation: ${TOKEN}")
    endif()
    cpp_token_brace_depth("${_stage17_header_code}" ${_declaration}
        _declaration_depth)
    if(NOT _declaration_depth EQUAL 2)
        message(FATAL_ERROR "Stage17 report declaration is not direct host_validation scope: ${TOKEN}")
    endif()
endfunction()

foreach(_stage17_report_public_definition IN ITEMS
        "std::optional<std::string> stage17_capture_path("
        "void mark_stage17_capture_complete("
        "bool stage17_validation_complete("
        "void write_stage17_validation_summary(")
    assert_cpp_definition_in_scope("Stage17 report public definition"
        "${_stage17_report_code}" "${_stage17_report_public_definition}" 1
        ${_stage17_report_namespace_open} ${_stage17_report_namespace_end})
    assert_unique_stage17_report_header_declaration("${_stage17_report_public_definition}")
    foreach(_wrong_owner IN ITEMS "${_host_active_text}" "${_stage17_runtime_code}")
        string(FIND "${_wrong_owner}" "${_stage17_report_public_definition}" _wrong_owner_definition)
        if(NOT _wrong_owner_definition EQUAL -1)
            message(FATAL_ERROR "Stage17 report public definition outside report: ${_stage17_report_public_definition}")
        endif()
    endforeach()
endforeach()

foreach(_stage17_report_private_definition IN ITEMS
        "bool stage17_draw_runtime_valid("
        "const char* stage17_capture_name("
        "const char* stage17_skill_name("
        "void write_stage17_loadout("
        "std::size_t stage17_support_none_count(")
    assert_unique_cpp_definition("Stage17 report private helper"
        "${_stage17_report_code}" "${_stage17_report_private_definition}")
    string(FIND "${_stage17_report_code}"
        "${_stage17_report_private_definition}" _stage17_report_private_position)
    cpp_token_brace_depth("${_stage17_report_code}"
        ${_stage17_report_private_position} _stage17_report_private_depth)
    if(_stage17_report_private_definition MATCHES
            "stage17_draw_runtime_valid|stage17_capture_name")
        set(_private_open ${_stage17_report_private_open})
        set(_private_end ${_stage17_report_private_end})
    else()
        set(_private_open ${_stage17_report_second_private_open})
        set(_private_end ${_stage17_report_second_private_end})
    endif()
    if(NOT _stage17_report_private_depth EQUAL 2
            OR _stage17_report_private_position LESS_EQUAL _private_open
            OR _stage17_report_private_position GREATER_EQUAL _private_end)
        message(FATAL_ERROR "Stage17 report private helper is outside an anonymous namespace: ${_stage17_report_private_definition}")
    endif()
    foreach(_wrong_owner IN ITEMS "${_host_active_text}" "${_stage17_runtime_code}" "${_stage17_header_code}")
        string(FIND "${_wrong_owner}" "${_stage17_report_private_definition}" _wrong_owner_definition)
        if(NOT _wrong_owner_definition EQUAL -1)
            message(FATAL_ERROR "Stage17 report private helper invalid owner: ${_stage17_report_private_definition}")
        endif()
    endforeach()
endforeach()

evidence_extract_cpp_function_block("${_stage17_report_literal_surface}"
    "const char* stage17_capture_name(" _stage17_capture_name_literal_block)
evidence_extract_cpp_function_block("${_stage17_report_literal_surface}"
    "void write_stage17_validation_summary(" _stage17_summary_literal_block)
foreach(_capture_literal IN ITEMS stage17_literal_01 stage17_literal_02
        stage17_literal_03 stage17_literal_04 stage17_literal_05
        stage17_literal_06 stage17_literal_07)
    stage17_assert_literal_once("stage17_capture_name"
        "${_stage17_capture_name_literal_block}" "${_capture_literal}")
endforeach()
foreach(_summary_literal IN ITEMS stage17_literal_result
        stage17_literal_clean_shutdown)
    stage17_assert_literal_once("write_stage17_validation_summary"
        "${_stage17_summary_literal_block}" "${_summary_literal}")
endforeach()
if(NOT DEFINED HOST_OVERRIDE)
    foreach(_literal_owner_surface IN ITEMS
            "${_stage17_host_literal_surface}"
            "${_stage17_runtime_literal_surface}"
            "${_stage17_header_literal_surface}")
        foreach(_forbidden_literal IN ITEMS stage17_literal_01 stage17_literal_02
                stage17_literal_03 stage17_literal_04 stage17_literal_05
                stage17_literal_06 stage17_literal_07 stage17_literal_result
                stage17_literal_clean_shutdown)
            stage17_assert_literal_absent("non-report source"
                "${_literal_owner_surface}" "${_forbidden_literal}")
        endforeach()
    endforeach()
endif()
foreach(_stage10_11_definition_token IN ITEMS
        "combat::MovementInput stage10_validation_input("
        "combat::MovementInput stage11_validation_input("
        "bool stage10_validation_reached("
        "bool stage11_validation_reached(")
    string(FIND "${_stage10_11_source_text}" "${_stage10_11_definition_token}"
        _stage10_11_definition)
    if(_stage10_11_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 definition is missing: ${_stage10_11_definition_token}")
    endif()
    string(FIND "${_host_text}" "${_stage10_11_definition_token}"
        _host_stage10_11_definition)
    if(NOT _host_stage10_11_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 definition remains in raylib_host.cpp: ${_stage10_11_definition_token}")
    endif()
endforeach()

foreach(_stage10_11_state_token IN ITEMS
        "struct Stage10ValidationState final"
        "struct Stage11ValidationState final")
    string(FIND "${_stage10_11_header_text}" "${_stage10_11_state_token}"
        _stage10_11_state_definition)
    if(_stage10_11_state_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 state definition is missing: ${_stage10_11_state_token}")
    endif()
    string(FIND "${_host_text}" "${_stage10_11_state_token}"
        _host_stage10_11_state_definition)
    if(NOT _host_stage10_11_state_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 state definition remains in raylib_host.cpp: ${_stage10_11_state_token}")
    endif()
endforeach()

foreach(_stage11b_definition_token IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary(")
    string(FIND "${_stage11b_source_text}" "${_stage11b_definition_token}"
        _stage11b_definition)
    if(_stage11b_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11B definition is missing: ${_stage11b_definition_token}")
    endif()
    string(FIND "${_host_text}" "${_stage11b_definition_token}"
        _host_stage11b_definition)
    if(NOT _host_stage11b_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11B definition remains in raylib_host.cpp: ${_stage11b_definition_token}")
    endif()
endforeach()
if(NOT _stage11b_header_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Host validation Stage11B state definition is missing")
endif()

foreach(_stage11c_definition_token IN ITEMS
        "PhysicalKeySnapshot inject_stage11c_physical_edges("
        "std::uint64_t stage11c_production_snapshot_hash("
        "bool stage11c_hud_validation_reached("
        "void write_stage11c_hud_validation_summary(")
    evidence_extract_cpp_function_block("${_stage11c_source_text}"
        "${_stage11c_definition_token}" _stage11c_function)
    string(FIND "${_stage11c_function}" "{" _stage11c_definition)
    string(FIND "${_stage11c_function}" ";" _stage11c_forward_declaration)
    if(_stage11c_definition EQUAL -1
            OR (NOT _stage11c_forward_declaration EQUAL -1
                AND _stage11c_forward_declaration LESS _stage11c_definition))
        message(FATAL_ERROR
            "Host validation Stage11C definition is missing: ${_stage11c_definition_token}")
    endif()
    string(FIND "${_host_text}" "${_stage11c_definition_token}"
        _host_stage11c_definition)
    if(NOT _host_stage11c_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11C definition remains in raylib_host.cpp: ${_stage11c_definition_token}")
    endif()
endforeach()
evidence_sanitize_cpp_for_scan("${_stage11c_header_text}" _stage11c_header_code)
if(NOT _stage11c_header_code MATCHES
        "struct[ \t\r\n]+Stage11CHudValidationState[ \t\r\n]+final[ \t\r\n]*[{]")
    message(FATAL_ERROR "Host validation Stage11C state definition is missing")
endif()
if(_host_text MATCHES "struct Stage11CHudValidationState final")
    message(FATAL_ERROR
        "Host validation Stage11C state definition remains in raylib_host.cpp")
endif()
if(_host_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR
        "Host validation Stage11B state definition remains in raylib_host.cpp")
endif()

foreach(_stage11d_definition_token IN ITEMS
        "bool stage11d_has_three_ordinary_rarities("
        "const dungeon::GroundItemSnapshot* stage11d_nearest_ground("
        "const combat::MonsterSnapshot* stage11d_priority_monster("
        "bool stage11d_attack_lane("
        "combat::MovementInput stage11d_safe_movement_toward("
        "PhysicalKeySnapshot inject_stage11d_physical_edges("
        "bool stage11d_validation_active("
        "void observe_stage11d_abyss_claim(")
    string(FIND "${_stage11d_runtime_text}"
        "${_stage11d_definition_token}" _stage11d_definition)
    if(_stage11d_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D runtime definition is missing: ${_stage11d_definition_token}")
    endif()
    stage11d_find_host_code_token("${_stage11d_definition_token}"
        _host_stage11d_definition)
    if(NOT _host_stage11d_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D runtime definition remains in raylib_host.cpp: ${_stage11d_definition_token}")
    endif()
endforeach()
evidence_sanitize_cpp_for_scan("${_stage11d_header_text}"
    _stage11d_header_code)
if(NOT _stage11d_header_code MATCHES
        "struct[ \t\r\n]+Stage11DLootValidationState[ \t\r\n]+final[ \t\r\n]*[{]")
    message(FATAL_ERROR "Host validation Stage11D state definition is missing")
endif()
stage11d_find_host_code_token("struct Stage11DLootValidationState final"
    _host_stage11d_state_definition)
if(NOT _host_stage11d_state_definition EQUAL -1)
    message(FATAL_ERROR
        "Host validation Stage11D state definition remains in raylib_host.cpp")
endif()

evidence_sanitize_cpp_for_scan("${_stage11d_report_text}"
    _stage11d_report_code)
foreach(_stage11d_report_definition IN ITEMS
        "bool stage11d_view_has_rarity("
        "void stage11d_record_semantics("
        "bool stage11d_target_visible("
        "const char* stage11d_scenario_name("
        "void write_stage11d_loot_validation_summary(")
    assert_unique_cpp_definition("Stage11D report"
        "${_stage11d_report_code}" "${_stage11d_report_definition}")
    stage11d_find_host_code_token("${_stage11d_report_definition}"
        _host_stage11d_report_definition)
    if(NOT _host_stage11d_report_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D report definition remains in raylib_host.cpp: ${_stage11d_report_definition}")
    endif()
endforeach()

foreach(_stage11d_report_declaration IN ITEMS
        "bool stage11d_target_visible("
        "void stage11d_record_semantics("
        "void write_stage11d_loot_validation_summary(")
    string(FIND "${_stage11d_header_code}"
        "${_stage11d_report_declaration}" _declaration)
    if(_declaration EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D report declaration is missing: ${_stage11d_report_declaration}")
    endif()
    string(LENGTH "${_stage11d_report_declaration}" _declaration_length)
    math(EXPR _after "${_declaration} + ${_declaration_length}")
    string(SUBSTRING "${_stage11d_header_code}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${_stage11d_report_declaration}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D report declaration is duplicated: ${_stage11d_report_declaration}")
    endif()
    string(SUBSTRING "${_stage11d_header_code}" ${_declaration} -1 _tail)
    string(FIND "${_tail}" ";" _semicolon)
    string(FIND "${_tail}" "{" _open)
    if(_semicolon EQUAL -1 OR (NOT _open EQUAL -1 AND _open LESS _semicolon))
        message(FATAL_ERROR
            "Host validation Stage11D report declaration is not header-only: ${_stage11d_report_declaration}")
    endif()
endforeach()

set(_input_definition_tokens
    "void inject_validation_pressed("
    "void inject_validation_action("
    "void inject_validation_movement(")
set(_navigation_definition_tokens
    "combat::MovementInput validation_route_fire_movement("
    "const combat::MonsterSnapshot* nearest_living_monster("
    "combat::MovementInput validation_movement_toward("
    "combat::Vec3 validation_door_position("
    "combat::MovementInput validation_exit_movement("
    "bool validation_attack_lane("
    "dungeon::ExitDirection validation_direction(")
foreach(_token IN LISTS _input_definition_tokens)
    string(FIND "${_input_source_text}" "${_token}" _definition)
    if(_definition EQUAL -1)
        message(FATAL_ERROR "Host validation input definition is missing: ${_token}")
    endif()
    string(FIND "${_host_text}" "${_token}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR "Host validation input helper remains in raylib_host.cpp: ${_token}")
    endif()
endforeach()
foreach(_token IN LISTS _navigation_definition_tokens)
    string(FIND "${_navigation_source_text}" "${_token}" _definition)
    if(_definition EQUAL -1)
        message(FATAL_ERROR "Host validation navigation definition is missing: ${_token}")
    endif()
    string(FIND "${_host_text}" "${_token}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR "Host validation navigation helper remains in raylib_host.cpp: ${_token}")
    endif()
endforeach()

foreach(_old_helper IN ITEMS
        "void inject_stage11b_pressed(" "void inject_stage11c_binding("
        "void inject_stage11c_movement(")
    string(FIND "${_host_text}" "${_old_helper}" _old_definition)
    if(NOT _old_definition EQUAL -1)
        message(FATAL_ERROR "Host validation boundary old helper remains: ${_old_helper}")
    endif()
endforeach()

function(assert_stage17_top_level_registration CMAKE_TEXT STAGE17_SOURCE LABEL)
    arpg_cmake_code_surface("${CMAKE_TEXT}" _cmake_code)
    string(TOLOWER "${_cmake_code}" _cmake_lower)
    string(LENGTH "${_cmake_lower}" _cmake_length)
    set(_scan 0)
    set(_block_depth 0)
    set(_top_level_count 0)
    set(_registration_begin -1)
    while(_scan LESS _cmake_length)
        string(SUBSTRING "${_cmake_lower}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "\n" _newline)
        if(_newline EQUAL -1)
            set(_line "${_tail}")
        else()
            string(SUBSTRING "${_tail}" 0 ${_newline} _line)
        endif()
        string(REGEX REPLACE "^[ \t]*" "" _trimmed "${_line}")

        # A direct return, or a return embedded in a macro declared before the
        # registration and then invoked, can make a textually top-level source
        # list unreachable.  The production file registers on its first
        # command, so conservatively reject every pre-registration return.
        if(_registration_begin EQUAL -1
                AND _trimmed MATCHES "^return[ \t]*\\(")
            message(FATAL_ERROR
                "${LABEL} registration is unreachable after a pre-registration return")
        endif()

        if(_trimmed MATCHES
                "^end(if|function|macro|foreach|while|block)[ \t]*\\(")
            if(_block_depth GREATER 0)
                math(EXPR _block_depth "${_block_depth} - 1")
            endif()
        endif()

        if(_trimmed MATCHES
                "^add_library[ \t]*\\([ \t]*arpg_raylib[ \t]+static([ \t]|$)")
            if(_block_depth EQUAL 0)
                math(EXPR _top_level_count "${_top_level_count} + 1")
                string(FIND "${_line}" "add_library" _line_command)
                math(EXPR _registration_begin "${_scan} + ${_line_command}")
            endif()
        endif()

        if(_trimmed MATCHES
                "^(if|function|macro|foreach|while|block)[ \t]*\\(")
            math(EXPR _block_depth "${_block_depth} + 1")
        endif()
        if(_newline EQUAL -1)
            break()
        endif()
        math(EXPR _scan "${_scan} + ${_newline} + 1")
    endwhile()

    if(NOT _top_level_count EQUAL 1)
        message(FATAL_ERROR
            "${LABEL} must be registered by one top-level add_library(arpg_raylib STATIC ...)")
    endif()

    string(SUBSTRING "${_cmake_lower}" ${_registration_begin} -1
        _registration_tail)
    string(FIND "${_registration_tail}" "(" _open_relative)
    if(_open_relative EQUAL -1)
        message(FATAL_ERROR "Stage17 top-level add_library command has no opening parenthesis")
    endif()
    math(EXPR _registration_open
        "${_registration_begin} + ${_open_relative}")
    math(EXPR _cmake_last "${_cmake_length} - 1")
    set(_paren_depth 0)
    set(_registration_end -1)
    foreach(_index RANGE ${_registration_open} ${_cmake_last})
        string(SUBSTRING "${_cmake_lower}" ${_index} 1 _character)
        if(_character STREQUAL "(")
            math(EXPR _paren_depth "${_paren_depth} + 1")
        elseif(_character STREQUAL ")")
            math(EXPR _paren_depth "${_paren_depth} - 1")
            if(_paren_depth EQUAL 0)
                set(_registration_end ${_index})
                break()
            endif()
        endif()
    endforeach()
    if(_registration_end EQUAL -1)
        message(FATAL_ERROR "Stage17 top-level add_library command is unbalanced")
    endif()
    math(EXPR _registration_length
        "${_registration_end} - ${_registration_begin} + 1")
    string(SUBSTRING "${_cmake_lower}" ${_registration_begin}
        ${_registration_length} _registration)

    set(_stage17_source "${STAGE17_SOURCE}")
    string(FIND "${_registration}" "${_stage17_source}" _source_position)
    if(_source_position EQUAL -1)
        message(FATAL_ERROR
            "${LABEL} is not a direct top-level arpg_raylib source")
    endif()
    string(LENGTH "${_stage17_source}" _source_length)
    math(EXPR _source_after "${_source_position} + ${_source_length}")
    string(SUBSTRING "${_registration}" ${_source_after} -1 _source_remainder)
    string(FIND "${_source_remainder}" "${_stage17_source}" _source_duplicate)
    if(NOT _source_duplicate EQUAL -1)
        message(FATAL_ERROR
            "${LABEL} is duplicated in the top-level arpg_raylib source list")
    endif()
    if(_source_position EQUAL 0)
        set(_source_before "")
    else()
        math(EXPR _source_before_position "${_source_position} - 1")
        string(SUBSTRING "${_registration}" ${_source_before_position} 1
            _source_before)
    endif()
    string(SUBSTRING "${_registration}" ${_source_after} 1 _source_after_character)
    if(NOT _source_before MATCHES "[ \t\r\n(]"
            OR NOT _source_after_character MATCHES "[ \t\r\n)]")
        message(FATAL_ERROR
            "${LABEL} registration is not a direct add_library argument")
    endif()
    string(SUBSTRING "${_registration}" 0 ${_source_position}
        _registration_prefix)
    arpg_cmake_code_line_and_paren_delta("${_registration_prefix}"
        _source_paren_depth)
    if(NOT _source_paren_depth EQUAL 1)
        message(FATAL_ERROR
            "${LABEL} registration is nested inside an add_library argument")
    endif()
endfunction()

foreach(_registered_source IN ITEMS
        "host_validation_input.cpp" "host_validation_navigation.cpp"
        "host_validation_stage10_11.cpp" "host_validation_stage11b.cpp"
        "host_validation_stage11c.cpp"
        "host_validation_stage11d_report.cpp"
        "host_validation_stage11d_runtime.cpp"
        "host_validation_stage17_report.cpp"
        "host_validation_stage17_runtime.cpp")
    arpg_cmake_count_arpg_raylib_source("${_raylib_cmake_text}"
        "${_registered_source}" _registered_count)
    if(NOT _registered_count EQUAL 1)
        message(FATAL_ERROR "arpg_raylib does not register ${_registered_source}")
    endif()
endforeach()
assert_stage17_top_level_registration("${_raylib_cmake_text}"
    "host_validation_stage17_report.cpp" "Stage17 report")
assert_stage17_top_level_registration("${_raylib_cmake_text}"
    "host_validation_stage17_runtime.cpp" "Stage17 runtime")

string(FIND "${_sanitized}" "while (!exit_requested) {" _loop_begin)
if(_loop_begin EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard cannot isolate host loop")
endif()
math(EXPR _loop_after "${_loop_begin} + 1")
string(SUBSTRING "${_sanitized}" ${_loop_after} -1 _loop_remainder)
string(FIND "${_loop_remainder}" "while (!exit_requested) {" _loop_duplicate)
if(NOT _loop_duplicate EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard found duplicate host loop")
endif()
cpp_token_brace_depth("${_sanitized}" ${_loop_begin} _loop_scope_depth)
if(NOT DEFINED HOST_OVERRIDE AND NOT _loop_scope_depth EQUAL 2)
    message(FATAL_ERROR
        "Host validation sequence guard rejected host loop outside run/try scope")
endif()
string(SUBSTRING "${_sanitized}" ${_loop_begin} -1 _loop_tail)
string(FIND "${_loop_tail}" "{" _loop_open_relative)
if(_loop_open_relative EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard cannot find host loop brace")
endif()
math(EXPR _loop_open "${_loop_begin} + ${_loop_open_relative}")
string(LENGTH "${_sanitized}" _runtime_length)
math(EXPR _runtime_last "${_runtime_length} - 1")
set(_loop_depth 0)
set(_loop_end -1)
foreach(_index RANGE ${_loop_open} ${_runtime_last})
    string(SUBSTRING "${_sanitized}" ${_index} 1 _character)
    if(_character STREQUAL "{")
        math(EXPR _loop_depth "${_loop_depth} + 1")
    elseif(_character STREQUAL "}")
        math(EXPR _loop_depth "${_loop_depth} - 1")
        if(_loop_depth EQUAL 0)
            set(_loop_end ${_index})
            break()
        endif()
    endif()
endforeach()
if(_loop_end EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard found unbalanced host loop")
endif()
math(EXPR _loop_length "${_loop_end} - ${_loop_begin} + 1")
string(SUBSTRING "${_sanitized}" ${_loop_begin} ${_loop_length} _host_loop)

function(assert_unique_ordered_host_tokens LABEL SURFACE REQUIRED_DEPTH)
    set(_previous -1)
    foreach(_token IN ITEMS ${ARGN})
        string(FIND "${SURFACE}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard missing ${LABEL} token: ${_token}")
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
        string(FIND "${_remainder}" "${_token}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard found duplicate ${LABEL} token: ${_token}")
        endif()
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR "Host validation sequence guard rejected ${LABEL} order")
        endif()
        cpp_token_brace_depth("${SURFACE}" ${_position} _token_depth)
        if(NOT _token_depth EQUAL REQUIRED_DEPTH)
            message(FATAL_ERROR "Host validation sequence guard rejected ${LABEL} token outside direct host scope: ${_token}")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

string(FIND "${_host_loop}" "validation_runtime->inject_physical_edges("
    _facade_input_chain)
if(DEFINED HOST_OVERRIDE AND _facade_input_chain EQUAL -1)
    # Legacy synthetic HOST_OVERRIDE fixtures predate Task 7A. Preserve their
    # route to the original targeted diagnostics without weakening production.
    assert_unique_ordered_host_tokens("input injection chain" "${_host_loop}" 1
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "inject_stage11b_physical_edges("
        "inject_stage11c_physical_edges("
        "inject_stage11d_physical_edges("
        "inject_stage17_physical_edges("
        "map_host_frame_input(")
else()
    assert_unique_ordered_host_tokens("input injection chain" "${_host_loop}" 1
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "const bool gameplay_rearm_was_required ="
        "runtime.gameplay_rearm_required();"
        "validation_runtime->inject_physical_edges("
        "HostValidationStateAccess::death_input_snapshot("
        "gameplay_controls_physically_released("
        "map_host_frame_input(")
    set(_rearm_acknowledgement "runtime.acknowledge_gameplay_rearmed();")
    string(FIND "${_host_loop}" "${_rearm_acknowledgement}" _rearm_ack_position)
    if(_rearm_ack_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation sequence guard is missing gameplay rearm acknowledgement occurrence")
    endif()
    math(EXPR _after_rearm_ack "${_rearm_ack_position} + 1")
    string(SUBSTRING "${_host_loop}" ${_after_rearm_ack} -1 _rearm_ack_tail)
    string(FIND "${_rearm_ack_tail}" "${_rearm_acknowledgement}"
        _extra_rearm_ack)
    if(NOT _extra_rearm_ack EQUAL -1)
        message(FATAL_ERROR
            "Host validation sequence guard found extra gameplay rearm acknowledgement occurrence")
    endif()
    cpp_token_brace_depth("${_host_loop}" ${_rearm_ack_position}
        _rearm_ack_depth)
    if(NOT _rearm_ack_depth EQUAL 2)
        message(FATAL_ERROR
            "Host validation sequence guard rejected gameplay rearm acknowledgement scope: expected=2, actual=${_rearm_ack_depth}")
    endif()
endif()

string(FIND "${_host_loop}" "if (!step_death) {" _fixed_step_begin)
string(FIND "${_host_loop}" "runtime.fixed_tick(step_movement,"
    _fixed_step_end)
if(_fixed_step_begin EQUAL -1 OR _fixed_step_end EQUAL -1
        OR NOT _fixed_step_begin LESS _fixed_step_end)
    message(FATAL_ERROR "Host validation sequence guard cannot isolate fixed-step movement branch")
endif()
math(EXPR _fixed_step_length "${_fixed_step_end} - ${_fixed_step_begin}")
string(SUBSTRING "${_host_loop}" ${_fixed_step_begin} ${_fixed_step_length}
    _fixed_step_branch)
string(REGEX REPLACE "[ \t\r\n]+" " " _fixed_step_normalized
    "${_fixed_step_branch}")
string(REGEX MATCH
    "if \\(!step_death\\) \\{ if \\(config\\.stage11_validation != Stage11ValidationScenario::none\\) \\{ step_movement = host_validation::stage11_validation_input\\(.*\\); \\} else if \\(config\\.stage10_validation != Stage10ValidationScenario::none\\) \\{ step_movement = host_validation::stage10_validation_input\\(.*\\); \\} else \\{ step_movement = movement; \\} \\}"
    _fixed_step_priority_structure "${_fixed_step_normalized}")
if(NOT _fixed_step_priority_structure)
    message(FATAL_ERROR "Host validation sequence guard rejected fixed-step movement priority structure")
endif()

function(assert_unique_fixed_step_token TOKEN)
    string(FIND "${_fixed_step_branch}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Host validation sequence guard missing fixed-step movement token: ${TOKEN}")
    endif()
    math(EXPR _after "${_position} + 1")
    string(SUBSTRING "${_fixed_step_branch}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "Host validation sequence guard found duplicate fixed-step movement token: ${TOKEN}")
    endif()
endfunction()

foreach(_fixed_step_token IN ITEMS
        "stage11_validation_input("
        "stage10_validation_input("
        "step_movement = movement;")
    assert_unique_fixed_step_token("${_fixed_step_token}")
endforeach()

function(count_nonempty_regex_matches SURFACE PATTERN OUT_COUNT)
    set(_remainder "${SURFACE}")
    set(_count 0)
    while(TRUE)
        string(REGEX MATCH "${PATTERN}" _match "${_remainder}")
        if(_match STREQUAL "")
            break()
        endif()
        string(FIND "${_remainder}" "${_match}" _position)
        string(LENGTH "${_match}" _length)
        if(_position EQUAL -1 OR _length EQUAL 0)
            message(FATAL_ERROR
                "Host validation sequence guard encountered an invalid regex match")
        endif()
        math(EXPR _after "${_position} + ${_length}")
        string(SUBSTRING "${_remainder}" ${_after} -1 _remainder)
        math(EXPR _count "${_count} + 1")
    endwhile()
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(assert_one_normalized_match LABEL SURFACE PATTERN)
    count_nonempty_regex_matches("${SURFACE}" "${PATTERN}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Host validation sequence guard expected one ${LABEL} match, found ${_count}")
    endif()
endfunction()

function(assert_direct_exact_call_count LABEL SURFACE TOKEN_PATTERN
        DIRECT_PATTERN EXPECTED_COUNT)
    count_nonempty_regex_matches("${SURFACE}" "${TOKEN_PATTERN}" _all_count)
    count_nonempty_regex_matches("${SURFACE}" "${DIRECT_PATTERN}"
        _direct_count)
    if(NOT _all_count EQUAL EXPECTED_COUNT
            OR NOT _direct_count EQUAL EXPECTED_COUNT)
        message(FATAL_ERROR
            "Host validation sequence guard rejected ${LABEL}: all=${_all_count}, direct_exact=${_direct_count}, expected=${EXPECTED_COUNT}")
    endif()
endfunction()

function(assert_unique_ordered_tokens LABEL SURFACE)
    set(_previous -1)
    foreach(_token IN ITEMS ${ARGN})
        string(FIND "${SURFACE}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard missing ${LABEL} token: ${_token}")
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
        string(FIND "${_remainder}" "${_token}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard found duplicate ${LABEL} token: ${_token}")
        endif()
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR "Host validation sequence guard rejected ${LABEL} order")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

# Shared, non-fatal lifecycle validator. Production converts its error code to
# a fatal diagnostic; direct in-memory negative fixtures assert the same code.
macro(stage17_lifecycle_competition_token TOKEN EXPECTED_DEPTH)
    if(_error STREQUAL "")
        string(FIND "${_loop}" "${TOKEN}" _position)
        if(_position EQUAL -1)
            set(_error "capture_competition_missing")
        else()
            math(EXPR _token_after "${_position} + 1")
            string(SUBSTRING "${_loop}" ${_token_after} -1 _remaining)
            string(FIND "${_remaining}" "${TOKEN}" _duplicate)
            if(NOT _duplicate EQUAL -1)
                set(_error "capture_competition_duplicate")
            elseif(NOT _previous EQUAL -1 AND _position LESS _previous)
                set(_error "capture_competition_order")
            else()
                cpp_token_brace_depth("${_loop}" ${_position} _actual_depth)
                if(NOT _actual_depth EQUAL ${EXPECTED_DEPTH})
                    set(_error "capture_competition_scope")
                endif()
            endif()
            set(_previous ${_position})
        endif()
    endif()
endmacro()

function(stage17_capture_lifecycle_status HOST_ACTIVE OUT_ERROR)
    set(_error "")
    evidence_find_cpp_code_token("${HOST_ACTIVE}" "HostExitCode run_raylib_host(" _run_begin)
    if(_run_begin EQUAL -1)
        set(${OUT_ERROR} "missing_run_host" PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${HOST_ACTIVE}" ${_run_begin} -1 _run_tail)
    evidence_extract_cpp_function_block("${_run_tail}" "HostExitCode run_raylib_host(" _run)
    string(FIND "${_run}" "while (!exit_requested) {" _loop_begin)
    if(_loop_begin EQUAL -1)
        set(${OUT_ERROR} "missing_loop" PARENT_SCOPE)
        return()
    endif()
    math(EXPR _after "${_loop_begin} + 1")
    string(SUBSTRING "${_run}" ${_after} -1 _loop_tail)
    string(FIND "${_loop_tail}" "{" _open_relative)
    math(EXPR _open "${_loop_begin} + ${_open_relative}")
    string(LENGTH "${_run}" _length)
    math(EXPR _last "${_length} - 1")
    set(_depth 0)
    set(_end -1)
    foreach(_index RANGE ${_open} ${_last})
        string(SUBSTRING "${_run}" ${_index} 1 _character)
        if(_character STREQUAL "{")
            math(EXPR _depth "${_depth} + 1")
        elseif(_character STREQUAL "}")
            math(EXPR _depth "${_depth} - 1")
            if(_depth EQUAL 0)
                set(_end ${_index})
                break()
            endif()
        endif()
    endforeach()
    if(_end EQUAL -1)
        set(${OUT_ERROR} "unbalanced_loop" PARENT_SCOPE)
        return()
    endif()
    math(EXPR _loop_length "${_end} - ${_loop_begin} + 1")
    string(SUBSTRING "${_run}" ${_loop_begin} ${_loop_length} _loop)

    set(_previous -1)
    stage17_lifecycle_competition_token("stage17_capture_path(" 1)
    stage17_lifecycle_competition_token(
        "const bool stage17_capture_requested = capture_path.has_value();" 1)
    stage17_lifecycle_competition_token(
        "if (!capture_path.has_value() && stage12_item_baseline_frame) {" 1)
    stage17_lifecycle_competition_token(
        "if (!capture_path.has_value()\n                    && (validation_reached || loot_validation_visible_capture" 1)
    stage17_lifecycle_competition_token(
        "if (death_gate.screenshot || (config.validation_request_screenshot" 1)
    stage17_lifecycle_competition_token(
        "} else if (!capture_path.has_value()\n                    && config.validation_capture_file.has_value()" 2)
    stage17_lifecycle_competition_token(
        "} else if (!capture_path.has_value()) {" 2)
    stage17_lifecycle_competition_token("const bool capture_succeeded =" 1)
    if(NOT _error STREQUAL "")
        set(${OUT_ERROR} "${_error}" PARENT_SCOPE)
        return()
    endif()
    string(FIND "${_loop}" "mark_stage17_capture_complete(" _mark_position)
    if(_mark_position EQUAL -1)
        set(${OUT_ERROR} "capture_mark_missing" PARENT_SCOPE)
        return()
    endif()
    cpp_token_brace_depth("${_loop}" ${_mark_position} _mark_depth)
    if(NOT _mark_depth EQUAL 2)
        set(${OUT_ERROR} "capture_mark_scope" PARENT_SCOPE)
        return()
    endif()
    string(FIND "${_run}" "stage17_capture_path(" _request_position)
    string(FIND "${_run}" "write_stage17_validation_summary(" _summary_position)
    if(_request_position EQUAL -1 OR _summary_position EQUAL -1
            OR NOT _request_position LESS _summary_position)
        set(${OUT_ERROR} "capture_summary_order" PARENT_SCOPE)
        return()
    endif()
    math(EXPR _return_window_length "${_summary_position} - ${_request_position}")
    string(SUBSTRING "${_run}" ${_request_position} ${_return_window_length}
        _return_window)
    string(FIND "${_return_window}" "return" _return_position)
    if(NOT _return_position EQUAL -1)
        set(${OUT_ERROR} "capture_early_return" PARENT_SCOPE)
        return()
    endif()
    set(${OUT_ERROR} "" PARENT_SCOPE)
endfunction()

# This validation deliberately starts from the complete, active host source.
# It rejects evidence copied into a lambda, a different function, comments, or
# an inactive branch before narrowing to the one executable host loop.
function(assert_stage17_capture_lifecycle HOST_SOURCE LABEL)
    stage17_mask_cpp_conditionals("${HOST_SOURCE}" _host_active)
    stage17_capture_lifecycle_status("${_host_active}" _lifecycle_error)
    if(NOT _lifecycle_error STREQUAL "")
        message(FATAL_ERROR "Stage17 lifecycle ${LABEL} rejected: ${_lifecycle_error}")
    endif()
    evidence_find_cpp_code_token("${_host_active}" "HostExitCode run_raylib_host("
        _run_begin)
    if(_run_begin EQUAL -1)
        message(FATAL_ERROR "Stage17 lifecycle ${LABEL} is missing run_raylib_host")
    endif()
    string(SUBSTRING "${_host_active}" ${_run_begin} -1 _run_tail)
    evidence_extract_cpp_function_block("${_run_tail}"
        "HostExitCode run_raylib_host(" _run_host)
    evidence_extract_cpp_function_block("${_host_active}"
        "bool present_frame_and_maybe_capture(" _present_helper)

    set(_all_active "${_host_active}\n${_stage17_report_active_text}\n${_stage17_runtime_active_text}\n${_stage17_header_active_text}")
    foreach(_raylib_api IN ITEMS "EndDrawing\\(" "LoadImageFromScreen\\(" "ExportImage\\(")
        count_nonempty_regex_matches("${_all_active}" "${_raylib_api}" _api_count)
        if(NOT _api_count EQUAL 1)
            message(FATAL_ERROR "Stage17 lifecycle ${LABEL} raylib API is not helper-owned: ${_raylib_api}")
        endif()
    endforeach()
    assert_unique_ordered_host_tokens("Stage17 ${LABEL} present helper"
        "${_present_helper}" 1 "EndDrawing();" "LoadImageFromScreen();"
        "ExportImage(image, path);")

    string(FIND "${_run_host}" "while (!exit_requested) {" _loop_begin)
    if(_loop_begin EQUAL -1)
        message(FATAL_ERROR "Stage17 lifecycle ${LABEL} cannot isolate host loop")
    endif()
    math(EXPR _loop_after "${_loop_begin} + 1")
    string(SUBSTRING "${_run_host}" ${_loop_after} -1 _loop_tail)
    string(FIND "${_loop_tail}" "{" _loop_open_relative)
    math(EXPR _loop_open "${_loop_begin} + ${_loop_open_relative}")
    string(LENGTH "${_run_host}" _run_length)
    math(EXPR _run_last "${_run_length} - 1")
    set(_loop_depth 0)
    set(_loop_end -1)
    foreach(_index RANGE ${_loop_open} ${_run_last})
        string(SUBSTRING "${_run_host}" ${_index} 1 _character)
        if(_character STREQUAL "{")
            math(EXPR _loop_depth "${_loop_depth} + 1")
        elseif(_character STREQUAL "}")
            math(EXPR _loop_depth "${_loop_depth} - 1")
            if(_loop_depth EQUAL 0)
                set(_loop_end ${_index})
                break()
            endif()
        endif()
    endforeach()
    if(_loop_end EQUAL -1)
        message(FATAL_ERROR "Stage17 lifecycle ${LABEL} found unbalanced host loop")
    endif()
    math(EXPR _loop_length "${_loop_end} - ${_loop_begin} + 1")
    string(SUBSTRING "${_run_host}" ${_loop_begin} ${_loop_length} _loop)

    assert_token_depth_sequence("Stage17 ${LABEL} capture request" "${_loop}"
        "stage17_capture_path(" 1)
    assert_token_depth_sequence("Stage17 ${LABEL} capture requested binding" "${_loop}"
        "const bool stage17_capture_requested = capture_path.has_value();" 1)
    assert_token_depth_sequence("Stage17 ${LABEL} capture success binding" "${_loop}"
        "const bool capture_succeeded =" 1)
    assert_token_depth_sequence("Stage17 ${LABEL} capture completion mark" "${_loop}"
        "mark_stage17_capture_complete(" 2)
    assert_unique_ordered_tokens("Stage17 ${LABEL} capture competition"
        "${_loop}"
        "stage17_capture_path("
        "const bool stage17_capture_requested = capture_path.has_value();"
        "if (!capture_path.has_value() && stage12_item_baseline_frame) {"
        "if (!capture_path.has_value()\n                    && (validation_reached || loot_validation_visible_capture"
        "if (death_gate.screenshot || (config.validation_request_screenshot"
        "} else if (!capture_path.has_value()\n                    && config.validation_capture_file.has_value()"
        "} else if (!capture_path.has_value()) {"
        "const bool capture_succeeded =")
    string(FIND "${_loop}" "stage17_capture_path(" _capture_request_position)
    string(FIND "${_loop}" "mark_stage17_capture_complete(" _capture_mark_position)
    math(EXPR _capture_segment_length
        "${_capture_mark_position} - ${_capture_request_position}")
    string(SUBSTRING "${_loop}" ${_capture_request_position}
        ${_capture_segment_length} _capture_segment)
    string(FIND "${_capture_segment}" "return" _early_return)
    if(NOT _early_return EQUAL -1)
        message(FATAL_ERROR "Stage17 lifecycle ${LABEL} has early return before capture completion")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" " " _capture_normalized "${_loop}")
    assert_one_normalized_match("Stage17 ${LABEL} capture request RHS"
        "${_capture_normalized}"
        "std::optional<std::string>[ ]+capture_path[ ]*=[ ]*stage17_capture_path\\([ ]*config,[ ]*[*]stage17_validation_state[ ]*\\)[ ]*;")
    assert_one_normalized_match("Stage17 ${LABEL} capture success RHS"
        "${_capture_normalized}"
        "const[ ]+bool[ ]+capture_succeeded[ ]*=[ ]*present_frame_and_maybe_capture\\([ ]*capture_path[.]has_value\\([ ]*\\)[ ]*\\?[ ]*capture_path->c_str\\([ ]*\\)[ ]*:[ ]*nullptr[ ]*\\)[ ]*;")
    assert_one_normalized_match("Stage17 ${LABEL} gated capture mark"
        "${_capture_normalized}"
        "if[ ]*\\([ ]*stage17_capture_requested[ ]*&&[ ]*capture_succeeded[ ]*\\)[ ]*\\{[ ]*mark_stage17_capture_complete\\([ ]*[*]stage17_validation_state[ ]*\\)[ ]*;")

    string(FIND "${_run_host}" "stage17_validation_state->clean_shutdown_exact_ready ="
        _shutdown_position)
    if(NOT _shutdown_position GREATER _loop_end)
        message(FATAL_ERROR "Stage17 lifecycle ${LABEL} shutdown assignment is not post-loop")
    endif()
    assert_one_normalized_match("Stage17 ${LABEL} exact shutdown assignment"
        "${_run_host}"
        "stage17_validation_state->clean_shutdown_exact_ready[ \t\r\n]*=[ \t\r\n]*runtime[.]clean_shutdown_state\\([ \t\r\n]*\\)[ \t\r\n]*==[ \t\r\n]*CleanShutdownState::ready[ \t\r\n]*;")
    assert_unique_ordered_host_tokens("Stage17 ${LABEL} shutdown summaries"
        "${_run_host}" 2
        "stage17_validation_state->clean_shutdown_exact_ready ="
        "write_stage11b_validation_summary("
        "write_stage11c_hud_validation_summary("
        "write_stage11d_loot_validation_summary("
        "write_stage17_validation_summary(")
endfunction()

# Keep the new adversarial cases in this direct production guard.  Legacy
# HOST_OVERRIDE fixtures retain their focused historical assertions without
# repeatedly parsing these extra Stage17-only source variants.
function(assert_stage17_lifecycle_error NAME SOURCE EXPECTED_ERROR)
    stage17_capture_lifecycle_status("${SOURCE}" _actual_error)
    if(NOT _actual_error STREQUAL "${EXPECTED_ERROR}")
        message(FATAL_ERROR
            "Stage17 lifecycle mutation ${NAME} expected ${EXPECTED_ERROR}, got ${_actual_error}")
    endif()
endfunction()

function(assert_stage17_lifecycle_memory_mutations HOST_SOURCE)
    set(_request [=[std::optional<std::string> capture_path = stage17_capture_path(
                config, *stage17_validation_state);]=])
    set(_requested [=[const bool stage17_capture_requested = capture_path.has_value();]=])
    set(_mark [=[mark_stage17_capture_complete(*stage17_validation_state);]=])
    stage17_mask_cpp_conditionals("${HOST_SOURCE}" _mutation_active)

    # These are complete source variants passed to the same pure lifecycle
    # validator as production; no mutation merely checks its own construction.
    string(REPLACE "${_mark}" "mark_stage17_capture_complete_removed(*stage17_validation_state);"
        _deleted_mark "${_mutation_active}")
    assert_stage17_lifecycle_error("deleted completion mark" "${_deleted_mark}"
        "capture_mark_missing")

    # Request moved after the first competing branch must invert the required
    # direct-loop order.  This guards against priority regressions.
    string(REPLACE "${_request}" "stage17_capture_path_removed(config, *stage17_validation_state);"
        _moved_request "${_mutation_active}")
    string(REPLACE "if (!capture_path.has_value() && stage12_item_baseline_frame) {"
        "if (!capture_path.has_value() && stage12_item_baseline_frame) {\n${_request}"
        _moved_request "${_moved_request}")
    assert_stage17_lifecycle_error("request after competing capture branch"
        "${_moved_request}" "capture_competition_scope")

    # The return window intentionally runs through the final Stage17 summary.
    string(REPLACE "${_requested}" "${_requested}\n            return HostExitCode::success;"
        _early_return "${_mutation_active}")
    assert_stage17_lifecycle_error("request-to-summary early return"
        "${_early_return}" "capture_early_return")

    set(_success [=[const bool capture_succeeded =
                present_frame_and_maybe_capture(capture_path.has_value()
                    ? capture_path->c_str() : nullptr);]=])
    set(_lambda [=[const auto stage17_uncalled_capture_lambda = [&]() noexcept {
                const bool capture_succeeded = present_frame_and_maybe_capture(
                    capture_path.has_value() ? capture_path->c_str() : nullptr);
                return capture_succeeded;
            };]=])
    string(REPLACE "${_success}" "${_lambda}" _lambda_mutation "${_mutation_active}")
    assert_stage17_lifecycle_error("uncalled lambda capture decoy"
        "${_lambda_mutation}" "capture_competition_scope")

    string(REPLACE "${_mark}" "mark_stage17_capture_complete_removed(*stage17_validation_state);"
        _cross_function "${_mutation_active}")
    string(APPEND _cross_function [=[
void stage17_cross_function_capture_decoy() {
    mark_stage17_capture_complete(*stage17_validation_state);
}
]=])
    assert_stage17_lifecycle_error("cross-function capture mark decoy"
        "${_cross_function}" "capture_mark_missing")
endfunction()

function(assert_token_depth_sequence LABEL SURFACE TOKEN)
    set(_offset 0)
    foreach(_expected_depth IN ITEMS ${ARGN})
        string(SUBSTRING "${SURFACE}" ${_offset} -1 _tail)
        string(FIND "${_tail}" "${TOKEN}" _relative)
        if(_relative EQUAL -1)
            message(FATAL_ERROR
                "Host validation sequence guard is missing ${LABEL} occurrence")
        endif()
        math(EXPR _position "${_offset} + ${_relative}")
        cpp_token_brace_depth("${SURFACE}" ${_position} _actual_depth)
        if(NOT _actual_depth EQUAL _expected_depth)
            message(FATAL_ERROR
                "Host validation sequence guard rejected ${LABEL} scope: expected=${_expected_depth}, actual=${_actual_depth}")
        endif()
        math(EXPR _offset "${_position} + 1")
    endforeach()
    string(SUBSTRING "${SURFACE}" ${_offset} -1 _tail)
    string(FIND "${_tail}" "${TOKEN}" _extra)
    if(NOT _extra EQUAL -1)
        message(FATAL_ERROR
            "Host validation sequence guard found extra ${LABEL} occurrence")
    endif()
endfunction()

string(REGEX REPLACE "[ \t\r\n]+" " " _host_loop_normalized
    "${_host_loop}")
string(REGEX REPLACE "[ \t\r\n]+" " " _host_runtime_normalized
    "${_host_runtime}")
assert_one_normalized_match("sampled physical-key producer"
    "${_host_loop_normalized}"
    "const[ ]+PhysicalKeySnapshot[ ]+sampled_physical_keys[ ]*=[ ]*sample_physical_keys\\([ ]*\\)")
assert_one_normalized_match("validation facade physical-key binding"
    "${_host_loop_normalized}"
    "const[ ]+PhysicalKeySnapshot[ ]+stage17_physical_keys[ ]*=[ ]*validation_runtime->inject_physical_edges\\([ ]*sampled_physical_keys,[ ]*input_settings,[ ]*current,[ ]*gameplay_rearm_was_required[ ]*\\)")
assert_one_normalized_match("cached Stage11D physical-key binding"
    "${_host_loop_normalized}"
    "const[ ]+PhysicalKeySnapshot&[ ]+physical_keys[ ]*=[ ]*HostValidationStateAccess::death_input_snapshot\\([ ]*[*]validation_runtime[ ]*\\)")
assert_one_normalized_match("final Stage17 release/rearm binding"
    "${_host_loop_normalized}"
    "if[ ]*\\([ ]*gameplay_rearm_was_required[ ]*&&[ ]*gameplay_controls_physically_released\\([ ]*stage17_physical_keys[ ]*\\)[ ]*\\)[ ]*\\{[ ]*runtime[.]acknowledge_gameplay_rearmed\\([ ]*\\)[ ]*;")
assert_one_normalized_match("mapped Stage17 physical-key consumer"
    "${_host_loop_normalized}"
    "HostFrameInput[ ]+frame_input[ ]*=[ ]*map_host_frame_input\\([ ]*input_settings,[ ]*stage17_physical_keys[ ]*\\)")
assert_one_normalized_match("pre-Stage17 death-gate physical-key consumer"
    "${_host_loop_normalized}"
    "DeathInputGate[ ]+death_gate[ ]*=[ ]*host_death_input_gate\\([ ]*death_saving,[ ]*death_pending,[ ]*frame_input[.]keys,[ ]*physical_keys[ ]*\\)")

function(extract_unique_direct_cpp_block LABEL SURFACE SIGNATURE EXPECTED_DEPTH
        OUT_BLOCK OUT_BEGIN)
    string(FIND "${SURFACE}" "${SIGNATURE}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation sequence guard cannot isolate ${LABEL}: ${SIGNATURE}")
    endif()
    math(EXPR _after "${_position} + 1")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${SIGNATURE}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR
            "Host validation sequence guard found duplicate ${LABEL}: ${SIGNATURE}")
    endif()
    cpp_token_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Host validation sequence guard rejected ${LABEL} outside direct scope")
    endif()
    evidence_extract_cpp_function_block("${SURFACE}" "${SIGNATURE}" _block)
    set(${OUT_BLOCK} "${_block}" PARENT_SCOPE)
    set(${OUT_BEGIN} ${_position} PARENT_SCOPE)
endfunction()

evidence_find_cpp_code_token("${_host_active_text}" "void drain_events("
    _drain_events_begin)
evidence_find_cpp_code_token("${_host_active_text}" "AudioBusLevels audio_bus_levels("
    _drain_events_end)
if(_drain_events_begin EQUAL -1 OR _drain_events_end EQUAL -1
        OR NOT _drain_events_begin LESS _drain_events_end)
    message(FATAL_ERROR
        "Host validation sequence guard cannot isolate drain_events")
endif()
math(EXPR _drain_events_length
    "${_drain_events_end} - ${_drain_events_begin}")
string(SUBSTRING "${_host_active_text}" ${_drain_events_begin}
    ${_drain_events_length} _drain_events_slice)
evidence_sanitize_cpp_for_scan("${_drain_events_slice}"
    _drain_events_code)
assert_unique_cpp_definition("drain_events" "${_drain_events_code}"
    "void drain_events(")
evidence_extract_cpp_function_block("${_drain_events_code}" "void drain_events("
    _drain_events_block)
string(REGEX REPLACE "[ \t\r\n]+" " " _drain_events_normalized
    "${_drain_events_block}")
assert_one_normalized_match("drain_events validation-runtime signature"
    "${_drain_events_normalized}"
    "void[ ]+drain_events\\([ ]*dungeon::DungeonSession&[ ]+session,[ ]*CombatRenderer&[ ]+renderer,[ ]*CombatFeedback&[ ]+feedback,[ ]*GameAudio&[ ]+audio,[ ]*HostValidationRuntime[*][ ]+validation_runtime[ ]*\\)")
extract_unique_direct_cpp_block("combat-event drain loop"
    "${_drain_events_block}"
    "while (const auto event = session.try_pop_combat_event())" 1
    _combat_event_drain_loop _combat_event_drain_begin)
assert_unique_ordered_host_tokens("combat-event observer fanout"
    "${_combat_event_drain_loop}" 1
    "validation_runtime->observe_combat_event(*event);"
    "renderer.consume_event(*event);"
    "feedback.consume(*event);"
    "audio.consume_event(*event);")
string(REGEX REPLACE "[ \t\r\n]+" " " _combat_event_drain_normalized
    "${_combat_event_drain_loop}")
assert_one_normalized_match("complete direct combat-event observer fanout"
    "${_combat_event_drain_normalized}"
    "while[ ]*\\(const auto event = session[.]try_pop_combat_event\\([ ]*\\)\\)[ ]*\\{[ ]*validation_runtime->observe_combat_event\\([ ]*[*]event[ ]*\\)[ ]*;[ ]*renderer[.]consume_event\\([ ]*[*]event[ ]*\\)[ ]*;[ ]*feedback[.]consume\\([ ]*[*]event[ ]*\\)[ ]*;[ ]*audio[.]consume_event\\([ ]*[*]event[ ]*\\)[ ]*;[ ]*\\}")
assert_direct_exact_call_count("combat-event observer direct statement"
    "${_combat_event_drain_normalized}"
    "observe_combat_event\\("
    "([;{}])[ ]*validation_runtime->observe_combat_event\\([ ]*[*]event[ ]*\\)[ ]*;"
    1)

string(SUBSTRING "${_host_runtime}" 0 ${_loop_begin} _host_pre_loop)
extract_unique_direct_cpp_block("pre-loop initial-session block"
    "${_host_pre_loop}" "if (runtime.session() != nullptr)" 2
    _initial_session_block _initial_session_begin)
extract_unique_direct_cpp_block("loop-top session refresh block"
    "${_host_loop}" "if (runtime.session() != nullptr)" 1
    _loop_session_block _loop_session_begin)
assert_unique_ordered_host_tokens("pre-loop Stage17 event observation"
    "${_initial_session_block}" 1
    "runtime.session()->snapshot(current);"
    "previous = current;"
    "validation_runtime->observe_snapshot("
    "drain_events(")
assert_unique_ordered_host_tokens("loop-top Stage17 event refresh"
    "${_loop_session_block}" 1
    "previous = current;"
    "runtime.session()->snapshot(current);"
    "drain_events(")

assert_direct_exact_call_count("all Stage17 snapshot facade calls"
    "${_host_runtime_normalized}"
    "observe_snapshot\\("
    "([;{}])[ ]*validation_runtime->observe_snapshot\\([ ]*current[ ]*\\)[ ]*;"
    3)
assert_direct_exact_call_count("all Stage17 inventory facade calls"
    "${_host_runtime_normalized}"
    "observe_inventory\\("
    "([;{}])[ ]*validation_runtime->observe_inventory\\([ ]*inventory,[ ]*current[ ]*\\)[ ]*;"
    1)
assert_direct_exact_call_count("all Stage17 submitted-action facade calls"
    "${_host_runtime_normalized}"
    "observe_submitted_actions\\("
    "([;{}])[ ]*validation_runtime->observe_submitted_actions\\([ ]*submitted_actions[ ]*\\)[ ]*;"
    1)
assert_direct_exact_call_count("all Stage17 draw observer calls"
    "${_host_runtime_normalized}"
    "observe_stage17_draw_runtime\\("
    "([;{}])[ ]*observe_stage17_draw_runtime\\([ ]*config,[ ]*[*]stage17_validation_state,[ ]*presented_snapshot,[ ]*renderer[.]active_skill_draw_status\\([ ]*\\)[ ]*\\)[ ]*;"
    1)
assert_direct_exact_call_count("all drain_events validation runtime arguments"
    "${_host_runtime_normalized}"
    "drain_events\\("
    "([;{}])[ ]*drain_events\\([ ]*[*](runtime[.]session\\([ ]*\\)|session),[ ]*renderer,[ ]*feedback,[ ]*audio,[ ]*validation_runtime[.]get\\([ ]*\\)[ ]*\\)[ ]*;"
    6)
assert_token_depth_sequence("Stage17 snapshot observer"
    "${_host_runtime}" "validation_runtime->observe_snapshot(" 3 3 4)
assert_token_depth_sequence("Stage17 inventory observer"
    "${_host_runtime}" "validation_runtime->observe_inventory(" 3)
assert_token_depth_sequence("Stage17 submitted-actions observer"
    "${_host_runtime}" "validation_runtime->observe_submitted_actions(" 4)
assert_token_depth_sequence("Stage17 draw observer"
    "${_host_runtime}" "observe_stage17_draw_runtime(" 3)
assert_token_depth_sequence("drain_events call"
    "${_host_runtime}" "drain_events(" 3 4 5 4 5 4)

extract_unique_direct_cpp_block("forward-actions block" "${_host_loop}"
    "if (forward_actions)" 1 _forward_actions_block _forward_actions_begin)
extract_unique_direct_cpp_block("fixed-step loop" "${_host_loop}"
    "for (std::uint32_t step = 0; step < frame.steps; ++step)" 1
    _stage17_fixed_step_loop _stage17_fixed_step_begin)
extract_unique_direct_cpp_block("ground-loot renderer draw lambda"
    "${_host_loop}"
    "const GroundLootView ground_loot_view = [&]() noexcept" 1
    _ground_loot_draw_block _ground_loot_draw_begin)

assert_unique_ordered_host_tokens("Stage17 direct loop observer"
    "${_host_loop}" 1
    "validation_runtime->observe_inventory("
    "observe_stage17_draw_runtime(")
assert_unique_ordered_host_tokens("Stage17 submitted-actions observer"
    "${_forward_actions_block}" 1
    "validation_runtime->observe_submitted_actions(")
assert_unique_ordered_host_tokens("Stage17 fixed-step snapshot observer"
    "${_stage17_fixed_step_loop}" 1
    "validation_runtime->observe_snapshot(")
assert_unique_ordered_host_tokens("real renderer draw return"
    "${_ground_loot_draw_block}" 1
    "return renderer.draw(")

string(REGEX REPLACE "[ \t\r\n]+" " " _forward_actions_normalized
    "${_forward_actions_block}")
string(REGEX REPLACE "[ \t\r\n]+" " " _stage17_fixed_step_normalized
    "${_stage17_fixed_step_loop}")
string(REGEX REPLACE "[ \t\r\n]+" " " _ground_loot_draw_normalized
    "${_ground_loot_draw_block}")
assert_one_normalized_match("Stage17 inventory observer arguments"
    "${_host_loop_normalized}"
    "validation_runtime->observe_inventory\\([ ]*inventory,[ ]*current[ ]*\\)")
assert_one_normalized_match("Stage17 submitted-actions observer arguments"
    "${_forward_actions_normalized}"
    "validation_runtime->observe_submitted_actions\\([ ]*submitted_actions[ ]*\\)")
assert_one_normalized_match("Stage17 fixed-step snapshot observer arguments"
    "${_stage17_fixed_step_normalized}"
    "validation_runtime->observe_snapshot\\([ ]*current[ ]*\\)")
assert_one_normalized_match("Stage17 draw observer arguments"
    "${_host_loop_normalized}"
    "observe_stage17_draw_runtime\\([ ]*config,[ ]*[*]stage17_validation_state,[ ]*presented_snapshot,[ ]*renderer[.]active_skill_draw_status\\([ ]*\\)[ ]*\\)")
assert_one_normalized_match("submitted-actions producer-to-observer binding"
    "${_forward_actions_normalized}"
    "const[ ]+SubmittedFrameActions[ ]+submitted_actions[ ]*=[ ]*submit_frame_actions\\([ ]*[*]session,[ ]*frame_input[ ]*\\)[ ]*;[ ]*validation_runtime->observe_submitted_actions\\([ ]*submitted_actions[ ]*\\)[ ]*;")
assert_one_normalized_match("fixed-step snapshot producer-to-observer binding"
    "${_stage17_fixed_step_normalized}"
    "session->snapshot\\([ ]*current[ ]*\\)[ ]*;[ ]*validation_runtime->observe_snapshot\\([ ]*current[ ]*\\)[ ]*;")
assert_direct_exact_call_count("real renderer draw return statement"
    "${_ground_loot_draw_normalized}"
    "renderer[.]draw\\("
    "([;{}])[ ]*return[ ]+renderer[.]draw\\([ ]*previous,[ ]*presented_snapshot,[ ]*runtime[.]render_status\\([ ]*\\),[ ]*static_cast<float>\\([ ]*frame[.]interpolation_alpha[ ]*\\),[ ]*draw_debug,[ ]*feedback,[ ]*audio_ready[ ]*\\)[ ]*;"
    1)
assert_one_normalized_match("renderer draw completion-to-observer binding"
    "${_host_loop_normalized}"
    "feedback,[ ]*audio_ready[ ]*\\)[ ]*;[ ]*\\}[ ]*\\([ ]*\\)[ ]*;[ ]*observe_stage17_draw_runtime\\([ ]*config,[ ]*[*]stage17_validation_state,[ ]*presented_snapshot,[ ]*renderer[.]active_skill_draw_status\\([ ]*\\)[ ]*\\)[ ]*;")

string(FIND "${_host_loop}" "validation_runtime->observe_inventory("
    _stage17_inventory_position)
string(FIND "${_host_loop}" "validation_runtime->observe_snapshot("
    _stage17_loop_top_snapshot_position)
if(_stage17_loop_top_snapshot_position EQUAL -1)
    message(FATAL_ERROR
        "Host validation sequence guard cannot isolate loop-top Stage17 snapshot")
endif()
math(EXPR _stage17_loop_top_after
    "${_stage17_loop_top_snapshot_position} + 1")
string(SUBSTRING "${_host_loop}" ${_stage17_loop_top_after} -1
    _stage17_snapshot_remainder)
string(FIND "${_stage17_snapshot_remainder}" "validation_runtime->observe_snapshot("
    _stage17_second_snapshot_relative)
if(_stage17_second_snapshot_relative EQUAL -1)
    message(FATAL_ERROR
        "Host validation sequence guard cannot isolate both loop Stage17 snapshots")
endif()
math(EXPR _stage17_second_snapshot_position
    "${_stage17_loop_top_after} + ${_stage17_second_snapshot_relative}")
math(EXPR _stage17_second_snapshot_after
    "${_stage17_second_snapshot_position} + 1")
string(SUBSTRING "${_host_loop}" ${_stage17_second_snapshot_after} -1
    _stage17_after_second_snapshot)
string(FIND "${_stage17_after_second_snapshot}" "validation_runtime->observe_snapshot("
    _stage17_extra_snapshot)
if(NOT _stage17_extra_snapshot EQUAL -1)
    message(FATAL_ERROR
        "Host validation sequence guard found an extra loop Stage17 snapshot")
endif()
cpp_token_brace_depth("${_host_loop}"
    ${_stage17_loop_top_snapshot_position} _stage17_loop_top_snapshot_depth)
if(NOT _stage17_loop_top_snapshot_depth EQUAL 1)
    message(FATAL_ERROR
        "Host validation sequence guard rejected loop-top Stage17 snapshot scope")
endif()
string(FIND "${_forward_actions_block}"
    "validation_runtime->observe_submitted_actions("
    _stage17_submitted_relative)
math(EXPR _stage17_submitted_position
    "${_forward_actions_begin} + ${_stage17_submitted_relative}")
string(FIND "${_stage17_fixed_step_loop}"
    "validation_runtime->observe_snapshot("
    _stage17_snapshot_relative)
math(EXPR _stage17_snapshot_position
    "${_stage17_fixed_step_begin} + ${_stage17_snapshot_relative}")
string(FIND "${_host_loop}" "observe_stage17_draw_runtime("
    _stage17_draw_position)
if(NOT _stage17_second_snapshot_position EQUAL _stage17_snapshot_position)
    message(FATAL_ERROR
        "Host validation sequence guard rejected fixed-step Stage17 snapshot identity")
endif()
string(LENGTH "${_loop_session_block}" _loop_session_block_length)
math(EXPR _loop_session_end
    "${_loop_session_begin} + ${_loop_session_block_length}")
string(FIND "${_host_loop}"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    _sampled_physical_keys_position)
if(NOT _loop_session_end LESS _stage17_loop_top_snapshot_position
        OR NOT _stage17_loop_top_snapshot_position LESS _sampled_physical_keys_position)
    message(FATAL_ERROR
        "Host validation sequence guard rejected loop-top Stage17 snapshot position")
endif()
if(NOT _stage17_loop_top_snapshot_position LESS _stage17_inventory_position
        OR NOT _stage17_inventory_position LESS _stage17_submitted_position
        OR NOT _stage17_submitted_position LESS _stage17_snapshot_position
        OR NOT _stage17_snapshot_position LESS _stage17_draw_position)
    message(FATAL_ERROR
        "Host validation sequence guard rejected Stage17 observer order")
endif()

if(NOT DEFINED HOST_OVERRIDE)
    assert_stage17_capture_lifecycle("${_host_text}" "production")
    assert_stage17_lifecycle_memory_mutations("${_host_text}")
endif()

# run_raylib_host owns the frame loop and summary calls inside its single
# top-level try block, so direct execution statements are at brace depth two
# relative to the complete function block (function body plus try body).
assert_unique_ordered_host_tokens("validation summary write" "${_host_runtime}" 2
    "write_stage11b_validation_summary("
    "write_stage11c_hud_validation_summary("
    "write_stage11d_loot_validation_summary("
    "write_stage17_validation_summary(")
foreach(_summary_token IN ITEMS
        "write_stage11b_validation_summary("
        "write_stage11c_hud_validation_summary("
        "write_stage11d_loot_validation_summary("
        "write_stage17_validation_summary(")
    string(FIND "${_host_runtime}" "${_summary_token}" _summary_position)
    if(NOT _summary_position GREATER _loop_end)
        message(FATAL_ERROR
            "Host validation sequence guard rejected summary write before loop end: ${_summary_token}")
    endif()
endforeach()
