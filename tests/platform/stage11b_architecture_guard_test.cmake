if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

function(arpg_assert_files_exclude REASON REGEX)
    foreach(_file IN LISTS ARGN)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_file}")
        endif()
        file(READ "${_file}" _source)
        if(_source MATCHES "${REGEX}")
            message(FATAL_ERROR "${REASON}: ${_file}")
        endif()
    endforeach()
endfunction()

function(arpg_assert_files_exclude_case_insensitive REASON REGEX)
    string(TOLOWER "${REGEX}" _regex)
    foreach(_file IN LISTS ARGN)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_file}")
        endif()
        file(READ "${_file}" _source)
        string(TOLOWER "${_source}" _source_lower)
        if(_source_lower MATCHES "${_regex}")
            message(FATAL_ERROR "${REASON}: ${_file}")
        endif()
    endforeach()
endfunction()

function(arpg_assert_default_bindings_unique SOURCE_FILE)
    file(READ "${SOURCE_FILE}" _source)
    string(FIND "${_source}" "default_bindings{" _start)
    if(_start EQUAL -1)
        message(FATAL_ERROR "settings default bindings duplicate stable key: default bindings are missing")
    endif()
    string(SUBSTRING "${_source}" ${_start} -1 _tail)
    string(FIND "${_tail}" "};" _end)
    if(_end EQUAL -1)
        message(FATAL_ERROR "settings default bindings duplicate stable key: default bindings are unterminated")
    endif()
    string(SUBSTRING "${_tail}" 0 ${_end} _bindings)
    string(REGEX MATCHALL "StableKey::[A-Za-z0-9_]+" _keys "${_bindings}")
    foreach(_key IN LISTS _keys)
        list(FIND _seen "${_key}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "settings default bindings duplicate stable key: ${_key}")
        endif()
        list(APPEND _seen "${_key}")
    endforeach()
endfunction()

set(_settings_types "${SOURCE_ROOT}/platform/settings/settings_types.cpp")
set(_settings_store "${SOURCE_ROOT}/platform/settings/settings_store.cpp")
set(_host_source "${SOURCE_ROOT}/platform/raylib/raylib_host.cpp")
set(_runtime_source "${SOURCE_ROOT}/platform/raylib/host_validation_runtime.cpp")
set(_stage_header "${SOURCE_ROOT}/platform/raylib/host_validation_stage11b.hpp")
set(_stage_source "${SOURCE_ROOT}/platform/raylib/host_validation_stage11b.cpp")
set(_hud_source "${SOURCE_ROOT}/platform/raylib/hud_renderer.cpp")
if(DEFINED STAGE11B_GUARD_MUTATION_KIND)
    if(STAGE11B_GUARD_MUTATION_KIND STREQUAL "raylib")
        set(_settings_types "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "save_name")
        set(_settings_store "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "key")
        set(_hud_source "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "duplicate")
        set(_settings_types "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "stage")
        set(_stage_source "${STAGE11B_GUARD_MUTATION_FILE}")
    endif()
endif()

foreach(_stage_required IN ITEMS "${_stage_header}" "${_stage_source}")
    if(NOT EXISTS "${_stage_required}")
        message(FATAL_ERROR "Stage 11B validation target is missing: ${_stage_required}")
    endif()
endforeach()
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_stage_source}" _stage_source_text)
file(READ "${_host_source}" _host_source_text)
file(READ "${_runtime_source}" _runtime_source_text)
file(READ "${SOURCE_ROOT}/platform/raylib/CMakeLists.txt" _raylib_cmake_text)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "PhysicalKeySnapshot inject_stage11b_physical_edges(" _stage11b_injection_block)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "bool stage11b_validation_complete(" _stage11b_complete_block)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "std::uint64_t stage11b_snapshot_hash(" _stage11b_hash_block)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "void write_stage11b_validation_summary(" _stage11b_summary_block)

function(arpg_assert_stage11b_block_tokens LABEL BLOCK)
    foreach(_token IN ITEMS ${ARGN})
        string(FIND "${BLOCK}" "${_token}" _token_found)
        if(_token_found EQUAL -1)
            message(FATAL_ERROR "Stage 11B ${LABEL} token is missing: ${_token}")
        endif()
    endforeach()
endfunction()

function(stage11b_arch_assert_direct_token LABEL BLOCK TOKEN)
    stage11b_arch_count_token("${BLOCK}" "${TOKEN}" token_count)
    string(FIND "${BLOCK}" "${TOKEN}" token_position)
    if(NOT token_count EQUAL 1)
        message(FATAL_ERROR
            "Stage 11B ${LABEL} direct token is missing or duplicated: ${TOKEN}")
    endif()
    stage11b_arch_brace_depth("${BLOCK}" ${token_position} token_depth)
    if(NOT token_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage 11B ${LABEL} token is outside direct method scope: ${TOKEN}")
    endif()
endfunction()

function(stage11b_arch_count_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${TOKEN}" token_length)
    string(LENGTH "${SOURCE}" before_length)
    string(REPLACE "${TOKEN}" "" without_token "${SOURCE}")
    string(LENGTH "${without_token}" after_length)
    math(EXPR token_count
        "(${before_length} - ${after_length}) / ${token_length}")
    set(${OUT_COUNT} ${token_count} PARENT_SCOPE)
endfunction()

function(stage11b_arch_brace_depth SOURCE POSITION OUT_DEPTH)
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

function(stage11b_arch_unconditional_cpp_surface
        SOURCE OUT_ACTIVE OUT_LEXICAL)
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
                    "Stage11B architecture conditional surface is unbalanced")
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
            "Stage11B architecture conditional surface is unbalanced")
    endif()
    set(${OUT_ACTIVE} "${active}" PARENT_SCOPE)
    set(${OUT_LEXICAL} "${lexical}" PARENT_SCOPE)
endfunction()

function(stage11b_arch_find_scope_end SOURCE OPEN_INDEX OUT_END OUT_VALID)
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

function(stage11b_arch_mask_non_direct SOURCE OUT_SOURCE)
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
            stage11b_arch_find_scope_end(
                "${SOURCE}" ${open_index} scope_end scope_valid)
            if(NOT scope_valid)
                message(FATAL_ERROR
                    "Stage11B architecture direct scope is unterminated")
            endif()
        endif()
        math(EXPR copy_cursor "${scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${masked}" PARENT_SCOPE)
endfunction()

function(stage11b_arch_extract_unique_owner
        ACTIVE LEXICAL SIGNATURE LABEL OUT_BLOCK)
    stage11b_arch_count_token("${ACTIVE}" "${SIGNATURE}" active_count)
    stage11b_arch_count_token("${LEXICAL}" "${SIGNATURE}" lexical_count)
    if(NOT active_count EQUAL 1 OR NOT lexical_count EQUAL 1)
        message(FATAL_ERROR
            "Stage 11B ${LABEL} must have one active and lexical definition")
    endif()
    foreach(owner_surface IN ITEMS ACTIVE LEXICAL)
        string(FIND "${${owner_surface}}" "${SIGNATURE}" owner_position)
        stage11b_arch_brace_depth("${${owner_surface}}" ${owner_position}
            owner_depth)
        if(NOT owner_depth EQUAL 1)
            message(FATAL_ERROR
                "Stage 11B ${LABEL} must be namespace-level")
        endif()
    endforeach()
    evidence_extract_cpp_function_block("${ACTIVE}" "${SIGNATURE}"
        owner_block)
    stage11b_arch_mask_non_direct("${owner_block}" owner_direct)
    set(${OUT_BLOCK} "${owner_direct}" PARENT_SCOPE)
endfunction()

stage11b_arch_unconditional_cpp_surface("${_host_source_text}"
    _host_active _host_lexical)
stage11b_arch_unconditional_cpp_surface("${_runtime_source_text}"
    _runtime_active _runtime_lexical)
stage11b_arch_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_pause_transition("
    "pause-transition owner" _stage11b_pause_transition_block)
stage11b_arch_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "PresentationDecision HostValidationRuntime::observe_presented_frame("
    "presented-frame owner" _stage11b_presented_block)
stage11b_arch_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::observe_capture_result("
    "capture-result owner" _stage11b_capture_result_block)
stage11b_arch_extract_unique_owner("${_runtime_active}" "${_runtime_lexical}"
    "void HostValidationRuntime::write_summaries("
    "summary owner" _stage11b_runtime_summary_block)
stage11b_arch_extract_unique_owner("${_host_active}" "${_host_lexical}"
    "HostExitCode run_raylib_host("
    "Host run owner" _stage11b_host_run_block)

foreach(_stage_definition IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary(")
    string(FIND "${_stage_source_text}" "${_stage_definition}" _stage_found)
    if(_stage_found EQUAL -1)
        message(FATAL_ERROR "Stage 11B validation definition is missing: ${_stage_definition}")
    endif()
    string(FIND "${_host_source_text}" "${_stage_definition}" _host_found)
    if(NOT _host_found EQUAL -1)
        message(FATAL_ERROR "Stage 11B validation definition remains in raylib_host.cpp: ${_stage_definition}")
    endif()
endforeach()
if(NOT _stage_header_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Stage 11B validation state definition is missing")
endif()
if(_host_source_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Stage 11B validation state definition remains in raylib_host.cpp")
endif()
arpg_assert_stage11b_block_tokens("injection" "${_stage11b_injection_block}"
    "StableKey::j" "StableKey::u")
arpg_assert_stage11b_block_tokens("completion" "${_stage11b_complete_block}"
    "state.pause_capture_while_paused")
arpg_assert_stage11b_block_tokens("snapshot hash" "${_stage11b_hash_block}"
    "mix(snapshot.depth)" "mix(snapshot.room_index)")
arpg_assert_stage11b_block_tokens("summary" "${_stage11b_summary_block}"
    "state.player_monster_hash_before <<" "state.load_status")
arpg_assert_stage11b_block_tokens("pause transition owner"
    "${_stage11b_pause_transition_block}"
    "Stage11BValidationScenario::paused_freeze"
    "impl_->states.stage11b.resume_input_injected"
    "impl_->states.stage11b.resume_ticks_before ="
    "impl_->states.stage11b.fixed_ticks")
arpg_assert_stage11b_block_tokens("presented-frame owner"
    "${_stage11b_presented_block}"
    "impl_->states.stage11b.resume_ticks_after ="
    "impl_->states.stage11b.paused_presented"
    "host_validation::stage11b_snapshot_hash(snapshot)"
    "host_validation::stage11b_validation_complete("
    "stage11b_paused_visible_capture")
arpg_assert_stage11b_block_tokens("capture-result owner"
    "${_stage11b_capture_result_block}"
    "CaptureOwner::generic_validation"
    "pause_capture_while_paused =")
arpg_assert_stage11b_block_tokens("runtime summary owner"
    "${_stage11b_runtime_summary_block}"
    "host_validation::write_stage11b_validation_summary(")
stage11b_arch_assert_direct_token("pause transition"
    "${_stage11b_pause_transition_block}"
    "if (impl_->config->stage11b_validation")
foreach(_stage11b_presented_direct_token IN ITEMS
        "const bool stage11b_reached ="
        "impl_->pending_stage11b_paused_visible_capture ="
        "decision.validation_complete ="
        "return decision;")
    stage11b_arch_assert_direct_token("presented frame"
        "${_stage11b_presented_block}"
        "${_stage11b_presented_direct_token}")
endforeach()
stage11b_arch_assert_direct_token("capture result"
    "${_stage11b_capture_result_block}"
    "if (owner != CaptureOwner::generic_validation)")
stage11b_arch_assert_direct_token("capture result"
    "${_stage11b_capture_result_block}"
    "impl_->states.stage11b.pause_capture_while_paused =")
stage11b_arch_assert_direct_token("summary"
    "${_stage11b_runtime_summary_block}"
    "host_validation::write_stage11b_validation_summary(")
foreach(_removed_host_stage11b IN ITEMS
        "Stage11BValidationState"
        "stage11b_validation_state"
        "HostValidationStateAccess::stage11b("
        "host_validation::write_stage11b_validation_summary(")
    string(FIND "${_host_source_text}" "${_removed_host_stage11b}"
        _removed_host_stage11b_index)
    if(NOT _removed_host_stage11b_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11B Host still owns validation state or summary: ${_removed_host_stage11b}")
    endif()
endforeach()
foreach(_required_host_stage11b_facade IN ITEMS
        "validation_runtime->observe_pause_transition("
        "validation_runtime->observe_presented_frame("
        "validation_runtime->observe_capture_result("
        "validation_runtime->write_summaries(")
    foreach(_host_surface IN ITEMS _host_active _host_lexical)
        stage11b_arch_count_token("${${_host_surface}}"
            "${_required_host_stage11b_facade}"
            _required_host_stage11b_facade_count)
        if(NOT _required_host_stage11b_facade_count EQUAL 1)
            message(FATAL_ERROR
                "Stage 11B Host facade seam must be active and lexical unique: ${_required_host_stage11b_facade}")
        endif()
    endforeach()
    stage11b_arch_count_token("${_stage11b_host_run_block}"
        "${_required_host_stage11b_facade}"
        _required_host_stage11b_run_count)
    if(NOT _required_host_stage11b_run_count EQUAL 1)
        message(FATAL_ERROR
            "Stage 11B Host facade seam is outside the direct run owner: ${_required_host_stage11b_facade}")
    endif()
endforeach()
string(FIND "${_raylib_cmake_text}" "host_validation_stage11b.cpp" _stage_registered)
if(_stage_registered EQUAL -1)
    message(FATAL_ERROR "arpg_raylib does not register host_validation_stage11b.cpp")
endif()

file(GLOB_RECURSE _settings_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/platform/settings/*.h" "${SOURCE_ROOT}/platform/settings/*.hpp"
    "${SOURCE_ROOT}/platform/settings/*.cpp")
list(REMOVE_ITEM _settings_sources "${SOURCE_ROOT}/platform/settings/settings_types.cpp"
             "${SOURCE_ROOT}/platform/settings/settings_store.cpp")
list(APPEND _settings_sources "${_settings_types}" "${_settings_store}")

arpg_assert_files_exclude_case_insensitive("settings must not include raylib"
    "#[ \t]*include[ \t]*[<\"](raylib|raymath|rlgl)([./\\]|[>\"])" ${_settings_sources})
arpg_assert_files_exclude_case_insensitive("settings must not include gameplay module"
    "#[ \t]*include[ \t]*[<\"](combat|dungeon|items|progression|passives)[/\\]" ${_settings_sources})

file(GLOB_RECURSE _core_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/core/*.h" "${SOURCE_ROOT}/core/*.hpp" "${SOURCE_ROOT}/core/*.cpp")
arpg_assert_files_exclude_case_insensitive("core must not include settings"
    "#[ \t]*include[ \t]*[<\"](platform[/\\])?settings[/\\]" ${_core_sources})

arpg_assert_files_exclude_case_insensitive("settings store must not use gameplay save filename"
    "[A-Za-z0-9_-]+[.]sav" "${_settings_store}")
arpg_assert_files_exclude("host HUD must not use direct gameplay KEY constants"
    "(^|[^A-Za-z0-9_])KEY_(A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z|ZERO|ONE|TWO|THREE|FOUR|FIVE|SIX|SEVEN|EIGHT|NINE|UP|DOWN|LEFT|RIGHT|SPACE|LEFT_SHIFT|RIGHT_SHIFT|LEFT_CONTROL|RIGHT_CONTROL)([^A-Za-z0-9_]|$)" "${_host_source}" "${_hud_source}")
arpg_assert_default_bindings_unique("${_settings_types}")

if(NOT DEFINED STAGE11B_GUARD_MUTATION_MODE)
    if(NOT DEFINED GUARD_TEST_ROOT)
        message(FATAL_ERROR "GUARD_TEST_ROOT is required for mutation self-checks")
    endif()
    file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

    function(arpg_expect_guard_rejects NAME KIND SOURCE MUTATION REASON)
        set(_fixture "${GUARD_TEST_ROOT}/${NAME}.cpp")
        file(COPY_FILE "${SOURCE}" "${_fixture}")
        if(KIND STREQUAL "duplicate")
            file(READ "${_fixture}" _mutated)
            string(REPLACE "StableKey::p};" "StableKey::w};" _mutated "${_mutated}")
            file(WRITE "${_fixture}" "${_mutated}")
        else()
            file(APPEND "${_fixture}" "\n${MUTATION}\n")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${SOURCE_ROOT}"
                "-DSTAGE11B_GUARD_MUTATION_MODE=1"
                "-DSTAGE11B_GUARD_MUTATION_KIND=${KIND}"
                "-DSTAGE11B_GUARD_MUTATION_FILE=${_fixture}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
        if(_result EQUAL 0)
            message(FATAL_ERROR "Stage 11b guard missed ${NAME} mutation")
        endif()
        set(_output "${_stdout}\n${_stderr}")
        if(NOT _output MATCHES "${REASON}")
            message(FATAL_ERROR "Stage 11b guard mutation ${NAME} lacked named reason '${REASON}'")
        endif()
    endfunction()

    arpg_expect_guard_rejects(raylib_include raylib
        "${SOURCE_ROOT}/platform/settings/settings_types.cpp"
        "#include <raylib.h>"
        "settings must not include raylib")
    arpg_expect_guard_rejects(gameplay_save_name save_name
        "${SOURCE_ROOT}/platform/settings/settings_store.cpp"
        "constexpr const char* kBad = \"slot-a.sav\";"
        "settings store must not use gameplay save filename")
    arpg_expect_guard_rejects(direct_gameplay_key key
        "${SOURCE_ROOT}/platform/raylib/hud_renderer.cpp"
        "int kBad = KEY_J;"
        "host HUD must not use direct gameplay KEY constants")
    arpg_expect_guard_rejects(duplicate_stable_key duplicate
        "${SOURCE_ROOT}/platform/settings/settings_types.cpp"
        ""
        "settings default bindings duplicate stable key")

    set(_stage_mutation "${GUARD_TEST_ROOT}/stage11b-rebound-key.cpp")
    file(COPY_FILE "${_stage_source}" "${_stage_mutation}")
    file(READ "${_stage_mutation}" _stage_mutation_text)
    string(REPLACE "settings::StableKey::j" "settings::StableKey::q"
        _stage_mutation_text "${_stage_mutation_text}")
    string(APPEND _stage_mutation_text
        "\n// decoy settings::StableKey::j\\\n")
    file(WRITE "${_stage_mutation}" "${_stage_mutation_text}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DSTAGE11B_GUARD_MUTATION_MODE=1"
            "-DSTAGE11B_GUARD_MUTATION_KIND=stage"
            "-DSTAGE11B_GUARD_MUTATION_FILE=${_stage_mutation}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE _stage_result OUTPUT_VARIABLE _stage_stdout ERROR_VARIABLE _stage_stderr)
    if(_stage_result EQUAL 0)
        message(FATAL_ERROR "Stage 11b guard missed stage rebound-key mutation")
    endif()
    if(NOT "${_stage_stdout}${_stage_stderr}" MATCHES
            "Stage 11B injection token is missing: StableKey::j")
        message(FATAL_ERROR "Stage 11b stage rebound-key mutation lacked the expected reason")
    endif()
endif()

message(STATUS "Stage 11b settings architecture boundaries verified")
