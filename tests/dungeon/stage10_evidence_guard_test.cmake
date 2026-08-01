include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")

function(stage10_count_token SOURCE TOKEN OUT_COUNT)
    set(tail "${SOURCE}")
    set(count 0)
    while(TRUE)
        string(FIND "${tail}" "${TOKEN}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR after "${position} + 1")
        string(SUBSTRING "${tail}" ${after} -1 tail)
        math(EXPR count "${count} + 1")
    endwhile()
    set(${OUT_COUNT} ${count} PARENT_SCOPE)
endfunction()

function(stage10_require_count LABEL SOURCE TOKEN EXPECTED)
    stage10_count_token("${SOURCE}" "${TOKEN}" actual)
    if(NOT actual EQUAL EXPECTED)
        message(FATAL_ERROR
            "Stage 10 ${LABEL}: expected ${EXPECTED}, found ${actual}: ${TOKEN}")
    endif()
endfunction()

function(stage10_fold_phase2_splices SOURCE OUT_SOURCE)
    string(ASCII 92 backslash)
    string(ASCII 13 carriage_return)
    string(ASCII 10 line_feed)
    set(folded "${SOURCE}")
    string(REPLACE "${backslash}${carriage_return}${line_feed}" ""
        folded "${folded}")
    string(REPLACE "${backslash}${line_feed}" "" folded "${folded}")
    set(${OUT_SOURCE} "${folded}" PARENT_SCOPE)
endfunction()

function(stage10_mask_cpp_non_newlines SOURCE OUT_MASKED)
    string(REGEX REPLACE "[^\r\n]" " " masked "${SOURCE}")
    set(${OUT_MASKED} "${masked}" PARENT_SCOPE)
endfunction()

# Chunk-oriented lexer used here instead of a character-by-character Host
# walk. Input is already phase-2 folded. It preserves code/braces and line
# endings while masking comments plus normal, character and raw literals.
function(stage10_sanitize_folded_cpp SOURCE OUT_LEXICAL)
    set(remaining "${SOURCE}")
    set(lexical "")
    while(NOT remaining STREQUAL "")
        set(next -1)
        set(kind "")
        foreach(marker IN ITEMS "//" "/*" "R\"" "\"" "'")
            string(FIND "${remaining}" "${marker}" position)
            if(NOT position EQUAL -1
                    AND (next EQUAL -1 OR position LESS next))
                set(next ${position})
                set(kind "${marker}")
            endif()
        endforeach()
        if(next EQUAL -1)
            string(APPEND lexical "${remaining}")
            break()
        endif()
        if(next GREATER 0)
            string(SUBSTRING "${remaining}" 0 ${next} prefix)
            string(APPEND lexical "${prefix}")
        endif()
        string(SUBSTRING "${remaining}" ${next} -1 marked)

        if(kind STREQUAL "//")
            string(FIND "${marked}" "\n" close)
            if(close EQUAL -1)
                stage10_mask_cpp_non_newlines("${marked}" masked)
                string(APPEND lexical "${masked}")
                break()
            endif()
            math(EXPR segment_length "${close} + 1")
        elseif(kind STREQUAL "/*")
            string(FIND "${marked}" "*/" close)
            if(close EQUAL -1)
                message(FATAL_ERROR "Stage 10 C++ block comment is unterminated")
            endif()
            math(EXPR segment_length "${close} + 2")
        elseif(kind STREQUAL "R\"")
            string(FIND "${marked}" "(" raw_open)
            if(raw_open EQUAL -1 OR raw_open GREATER 18)
                string(APPEND lexical "R")
                string(SUBSTRING "${marked}" 1 -1 remaining)
                continue()
            endif()
            math(EXPR delimiter_length "${raw_open} - 2")
            string(SUBSTRING "${marked}" 2 ${delimiter_length} delimiter)
            set(raw_close ")${delimiter}\"")
            string(FIND "${marked}" "${raw_close}" close)
            if(close EQUAL -1)
                message(FATAL_ERROR "Stage 10 C++ raw string is unterminated")
            endif()
            string(LENGTH "${raw_close}" raw_close_length)
            math(EXPR segment_length "${close} + ${raw_close_length}")
        else()
            set(quote "${kind}")
            set(scan 1)
            set(segment_length -1)
            string(LENGTH "${marked}" marked_length)
            while(scan LESS marked_length)
                string(SUBSTRING "${marked}" ${scan} -1 literal_tail)
                string(FIND "${literal_tail}" "\\" escape)
                string(FIND "${literal_tail}" "${quote}" close)
                if(close EQUAL -1)
                    message(FATAL_ERROR "Stage 10 C++ literal is unterminated")
                endif()
                if(NOT escape EQUAL -1 AND escape LESS close)
                    math(EXPR scan "${scan} + ${escape} + 2")
                else()
                    math(EXPR segment_length "${scan} + ${close} + 1")
                    break()
                endif()
            endwhile()
            if(segment_length EQUAL -1)
                message(FATAL_ERROR "Stage 10 C++ literal is unterminated")
            endif()
        endif()

        string(SUBSTRING "${marked}" 0 ${segment_length} segment)
        stage10_mask_cpp_non_newlines("${segment}" masked)
        string(APPEND lexical "${masked}")
        string(LENGTH "${marked}" marked_length)
        if(segment_length EQUAL marked_length)
            set(remaining "")
        else()
            string(SUBSTRING "${marked}" ${segment_length} -1 remaining)
        endif()
    endwhile()
    set(${OUT_LEXICAL} "${lexical}" PARENT_SCOPE)
endfunction()

# Fold phase-2 line splices and remove comments/literals first, then mask every
# conditional-preprocessor region. The lexical output retains inactive code so
# an exact owner cannot be borrowed from #if 0 or duplicated there.
function(stage10_unconditional_cpp_surface SOURCE OUT_ACTIVE OUT_LEXICAL)
    stage10_fold_phase2_splices("${SOURCE}" phase2_source)
    stage10_sanitize_folded_cpp("${phase2_source}" lexical)
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
                    "Stage 10 conditional owner surface is unbalanced")
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
            "Stage 10 conditional owner surface is unbalanced")
    endif()
    set(${OUT_ACTIVE} "${active}" PARENT_SCOPE)
    set(${OUT_LEXICAL} "${lexical}" PARENT_SCOPE)
endfunction()

function(stage10_extract_sanitized_block SOURCE SIGNATURE OUT_BLOCK)
    evidence_find_cpp_function_bounds_in_sanitized(
        "${SOURCE}" "${SIGNATURE}" begin open end)
    math(EXPR length "${end} - ${begin} + 1")
    string(SUBSTRING "${SOURCE}" ${begin} ${length} block)
    set(${OUT_BLOCK} "${block}" PARENT_SCOPE)
endfunction()

function(stage10_find_balanced_scope_end SOURCE OPEN_INDEX OUT_END OUT_VALID)
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

# Remove uncalled lambdas and compile-time-dead scopes before sequence scans.
# Comments/literals and inactive preprocessor regions were already masked by
# stage10_unconditional_cpp_surface().
function(stage10_mask_non_direct_scopes SOURCE OUT_SOURCE)
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
            stage10_find_balanced_scope_end(
                "${SOURCE}" ${open_index} scope_end scope_valid)
            if(NOT scope_valid)
                message(FATAL_ERROR
                    "Stage 10 direct-scope fixture has no closing brace")
            endif()
        endif()
        math(EXPR copy_cursor "${scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${masked}" PARENT_SCOPE)
endfunction()

function(stage10_normalize SOURCE OUT_NORMALIZED)
    string(REGEX REPLACE "[ \t\r\n]+" " " normalized "${SOURCE}")
    string(STRIP "${normalized}" normalized)
    set(${OUT_NORMALIZED} "${normalized}" PARENT_SCOPE)
endfunction()

function(stage10_brace_depth SOURCE POSITION OUT_DEPTH)
    string(SUBSTRING "${SOURCE}" 0 ${POSITION} prefix)
    string(REGEX REPLACE "[^{}]" "" braces "${prefix}")
    string(LENGTH "${braces}" length)
    set(depth 0)
    if(length GREATER 0)
        math(EXPR last "${length} - 1")
        foreach(index RANGE 0 ${last})
            string(SUBSTRING "${braces}" ${index} 1 brace)
            if(brace STREQUAL "{")
                math(EXPR depth "${depth} + 1")
            else()
                math(EXPR depth "${depth} - 1")
            endif()
        endforeach()
    endif()
    set(${OUT_DEPTH} ${depth} PARENT_SCOPE)
endfunction()

function(stage10_require_depth LABEL SOURCE TOKEN EXPECTED)
    stage10_require_count("${LABEL} uniqueness" "${SOURCE}" "${TOKEN}" 1)
    string(FIND "${SOURCE}" "${TOKEN}" position)
    stage10_brace_depth("${SOURCE}" ${position} depth)
    if(NOT depth EQUAL EXPECTED)
        message(FATAL_ERROR
            "Stage 10 ${LABEL} has wrong scope: expected=${EXPECTED}, actual=${depth}")
    endif()
endfunction()

function(stage10_require_active_exact_definition
        LABEL ACTIVE LEXICAL SIGNATURE EXPECTED)
    stage10_count_token("${ACTIVE}" "${SIGNATURE}" active_count)
    stage10_count_token("${LEXICAL}" "${SIGNATURE}" lexical_count)
    if(NOT active_count EQUAL 1 OR NOT lexical_count EQUAL 1)
        message(FATAL_ERROR
            "Stage 10 ${LABEL} is not one unique active and lexical definition")
    endif()
    string(FIND "${ACTIVE}" "${SIGNATURE}" position)
    stage10_brace_depth("${ACTIVE}" ${position} depth)
    if(NOT depth EQUAL 1)
        message(FATAL_ERROR
            "Stage 10 ${LABEL} is not a namespace-level definition")
    endif()
    stage10_extract_sanitized_block("${ACTIVE}" "${SIGNATURE}"
        active_block)
    stage10_extract_sanitized_block("${LEXICAL}" "${SIGNATURE}"
        lexical_block)
    stage10_normalize("${active_block}" active_normalized)
    stage10_normalize("${lexical_block}" lexical_normalized)
    stage10_normalize("${EXPECTED}" expected_normalized)
    if(NOT active_normalized STREQUAL expected_normalized
            OR NOT lexical_normalized STREQUAL expected_normalized)
        message(FATAL_ERROR
            "Stage 10 ${LABEL} is not the exact reachable facade contract")
    endif()
endfunction()

function(stage10_extract_unique_platform_namespace
        ACTIVE LEXICAL OUT_ACTIVE_NAMESPACE OUT_LEXICAL_NAMESPACE)
    set(signature "namespace arpg::platform {")
    stage10_require_depth("active arpg::platform namespace"
        "${ACTIVE}" "${signature}" 0)
    stage10_require_depth("lexical arpg::platform namespace"
        "${LEXICAL}" "${signature}" 0)
    stage10_extract_sanitized_block("${ACTIVE}" "${signature}"
        active_namespace)
    stage10_extract_sanitized_block("${LEXICAL}" "${signature}"
        lexical_namespace)
    set(${OUT_ACTIVE_NAMESPACE} "${active_namespace}" PARENT_SCOPE)
    set(${OUT_LEXICAL_NAMESPACE} "${lexical_namespace}" PARENT_SCOPE)
endfunction()

function(stage10_require_order LABEL SOURCE)
    if(ARGC LESS 3)
        message(FATAL_ERROR "Stage 10 ${LABEL} has no ordered tokens")
    endif()
    set(previous -1)
    math(EXPR last_argument "${ARGC} - 1")
    foreach(argument_index RANGE 2 ${last_argument})
        set(argument_name "ARGV${argument_index}")
        set(token "${${argument_name}}")
        string(FIND "${SOURCE}" "${token}" position)
        if(position EQUAL -1
                OR (NOT previous EQUAL -1 AND position LESS_EQUAL previous))
            message(FATAL_ERROR
                "Stage 10 ${LABEL} is missing, duplicated, or reordered: ${token}")
        endif()
        set(previous ${position})
    endforeach()
endfunction()

function(stage10_extract_between SOURCE BEGIN_TOKEN END_TOKEN OUT_CROP)
    string(FIND "${SOURCE}" "${BEGIN_TOKEN}" begin)
    string(FIND "${SOURCE}" "${END_TOKEN}" end)
    if(begin EQUAL -1 OR end EQUAL -1 OR end LESS_EQUAL begin)
        message(FATAL_ERROR "Stage 10 owner crop is missing")
    endif()
    math(EXPR length "${end} - ${begin}")
    string(SUBSTRING "${SOURCE}" ${begin} ${length} crop)
    set(${OUT_CROP} "${crop}" PARENT_SCOPE)
endfunction()

foreach(required FIXTURE_SOURCE VALIDATION_GAME_SOURCE FORMAL_SOURCE
        CAPTURE_SCRIPT FORMAL_CAPTURE_SCRIPT STRESS_SOURCE HOST_HEADER HOST_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 10 evidence guard missing ${required}")
    endif()
endforeach()

file(READ "${FIXTURE_SOURCE}" fixture_source)
file(READ "${VALIDATION_GAME_SOURCE}" validation_game_source)
file(READ "${FORMAL_SOURCE}" formal_source)
file(READ "${CAPTURE_SCRIPT}" capture_script)
file(READ "${FORMAL_CAPTURE_SCRIPT}" formal_capture_script)
file(READ "${STRESS_SOURCE}" stress_source)
file(READ "${HOST_HEADER}" host_header)
file(READ "${HOST_SOURCE}" host_source)
get_filename_component(host_directory "${HOST_HEADER}" DIRECTORY)
set(stage_source "${host_directory}/host_validation_stage10_11.cpp")
set(stage_header "${host_directory}/host_validation_stage10_11.hpp")
set(host_validation_runtime_source
    "${host_directory}/host_validation_runtime.cpp")
if(DEFINED STAGE_SOURCE)
    set(stage_source "${STAGE_SOURCE}")
endif()
if(DEFINED STAGE_HEADER)
    set(stage_header "${STAGE_HEADER}")
endif()
if(DEFINED HOST_VALIDATION_RUNTIME_SOURCE)
    set(host_validation_runtime_source "${HOST_VALIDATION_RUNTIME_SOURCE}")
endif()
if(NOT EXISTS "${stage_source}")
    message(FATAL_ERROR "Stage 10 validation route target is missing: ${stage_source}")
endif()
if(NOT EXISTS "${stage_header}")
    message(FATAL_ERROR "Stage 10 validation state target is missing: ${stage_header}")
endif()
if(NOT EXISTS "${host_validation_runtime_source}")
    message(FATAL_ERROR
        "Stage 10 facade runtime target is missing: ${host_validation_runtime_source}")
endif()
file(READ "${stage_source}" stage_source_text)
file(READ "${stage_header}" stage_header_text)
file(READ "${host_validation_runtime_source}" host_validation_runtime_text)
set(formal_evidence "${fixture_source}\n${validation_game_source}\n${formal_source}\n${capture_script}\n${formal_capture_script}\n${host_header}\n${host_source}\n${host_validation_runtime_text}\n${stage_source_text}")

evidence_extract_cpp_function_block("${stage_source_text}"
    "combat::MovementInput stage10_validation_input(" stage10_input_block)
evidence_extract_cpp_function_block("${stage_source_text}"
    "bool stage10_validation_reached(" stage10_reached_block)

foreach(forbidden
        "DungeonSessionTestAccess"
        "CombatWorldTestAccess"
        "force_defeat"
        "relay_defeated"
        "defeat_monster\\("
        "set_phase\\("
        "stable_state_"
        "phase_[ \\t]*="
        "generated_mask[ \\t]*="
        "claimed_mask[ \\t]*="
        "abandoned_mask[ \\t]*=")
    if(formal_evidence MATCHES "${forbidden}")
        message(FATAL_ERROR "Forbidden Stage 10 evidence injection: ${forbidden}")
    endif()
endforeach()

if(NOT fixture_source MATCHES "SaveStore"
        OR NOT fixture_source MATCHES "request_pickup"
        OR NOT fixture_source MATCHES "abyss_exit_warning"
        OR NOT fixture_source MATCHES "abyss_exit_confirmation_armed"
        OR NOT fixture_source MATCHES "resolution.room_seed"
        OR NOT fixture_source MATCHES "resolution.rule"
        OR NOT fixture_source MATCHES "MovementInput")
    message(FATAL_ERROR "Stage 10 fixture must use real save, pickup, warning, confirmation, and movement APIs")
endif()
if(NOT validation_game_source MATCHES "run_raylib_host"
        OR NOT formal_source MATCHES "make_door_transition"
        OR NOT formal_source MATCHES "run_raylib_host")
    message(FATAL_ERROR "Formal Stage 10 evidence must use generation and normal host inputs")
endif()
foreach(required_stage10_input_token
        "session.reset_current_room()"
        "session.request_descent(true)"
        "session.request_active_skill_slot(1U)"
        "session.queue_action(combat::Action::light)")
    string(FIND "${stage10_input_block}" "${required_stage10_input_token}"
        required_stage10_index)
    if(required_stage10_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 10 validation input lacks production route: ${required_stage10_input_token}")
    endif()
endforeach()
foreach(required_stage10_reached_token
        "has_environment_visual("
        "state.entered_abyss && state.descent_warning_seen")
    string(FIND "${stage10_reached_block}" "${required_stage10_reached_token}"
        required_stage10_reached_index)
    if(required_stage10_reached_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 10 validation completion lacks production predicate: ${required_stage10_reached_token}")
    endif()
endforeach()
string(FIND "${stage_source_text}" "bool has_environment_visual("
    stage10_environment_visual_index)
if(stage10_environment_visual_index EQUAL -1)
    message(FATAL_ERROR "Stage 10 validation environment helper is missing")
endif()
if(NOT stage_header_text MATCHES "struct Stage10ValidationState final")
    message(FATAL_ERROR "Stage 10 validation state definition is missing")
endif()

# Stage 10 input and completion stay implemented in
# host_validation_stage10_11.cpp, while these exact active facade methods own
# scenario selection. Full function contracts reject dead if(false) copies,
# uncalled lambdas, early returns, altered real returns, and cross-scope owners.
stage10_unconditional_cpp_surface("${host_validation_runtime_text}"
    host_validation_runtime_active host_validation_runtime_lexical)
stage10_extract_unique_platform_namespace(
    "${host_validation_runtime_active}" "${host_validation_runtime_lexical}"
    host_validation_runtime_namespace_active
    host_validation_runtime_namespace_lexical)

set(stage10_movement_signature
    "combat::MovementInput HostValidationRuntime::fixed_step_movement(")
set(stage10_movement_contract [=[
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
stage10_require_count("global active fixed_step_movement definition"
    "${host_validation_runtime_active}" "${stage10_movement_signature}" 1)
stage10_require_count("global lexical fixed_step_movement definition"
    "${host_validation_runtime_lexical}" "${stage10_movement_signature}" 1)
stage10_require_active_exact_definition(
    "fixed_step_movement owner"
    "${host_validation_runtime_namespace_active}"
    "${host_validation_runtime_namespace_lexical}"
    "${stage10_movement_signature}" "${stage10_movement_contract}")

set(stage10_target_signature
    "bool HostValidationRuntime::fixed_step_target_reached(")
set(stage10_target_contract [=[
bool HostValidationRuntime::fixed_step_target_reached(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    return host_validation::stage10_validation_reached(
        snapshot, *impl_->config, impl_->states.stage10)
        || host_validation::stage11_validation_reached(
            snapshot, *impl_->config, impl_->states.stage11);
}
]=])
stage10_require_count("global active fixed_step_target_reached definition"
    "${host_validation_runtime_active}" "${stage10_target_signature}" 1)
stage10_require_count("global lexical fixed_step_target_reached definition"
    "${host_validation_runtime_lexical}" "${stage10_target_signature}" 1)
stage10_require_active_exact_definition(
    "fixed_step_target_reached owner"
    "${host_validation_runtime_namespace_active}"
    "${host_validation_runtime_namespace_lexical}"
    "${stage10_target_signature}" "${stage10_target_contract}")

# Task 7C moves Stage10 presentation thresholds and the shared generic-capture
# completion latch behind the facade. Bind the real active methods rather than
# accepting Host-side state aliases or cross-file token concatenation.
set(stage10_present_signature
    "PresentationDecision HostValidationRuntime::observe_presented_frame(")
stage10_require_count("global active presentation decision definition"
    "${host_validation_runtime_active}" "${stage10_present_signature}" 1)
stage10_require_count("global lexical presentation decision definition"
    "${host_validation_runtime_lexical}" "${stage10_present_signature}" 1)
stage10_require_depth("active presentation decision inside arpg::platform"
    "${host_validation_runtime_namespace_active}" "${stage10_present_signature}" 1)
stage10_require_depth("lexical presentation decision inside arpg::platform"
    "${host_validation_runtime_namespace_lexical}" "${stage10_present_signature}" 1)
stage10_extract_sanitized_block("${host_validation_runtime_namespace_active}"
    "${stage10_present_signature}" stage10_present_block)
foreach(stage10_present_direct_token IN ITEMS
        "const bool stage10_target_visible ="
        "if (impl_->config->stage10_validation"
        "const bool stage10_reached =")
    stage10_require_depth("T7C Stage10 chaos branch contract"
        "${stage10_present_block}" "${stage10_present_direct_token}" 1)
endforeach()
foreach(stage10_present_direct_token IN ITEMS
        "decision.validation_complete =" "return decision;")
    stage10_require_depth("T7C Stage10 decision completion contract"
        "${stage10_present_block}" "${stage10_present_direct_token}" 1)
endforeach()
stage10_require_depth("T7C Stage10 generic completion decision"
    "${stage10_present_block}" "decision.generic_capture_complete =" 1)
set(stage10_chaos_branch [=[
if (impl_->config->stage10_validation
        == Stage10ValidationScenario::chaos_expansion
    && stage10_target_visible) {
    ++impl_->states.stage10.chaos_presented_frames;
}
const bool stage10_reached = stage10_target_visible
    && (impl_->config->stage10_validation
            != Stage10ValidationScenario::chaos_expansion
        || impl_->states.stage10.chaos_presented_frames >= 16U);
]=])
set(stage10_target_contract [=[
const bool stage10_target_visible =
    host_validation::stage10_validation_reached(
        snapshot, *impl_->config, impl_->states.stage10);
]=])
set(stage10_completion_contract [=[
decision.validation_complete = stage10_reached || stage11_reached
    || stage11b_reached || stage11c_reached
    || stage11d_reached || stage17_reached;
]=])
string(REGEX REPLACE "[ \t\r\n]+" "" stage10_present_compact
    "${stage10_present_block}")
string(REGEX REPLACE "[ \t\r\n]+" "" stage10_chaos_branch_compact
    "${stage10_chaos_branch}")
string(REGEX REPLACE "[ \t\r\n]+" "" stage10_target_contract_compact
    "${stage10_target_contract}")
string(REGEX REPLACE "[ \t\r\n]+" "" stage10_completion_contract_compact
    "${stage10_completion_contract}")
stage10_require_count("T7C Stage10 target owner contract"
    "${stage10_present_compact}" "${stage10_target_contract_compact}" 1)
stage10_require_count("T7C Stage10 chaos branch contract"
    "${stage10_present_compact}" "${stage10_chaos_branch_compact}" 1)
stage10_require_count("T7C Stage10 decision completion contract"
    "${stage10_present_compact}" "${stage10_completion_contract_compact}" 1)
stage10_require_count("T7C Stage10 generic completion decision"
    "${stage10_present_compact}"
    "decision.generic_capture_complete=impl_->generic_capture_complete;" 1)
stage10_require_count("T7C Stage10 real decision return"
    "${stage10_present_compact}" "returndecision;" 1)
stage10_require_order("T7C Stage10 presentation result flow"
    "${stage10_present_compact}"
    "${stage10_target_contract_compact}"
    "${stage10_chaos_branch_compact}"
    "${stage10_completion_contract_compact}"
    "decision.generic_capture_complete=impl_->generic_capture_complete;"
    "returndecision;")

set(stage10_capture_result_signature
    "void HostValidationRuntime::observe_capture_result(")
stage10_require_count("global active capture-result definition"
    "${host_validation_runtime_active}" "${stage10_capture_result_signature}" 1)
stage10_require_count("global lexical capture-result definition"
    "${host_validation_runtime_lexical}" "${stage10_capture_result_signature}" 1)
stage10_require_depth("active capture-result definition inside arpg::platform"
    "${host_validation_runtime_namespace_active}"
    "${stage10_capture_result_signature}" 1)
stage10_require_depth("lexical capture-result definition inside arpg::platform"
    "${host_validation_runtime_namespace_lexical}"
    "${stage10_capture_result_signature}" 1)
stage10_extract_sanitized_block("${host_validation_runtime_namespace_active}"
    "${stage10_capture_result_signature}" stage10_capture_result_block)
foreach(stage10_capture_direct_token IN ITEMS
        "if (owner != CaptureOwner::generic_validation)"
        "if (!succeeded) return;"
        "impl_->generic_capture_complete = true;")
    stage10_require_depth("T7C direct generic capture-result token"
        "${stage10_capture_result_block}" "${stage10_capture_direct_token}" 1)
endforeach()
stage10_require_order("T7C generic capture-result flow"
    "${stage10_capture_result_block}"
    "if (owner != CaptureOwner::generic_validation)"
    "if (!succeeded) return;"
    "impl_->generic_capture_complete = true;")

# Extract the unique real Host namespace and run function from both the active
# and lexical surfaces. Every fixed-step owner below is then proven inside that
# exact function boundary, so moving the producer/loop into a later helper can
# no longer satisfy whole-file ordering checks.
stage10_unconditional_cpp_surface("${host_source}"
    host_active host_lexical)
stage10_extract_unique_platform_namespace("${host_active}" "${host_lexical}"
    host_namespace_active host_namespace_lexical)
set(stage10_run_signature "HostExitCode run_raylib_host(")
stage10_require_count("global active run_raylib_host definition"
    "${host_active}" "${stage10_run_signature}" 1)
stage10_require_count("global lexical run_raylib_host definition"
    "${host_lexical}" "${stage10_run_signature}" 1)
stage10_require_depth("active run_raylib_host inside arpg::platform"
    "${host_namespace_active}" "${stage10_run_signature}" 1)
stage10_require_depth("lexical run_raylib_host inside arpg::platform"
    "${host_namespace_lexical}" "${stage10_run_signature}" 1)
stage10_extract_sanitized_block("${host_namespace_active}"
    "${stage10_run_signature}" stage10_run_active)
stage10_extract_sanitized_block("${host_namespace_lexical}"
    "${stage10_run_signature}" stage10_run_lexical)

# Host consumes only the opaque facade decision/result. Direct Stage10 state or
# predicate ownership is forbidden on both active and lexical surfaces.
foreach(removed_host_stage10_token IN ITEMS
        "Stage10ValidationState"
        "stage10_validation_state"
        "stage10_validation_captured"
        "host_validation::stage10_validation_reached(")
    stage10_require_count("removed active Host Stage10 presentation owner"
        "${stage10_run_active}" "${removed_host_stage10_token}" 0)
    stage10_require_count("removed lexical Host Stage10 presentation owner"
        "${stage10_run_lexical}" "${removed_host_stage10_token}" 0)
endforeach()
foreach(required_host_stage10_facade IN ITEMS
        "const PresentationDecision decision ="
        "validation_runtime->observe_presented_frame("
        "validation_runtime->observe_capture_result(")
    set(stage10_host_facade_label "active run-host Task7C facade")
    if(required_host_stage10_facade STREQUAL
            "validation_runtime->observe_capture_result(")
        set(stage10_host_facade_label
            "T7C-M21 capture-result callback must use the selected owner and real effective success")
    endif()
    stage10_require_count("${stage10_host_facade_label}"
        "${stage10_run_active}" "${required_host_stage10_facade}" 1)
    stage10_require_count("${stage10_host_facade_label} lexical"
        "${stage10_run_lexical}" "${required_host_stage10_facade}" 1)
endforeach()

set(stage10_fixed_crop_begin
    "const combat::MovementInput movement = forward_movement")
set(stage10_fixed_loop_signature
    "for (std::uint32_t step = 0; step < frame.steps; ++step)")
set(stage10_fixed_crop_end
    "if (inventory.is_open() != inventory_open_before")
set(stage10_presentation_anchor
    "const PresentationDecision decision =")
set(stage10_run_owner_inventory
    "${stage10_fixed_crop_begin}"
    "${stage10_fixed_loop_signature}"
    "validation_runtime->fixed_step_movement("
    "validation_runtime->fixed_step_target_reached("
    "${stage10_fixed_crop_end}"
    "${stage10_presentation_anchor}")
foreach(owner_token IN LISTS stage10_run_owner_inventory)
    stage10_require_count("active run-host fixed-step owner"
        "${stage10_run_active}" "${owner_token}" 1)
    stage10_require_count("lexical run-host fixed-step owner"
        "${stage10_run_lexical}" "${owner_token}" 1)
endforeach()
foreach(facade_call IN ITEMS
        "validation_runtime->fixed_step_movement("
        "validation_runtime->fixed_step_target_reached(")
    stage10_require_count("global active Host facade owner"
        "${host_active}" "${facade_call}" 1)
    stage10_require_count("global lexical Host facade owner"
        "${host_lexical}" "${facade_call}" 1)
endforeach()
stage10_require_order("active run-host fixed-step data flow"
    "${stage10_run_active}" ${stage10_run_owner_inventory})
stage10_require_order("lexical run-host fixed-step data flow"
    "${stage10_run_lexical}" ${stage10_run_owner_inventory})
stage10_extract_between("${stage10_run_active}"
    "${stage10_fixed_crop_begin}" "${stage10_fixed_crop_end}"
    stage10_fixed_crop_active)
stage10_extract_between("${stage10_run_lexical}"
    "${stage10_fixed_crop_begin}" "${stage10_fixed_crop_end}"
    stage10_fixed_crop_lexical)
stage10_require_depth("direct fixed-step loop in run_raylib_host"
    "${stage10_run_active}" "${stage10_fixed_loop_signature}" 3)
stage10_extract_sanitized_block("${stage10_run_active}"
    "${stage10_fixed_loop_signature}" stage10_fixed_loop)

set(stage10_host_movement_statement
    "step_movement = validation_runtime->fixed_step_movement(")
set(stage10_host_target_statement
    "if (validation_runtime->fixed_step_target_reached(current))")
stage10_require_depth("fixed-step movement result assignment"
    "${stage10_fixed_loop}" "${stage10_host_movement_statement}" 2)
stage10_require_depth("fixed-step target result gate"
    "${stage10_fixed_loop}" "${stage10_host_target_statement}" 1)

# Prove the facade movement result flows directly into fixed_tick, and prove
# the target result controls the loop's sole terminal break after event drain.
set(stage10_fixed_prefix [=[
for (std::uint32_t step = 0; step < frame.steps; ++step) {
    previous = current;
    const bool step_death = current.death.has_value();
    combat::MovementInput step_movement{};
    if (!step_death) {
        step_movement = validation_runtime->fixed_step_movement(
            *session, current, movement);
    }
    runtime.fixed_tick(step_movement,
        loot_pickup_policy(live_settings.loot_filter_mode));
]=])
set(stage10_fixed_suffix [=[
drain_events(*session, renderer, feedback, audio,
    validation_runtime.get());
if (validation_runtime->fixed_step_target_reached(current)) {
    break;
}
}
]=])
stage10_normalize("${stage10_fixed_loop}" stage10_fixed_loop_normalized)
stage10_normalize("${stage10_fixed_prefix}" stage10_fixed_prefix_normalized)
stage10_normalize("${stage10_fixed_suffix}" stage10_fixed_suffix_normalized)
string(FIND "${stage10_fixed_loop_normalized}"
    "${stage10_fixed_prefix_normalized}" stage10_fixed_prefix_index)
if(NOT stage10_fixed_prefix_index EQUAL 0)
    message(FATAL_ERROR
        "Stage 10 Host facade movement result does not directly feed fixed_tick")
endif()
string(LENGTH "${stage10_fixed_loop_normalized}" stage10_fixed_loop_length)
string(LENGTH "${stage10_fixed_suffix_normalized}" stage10_fixed_suffix_length)
math(EXPR stage10_fixed_suffix_begin
    "${stage10_fixed_loop_length} - ${stage10_fixed_suffix_length}")
if(stage10_fixed_suffix_begin LESS 0)
    message(FATAL_ERROR "Stage 10 Host target facade tail is missing")
endif()
string(SUBSTRING "${stage10_fixed_loop_normalized}"
    ${stage10_fixed_suffix_begin} -1 stage10_fixed_actual_suffix)
if(NOT stage10_fixed_actual_suffix STREQUAL stage10_fixed_suffix_normalized)
    message(FATAL_ERROR
        "Stage 10 Host target facade result does not own the terminal break")
endif()
stage10_require_count("fixed-step terminal break"
    "${stage10_fixed_loop}" "break;" 1)

# The lexical crop includes inactive code and is phase-2 folded. Therefore an
# old direct call split with backslash-newline, or hidden under #if 0 inside the
# fixed-step segment, still fails without rejecting later presentation reads.
foreach(old_owner IN ITEMS
        "host_validation::stage10_validation_input("
        "host_validation::stage10_validation_reached(")
    stage10_require_count("removed active fixed-step owner"
        "${stage10_fixed_loop}" "${old_owner}" 0)
    stage10_require_count("removed lexical fixed-step owner"
        "${stage10_fixed_crop_lexical}" "${old_owner}" 0)
endforeach()

if(stress_source MATCHES "make_full_ground_pool"
        OR stress_source MATCHES "std::array<[^>]*GroundItem")
    message(FATAL_ERROR "Stage 10 stress may not substitute a local GroundItem array for the production session pool")
endif()
if(NOT stress_source MATCHES "DungeonSession"
        OR NOT stress_source MATCHES "fill_ground_pool"
        OR NOT stress_source MATCHES "ground_items\\(session\\)"
        OR NOT stress_source MATCHES "session.tick\\("
        OR NOT stress_source MATCHES "ground_saturation_count"
        OR NOT stress_source MATCHES "ground_saturation_count[\r\n ]*-[\r\n ]*ground_saturation_before[\r\n ]*==[\r\n ]*600U"
        OR NOT stress_source MATCHES "exactly_same_ground_item"
        OR NOT stress_source MATCHES "ground_before"
        OR NOT stress_source MATCHES "production_resolution"
        OR NOT stress_source MATCHES "last_abyss_resolution"
        OR NOT stress_source MATCHES "encode_checkpoint"
        OR NOT stress_source MATCHES "generate_long_trace\\(\\*first, 0U\\)"
        OR NOT stress_source MATCHES "generate_long_trace\\(\\*second, 37U\\)")
    message(FATAL_ERROR "Stage 10 stress must exercise production session ground, resolution, and restarted traces")
endif()

stage10_mask_non_direct_scopes("${stage10_run_active}"
    stage10_run_direct)
string(REGEX REPLACE "[ \t\r\n]+" "" stage10_run_compact
    "${stage10_run_direct}")
string(REGEX REPLACE "[ \t\r\n]+" "" host_compact "${host_active}")
set(present_helper
    "[[nodiscard]]boolpresent_frame_and_maybe_capture(constchar*path)noexcept{EndDrawing();if(path==nullptr)returntrue;Imageimage=LoadImageFromScreen();if(image.data==nullptr)returnfalse;constboolexported=ExportImage(image,path);UnloadImage(image);returnexported;}")
string(FIND "${host_compact}" "${present_helper}" present_helper_index)
set(capture_result_gate
    "constboolcapture_succeeded=present_frame_and_maybe_capture(capture_path.has_value()?capture_path->c_str():nullptr);")
string(FIND "${stage10_run_compact}" "${capture_result_gate}"
    capture_result_gate_index)
set(stage10_generic_selection_gate
    "if(!capture_path.has_value()&&decision.capture_owner==CaptureOwner::generic_validation){capture_path=config.validation_capture_file->string();selected_capture_owner=CaptureOwner::generic_validation;generic_capture_path_selected=true;}")
string(FIND "${stage10_run_compact}" "${stage10_generic_selection_gate}"
    stage10_generic_selection_gate_index)
set(stage10_manual_override_gate
    "if(death_gate.screenshot||(config.validation_request_screenshot&&presented_frame_count==1U)){generic_capture_path_selected=false;capture_path=host_screenshot_path(config);}")
string(FIND "${stage10_run_compact}" "${stage10_manual_override_gate}"
    stage10_manual_override_gate_index)
set(stage10_result_gate
    "constboolselected_generic_capture_succeeded_now=selected_capture_owner==CaptureOwner::generic_validation&&generic_capture_path_selected&&capture_succeeded;")
string(FIND "${stage10_run_compact}" "${stage10_result_gate}"
    stage10_result_gate_index)
set(stage10_selected_owner_gate
    "CaptureOwnerselected_capture_owner=decision.capture_owner==CaptureOwner::stage17?CaptureOwner::stage17:CaptureOwner::none;")
string(FIND "${stage10_run_compact}" "${stage10_selected_owner_gate}"
    stage10_selected_owner_gate_index)
set(stage10_selected_result_gate
    "constboolselected_validation_capture_succeeded=selected_capture_owner==CaptureOwner::stage17?capture_succeeded:selected_generic_capture_succeeded_now;")
string(FIND "${stage10_run_compact}" "${stage10_selected_result_gate}"
    stage10_selected_result_gate_index)
set(stage10_observer_gate
    "validation_runtime->observe_capture_result(selected_capture_owner,selected_validation_capture_succeeded);")
string(FIND "${stage10_run_compact}" "${stage10_observer_gate}"
    stage10_observer_gate_index)
string(FIND "${stage10_run_compact}"
    "validation_runtime->observe_capture_result("
    stage10_observer_call_index)
set(stage10_completion_gate
    "constbooleffective_generic_complete=decision.generic_capture_complete||selected_generic_capture_succeeded_now;")
string(FIND "${stage10_run_compact}" "${stage10_completion_gate}"
    stage10_completion_gate_index)
set(stage10_clean_exit_gate
    "if(decision.validation_complete&&(!config.validation_capture_file.has_value()||effective_generic_complete)){begin_clean_exit();}")
string(FIND "${stage10_run_compact}" "${stage10_clean_exit_gate}"
    stage10_clean_exit_gate_index)
string(REGEX MATCHALL "EndDrawing\\(\\)" end_drawing_calls "${host_active}")
string(REGEX MATCHALL "LoadImageFromScreen\\(\\)" screen_load_calls "${host_active}")
string(REGEX MATCHALL "ExportImage\\(" export_image_calls "${host_active}")
string(REGEX MATCHALL "present_frame_and_maybe_capture\\("
    present_helper_mentions "${host_active}")
list(LENGTH end_drawing_calls end_drawing_count)
list(LENGTH screen_load_calls screen_load_count)
list(LENGTH export_image_calls export_image_count)
list(LENGTH present_helper_mentions present_helper_mention_count)
if(NOT host_header MATCHES "validation_exit_after_presented_frames")
    message(FATAL_ERROR "Stage 10 host lacks presented-frame exit control")
endif()
if(host_source MATCHES "export_screenshot"
        OR host_source MATCHES "capture_after_presented_frame")
    message(FATAL_ERROR "Stage 10 host may not expose a detached screenshot helper")
endif()
if(stage10_generic_selection_gate_index EQUAL -1)
    message(FATAL_ERROR
        "T7C-M15 generic validation must not replace an occupied Stage12/material capture path")
endif()
if(stage10_manual_override_gate_index EQUAL -1
        OR stage10_result_gate_index EQUAL -1)
    message(FATAL_ERROR
        "T7C-M17 manual override must report selected generic validation failure")
endif()
if(capture_result_gate_index EQUAL -1
        OR stage10_observer_call_index EQUAL -1
        OR NOT capture_result_gate_index LESS stage10_observer_call_index)
    message(FATAL_ERROR
        "T7C-M18 capture-result observation must follow EndDrawing")
endif()
if(stage10_selected_owner_gate_index EQUAL -1
        OR stage10_selected_result_gate_index EQUAL -1
        OR stage10_observer_gate_index EQUAL -1)
    message(FATAL_ERROR
        "T7C-M21 capture-result callback must use the selected owner and real effective success")
endif()
if(stage10_completion_gate_index EQUAL -1
        OR stage10_clean_exit_gate_index EQUAL -1
        OR NOT stage10_observer_gate_index LESS stage10_completion_gate_index
        OR NOT stage10_completion_gate_index LESS stage10_clean_exit_gate_index)
    message(FATAL_ERROR
        "T7C-M23 clean exit must include current-frame selected generic success")
endif()
string(FIND "${stage10_run_compact}"
    "if(!capture_path.has_value()&&stage12_item_baseline_frame)"
    stage10_stage12_capture_gate_index)
if(stage10_stage12_capture_gate_index EQUAL -1
        OR NOT stage10_stage12_capture_gate_index LESS
            stage10_generic_selection_gate_index
        OR NOT stage10_generic_selection_gate_index LESS
            stage10_manual_override_gate_index)
    message(FATAL_ERROR
        "T7C-M15 generic validation must remain after Stage12/material arbitration")
endif()
if(NOT capture_result_gate_index LESS stage10_result_gate_index
        OR NOT stage10_result_gate_index LESS stage10_selected_result_gate_index
        OR NOT stage10_selected_result_gate_index LESS
            stage10_observer_gate_index)
    message(FATAL_ERROR
        "Stage 10 capture result must flow from EndDrawing through the facade before clean-exit completion")
endif()
if(NOT end_drawing_count EQUAL 1 OR NOT screen_load_count EQUAL 1
        OR NOT export_image_count EQUAL 1)
    message(FATAL_ERROR
        "Stage 10 capture/order must contain exactly one EndDrawing, LoadImageFromScreen, and ExportImage "
        "(end=${end_drawing_count}, load=${screen_load_count}, export=${export_image_count})")
endif()
string(FIND "${host_compact}" "EndDrawing()" first_end_drawing_index)
string(FIND "${host_compact}" "LoadImageFromScreen()" first_screen_load_index)
if(first_end_drawing_index GREATER first_screen_load_index)
    message(FATAL_ERROR
        "Stage 10 capture helper must call EndDrawing before LoadImageFromScreen")
endif()
if(present_helper_index EQUAL -1)
    message(FATAL_ERROR
        "Stage 10 presentation and capture must be owned by the formal helper "
        "(helper=${present_helper_index})")
endif()

foreach(script_text capture_script formal_capture_script)
    if(NOT ${script_text} MATCHES "LastWriteTimeUtc"
            OR NOT ${script_text} MATCHES "System.Drawing"
            OR NOT ${script_text} MATCHES "HashSet"
            OR NOT ${script_text} MATCHES "nonBackground")
        message(FATAL_ERROR "Stage 10 capture script lacks freshness and pixel-content checks")
    endif()
endforeach()
