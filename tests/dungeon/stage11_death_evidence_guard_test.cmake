include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../platform/cpp_source_lexer.cmake")

foreach(required FIXTURE_SOURCE FORMAL_SOURCE CAPTURE_SCRIPT HOST_HEADER HOST_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 11 evidence guard missing ${required}")
    endif()
endforeach()
file(READ "${FIXTURE_SOURCE}" fixture_source)
file(READ "${FORMAL_SOURCE}" formal_source)
file(READ "${CAPTURE_SCRIPT}" capture_script)
file(READ "${HOST_HEADER}" host_header)
file(READ "${HOST_SOURCE}" host_source)
get_filename_component(host_directory "${HOST_HEADER}" DIRECTORY)
set(stage_source "${host_directory}/host_validation_stage10_11.cpp")
set(stage_header "${host_directory}/host_validation_stage10_11.hpp")
set(runtime_source "${host_directory}/host_validation_runtime.cpp")
if(DEFINED STAGE_SOURCE)
    set(stage_source "${STAGE_SOURCE}")
endif()
if(DEFINED STAGE_HEADER)
    set(stage_header "${STAGE_HEADER}")
endif()
if(DEFINED RUNTIME_SOURCE)
    set(runtime_source "${RUNTIME_SOURCE}")
endif()
if(NOT EXISTS "${stage_source}")
    message(FATAL_ERROR "Stage 11 validation route target is missing: ${stage_source}")
endif()
if(NOT EXISTS "${stage_header}")
    message(FATAL_ERROR "Stage 11 validation state target is missing: ${stage_header}")
endif()
if(NOT EXISTS "${runtime_source}")
    message(FATAL_ERROR "Stage 11 validation runtime target is missing: ${runtime_source}")
endif()
file(READ "${stage_source}" stage_source_text)
file(READ "${stage_header}" stage_header_text)
file(READ "${runtime_source}" runtime_source_text)
set(all_evidence "${fixture_source}\n${formal_source}\n${host_header}\n${host_source}\n${stage_source_text}\n${runtime_source_text}")

# Match Task 7A's active-plus-lexical ownership model.  Translation-phase
# splices and non-code text are removed first; all conditional regions are then
# masked so an inactive implementation can never satisfy an owner contract.
function(stage11_unconditional_cpp_surface SOURCE OUT_ACTIVE)
    arpg_sanitize_cpp_source("${SOURCE}" logical_source)
    if(ARGC GREATER 2)
        set("${ARGV2}" "${logical_source}" PARENT_SCOPE)
    endif()
    string(LENGTH "${logical_source}" source_length)
    set(cursor 0)
    set(conditional_depth 0)
    set(active_surface "")
    while(cursor LESS source_length)
        string(SUBSTRING "${logical_source}" ${cursor} -1 tail)
        string(FIND "${tail}" "\n" newline)
        if(newline EQUAL -1)
            set(line "${tail}")
            set(line_length -1)
        else()
            math(EXPR line_length "${newline} + 1")
            string(SUBSTRING "${tail}" 0 ${line_length} line)
        endif()
        set(mask_line FALSE)
        if(line MATCHES "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR conditional_depth "${conditional_depth} + 1")
            set(mask_line TRUE)
        elseif(line MATCHES "^[ \t]*#[ \t]*endif([ \t\r\n]|$)")
            if(conditional_depth EQUAL 0)
                message(FATAL_ERROR
                    "Stage 11 inactive preprocessor surface is unbalanced")
            endif()
            set(mask_line TRUE)
            math(EXPR conditional_depth "${conditional_depth} - 1")
        elseif(conditional_depth GREATER 0)
            set(mask_line TRUE)
        endif()
        if(mask_line)
            string(REGEX REPLACE "[^\r\n]" " " line "${line}")
        endif()
        string(APPEND active_surface "${line}")
        if(newline EQUAL -1)
            break()
        endif()
        math(EXPR cursor "${cursor} + ${line_length}")
    endwhile()
    if(NOT conditional_depth EQUAL 0)
        message(FATAL_ERROR
            "Stage 11 inactive preprocessor surface is unbalanced")
    endif()
    set("${OUT_ACTIVE}" "${active_surface}" PARENT_SCOPE)
endfunction()

function(stage11_compact_cpp SOURCE OUT_COMPACT)
    string(REGEX REPLACE "[ \t\r\n]+" "" compact "${SOURCE}")
    set("${OUT_COMPACT}" "${compact}" PARENT_SCOPE)
endfunction()

function(stage11_count_literal SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${TOKEN}" token_length)
    if(token_length EQUAL 0)
        message(FATAL_ERROR "Stage 11 guard cannot count an empty token")
    endif()
    string(LENGTH "${SOURCE}" before_length)
    string(REPLACE "${TOKEN}" "" without_token "${SOURCE}")
    string(LENGTH "${without_token}" after_length)
    math(EXPR count "(${before_length} - ${after_length}) / ${token_length}")
    set("${OUT_COUNT}" ${count} PARENT_SCOPE)
endfunction()

function(stage11_find_balanced_scope_end SOURCE OPEN_INDEX OUT_END OUT_VALID)
    string(LENGTH "${SOURCE}" source_length)
    if(OPEN_INDEX LESS 0 OR OPEN_INDEX GREATER_EQUAL source_length)
        set("${OUT_END}" -1 PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    set(cursor ${OPEN_INDEX})
    set(depth 0)
    while(cursor LESS source_length)
        string(SUBSTRING "${SOURCE}" ${cursor} 1 character)
        if(character STREQUAL "{")
            math(EXPR depth "${depth} + 1")
        elseif(character STREQUAL "}")
            math(EXPR depth "${depth} - 1")
            if(depth EQUAL 0)
                set("${OUT_END}" ${cursor} PARENT_SCOPE)
                set("${OUT_VALID}" TRUE PARENT_SCOPE)
                return()
            endif()
        endif()
        math(EXPR cursor "${cursor} + 1")
    endwhile()
    set("${OUT_END}" -1 PARENT_SCOPE)
    set("${OUT_VALID}" FALSE PARENT_SCOPE)
endfunction()

function(stage11_mask_non_direct_scopes SOURCE OUT_SOURCE)
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
            stage11_find_balanced_scope_end(
                "${SOURCE}" ${open_index} scope_end scope_valid)
            if(NOT scope_valid)
                message(FATAL_ERROR
                    "Stage 11 direct-scope fixture has no closing brace")
            endif()
        endif()
        math(EXPR copy_cursor "${scope_end} + 1")
    endwhile()
    set("${OUT_SOURCE}" "${masked}" PARENT_SCOPE)
endfunction()

function(stage11_extract_unique_function SOURCE SIGNATURE LABEL OUT_BLOCK)
    stage11_count_literal("${SOURCE}" "${SIGNATURE}" signature_count)
    if(NOT signature_count EQUAL 1)
        message(FATAL_ERROR "${LABEL}")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized(
        "${SOURCE}" "${SIGNATURE}" block_begin block_open block_end)
    math(EXPR block_length "${block_end} - ${block_begin} + 1")
    string(SUBSTRING "${SOURCE}" ${block_begin} ${block_length} block)
    set("${OUT_BLOCK}" "${block}" PARENT_SCOPE)
endfunction()

function(stage11_brace_depth SOURCE POSITION OUT_DEPTH)
    if(POSITION EQUAL 0)
        set("${OUT_DEPTH}" 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" 0 ${POSITION} prefix)
    string(REGEX MATCHALL "\\{" opening_braces "${prefix}")
    string(REGEX MATCHALL "\\}" closing_braces "${prefix}")
    list(LENGTH opening_braces opening_count)
    list(LENGTH closing_braces closing_count)
    math(EXPR depth "${opening_count} - ${closing_count}")
    set("${OUT_DEPTH}" ${depth} PARENT_SCOPE)
endfunction()

function(stage11_platform_definition_valid SOURCE SIGNATURE EXPECTED OUT_VALID)
    set("${OUT_VALID}" FALSE PARENT_SCOPE)
    set(namespace_token "namespace arpg::platform")
    stage11_count_literal("${SOURCE}" "${namespace_token}" namespace_count)
    stage11_count_literal("${SOURCE}" "${SIGNATURE}" signature_count)
    if(NOT namespace_count EQUAL 1 OR NOT signature_count EQUAL 1)
        return()
    endif()
    string(FIND "${SOURCE}" "${namespace_token}" namespace_begin)
    stage11_brace_depth("${SOURCE}" ${namespace_begin} namespace_depth)
    if(NOT namespace_depth EQUAL 0)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" ${namespace_begin} -1 namespace_tail)
    string(FIND "${namespace_tail}" "{" namespace_relative_open)
    if(namespace_relative_open EQUAL -1)
        return()
    endif()
    math(EXPR namespace_open "${namespace_begin} + ${namespace_relative_open}")
    stage11_find_balanced_scope_end(
        "${SOURCE}" ${namespace_open} namespace_close namespace_valid)
    if(NOT namespace_valid)
        return()
    endif()
    string(FIND "${SOURCE}" "${SIGNATURE}" signature_begin)
    stage11_brace_depth("${SOURCE}" ${signature_begin} signature_depth)
    if(NOT signature_depth EQUAL 1
            OR NOT signature_begin GREATER namespace_open
            OR NOT signature_begin LESS namespace_close)
        return()
    endif()
    stage11_extract_unique_function(
        "${SOURCE}" "${SIGNATURE}" "Stage 11 internal definition error"
        definition_block)
    stage11_compact_cpp("${definition_block}" definition_compact)
    stage11_compact_cpp("${EXPECTED}" expected_compact)
    if(definition_compact STREQUAL expected_compact)
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    endif()
endfunction()

function(stage11_require_exact_runtime_definition LABEL SIGNATURE EXPECTED)
    stage11_platform_definition_valid(
        "${runtime_active}" "${SIGNATURE}" "${EXPECTED}" active_valid)
    stage11_platform_definition_valid(
        "${runtime_lexical}" "${SIGNATURE}" "${EXPECTED}" lexical_valid)
    if(NOT active_valid OR NOT lexical_valid)
        message(FATAL_ERROR "${LABEL}")
    endif()
endfunction()

function(stage11_extract_token_block SOURCE TOKEN LABEL OUT_BLOCK)
    stage11_count_literal("${SOURCE}" "${TOKEN}" token_count)
    if(NOT token_count EQUAL 1)
        message(FATAL_ERROR "${LABEL}")
    endif()
    string(FIND "${SOURCE}" "${TOKEN}" token_begin)
    string(SUBSTRING "${SOURCE}" ${token_begin} -1 tail)
    string(FIND "${tail}" "{" relative_open)
    if(relative_open EQUAL -1)
        message(FATAL_ERROR "${LABEL}")
    endif()
    math(EXPR open_index "${token_begin} + ${relative_open}")
    stage11_find_balanced_scope_end(
        "${SOURCE}" ${open_index} block_end block_valid)
    if(NOT block_valid)
        message(FATAL_ERROR "${LABEL}")
    endif()
    math(EXPR block_length "${block_end} - ${token_begin} + 1")
    string(SUBSTRING "${SOURCE}" ${token_begin} ${block_length} block)
    set("${OUT_BLOCK}" "${block}" PARENT_SCOPE)
endfunction()

stage11_unconditional_cpp_surface(
    "${stage_source_text}" stage_source_active stage_source_lexical)
evidence_extract_cpp_function_block("${stage_source_active}"
    "combat::MovementInput stage11_validation_input(" stage11_input_block)
evidence_extract_cpp_function_block("${stage_source_active}"
    "bool stage11_validation_reached(" stage11_reached_block)

set(public_death_mutation_scan "${all_evidence}")
string(REPLACE "==" "__stage11_eq__" public_death_mutation_scan
    "${public_death_mutation_scan}")
string(REPLACE "!=" "__stage11_ne__" public_death_mutation_scan
    "${public_death_mutation_scan}")
string(REPLACE "<=" "__stage11_le__" public_death_mutation_scan
    "${public_death_mutation_scan}")
string(REPLACE ">=" "__stage11_ge__" public_death_mutation_scan
    "${public_death_mutation_scan}")
set(public_death_field_mutation
    "([.]|->)[ \t\r\n]*death([ \t\r\n]*[.][ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*|[ \t\r\n]*\\[[^]]*\\])+[ \t\r\n]*(=|[+*/%|&^-]=|<<=|>>=)")
if(public_death_mutation_scan MATCHES "${public_death_field_mutation}")
    message(FATAL_ERROR
        "Forbidden Stage 11 public death injection: death subfield mutation")
endif()

foreach(public_death_injection
        "[.]death[ \t\r\n]*="
        "->[ \t\r\n]*death[ \t\r\n]*="
        "[.]death[ \t\r\n]*[.]emplace[ \t\r\n]*\\("
        "death_snapshot[ \t\r\n]*="
        "death_snapshot[ \t\r\n]*[.]emplace[ \t\r\n]*\\("
        "death_checkpoint[ \t\r\n]*="
        "death_checkpoint[ \t\r\n]*[.]emplace[ \t\r\n]*\\("
        "make_death_checkpoint[ \t\r\n]*\\(")
    if(public_death_mutation_scan MATCHES "${public_death_injection}")
        message(FATAL_ERROR
            "Forbidden Stage 11 public death injection: ${public_death_injection}")
    endif()
endforeach()

foreach(forbidden "DungeonSessionTestAccess" "CombatWorldTestAccess"
        "force_defeat" "stable_state_" "phase_[ \t]*="
        "pending_save_[ \t]*=")
    if(all_evidence MATCHES "${forbidden}")
        message(FATAL_ERROR "Forbidden Stage 11 evidence injection: ${forbidden}")
    endif()
endforeach()

stage11_unconditional_cpp_surface(
    "${fixture_source}" fixture_active fixture_lexical)
foreach(required_fixture "SaveCommitStorage" "SaveCommitWorker"
        "acquire_capture_slot" "capture_save_checkpoint"
        "submit[ \\t\\r\\n]*\\(" "try_take_completion"
        "inspect_checkpoint_v9_envelope" "loaded_checkpoint"
        "loaded_format" "loaded_migrated" "loaded_slot"
        "same_run_state" "same_room_progress_checkpoint"
        "restore_room_progress_checkpoint" "release_loaded_checkpoints"
        "stop_and_join" "sleep_for" "session.tick" "pending_save_view"
        "request_death_continue" "fresh_scan_and_restart")
    if(NOT fixture_active MATCHES "${required_fixture}")
        message(FATAL_ERROR "Fixture lacks production API: ${required_fixture}")
    endif()
endforeach()

if(NOT formal_source MATCHES "run_raylib_host"
        OR NOT formal_source MATCHES "SaveStore"
        OR NOT formal_source MATCHES "formal-path-summary.txt")
    message(FATAL_ERROR "Formal Stage 11 executable lacks real host/save/summary evidence")
endif()
foreach(required_script "LastWriteTimeUtc" "System.Drawing" "GetPixel"
        "nonBackground" "Get-PanelHash" "formal-path-summary.txt"
        "panelDark[ \t]*-lt[ \t]*500"
        "panelAccent[ \t]*-lt[ \t]*15"
        "panelAuthored[ \t]*-lt[ \t]*3000"
        "greenDelta[ \t]*-ge[ \t]*8"
        "blueDelta[ \t]*-ge[ \t]*8"
        "panelLeft[ \t]*=[ \t]*152"
        "panelTop[ \t]*=[ \t]*80"
        "panelRightExclusive[ \t]*=[ \t]*1128"
        "panelBottomExclusive[ \t]*=[ \t]*644")
    if(NOT capture_script MATCHES "${required_script}")
        message(FATAL_ERROR "Capture validator lacks ${required_script}")
    endif()
endforeach()

# Preserve the original present-then-capture proof before owner checks.  This
# lets the dedicated negative fixture continue to fail for capture order rather
# than for an unrelated facade diagnostic.
stage11_compact_cpp("${host_source}" compact_host_source)
set(capture_helper
    "boolpresent_frame_and_maybe_capture(constchar*path)noexcept{EndDrawing();if(path==nullptr)returntrue;Imageimage=LoadImageFromScreen();if(image.data==nullptr)returnfalse;constboolexported=ExportImage(image,path);UnloadImage(image);returnexported;}")
string(FIND "${compact_host_source}" "${capture_helper}" capture_helper_index)
string(REGEX MATCHALL "EndDrawing\\(\\)" capture_ends "${compact_host_source}")
string(REGEX MATCHALL "LoadImageFromScreen\\(\\)" capture_loads "${compact_host_source}")
string(REGEX MATCHALL "ExportImage\\(" capture_exports "${compact_host_source}")
list(LENGTH capture_ends capture_end_count)
list(LENGTH capture_loads capture_load_count)
list(LENGTH capture_exports capture_export_count)
if(capture_helper_index EQUAL -1 OR NOT capture_end_count EQUAL 1
        OR NOT capture_load_count EQUAL 1 OR NOT capture_export_count EQUAL 1)
    message(FATAL_ERROR
        "Capture must occur once after EndDrawing (helper=${capture_helper_index} end=${capture_end_count} load=${capture_load_count} export=${capture_export_count})")
endif()

foreach(required_stage11_input_token
        "session.request_descent(true)"
        "stage10_validation_input(")
    string(FIND "${stage11_input_block}" "${required_stage11_input_token}"
        required_stage11_index)
    if(required_stage11_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 validation input lacks production route: ${required_stage11_input_token}")
    endif()
endforeach()
set(stage11_early_hole_token
    "if (drive_to_depth && snapshot.exits_unlocked && snapshot.has_hole")
stage11_extract_token_block("${stage11_input_block}"
    "${stage11_early_hole_token}"
    "Stage 11 early hole route is missing" stage11_early_hole_block)
foreach(required_early_hole_token
        "settle_grid_route_movement"
        "grid_route_movement"
        "can_prompt_descent"
        "session.request_descent(true)")
    string(FIND "${stage11_early_hole_block}"
        "${required_early_hole_token}" required_early_hole_index)
    if(required_early_hole_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 early hole route lacks production descent: ${required_early_hole_token}")
    endif()
endforeach()
stage11_count_literal("${stage11_early_hole_block}"
    "session.request_descent(true)" early_hole_descent_count)
if(NOT early_hole_descent_count EQUAL 1)
    message(FATAL_ERROR
        "Stage 11 early hole route lacks production descent: request count ${early_hole_descent_count}")
endif()
foreach(required_stage11_reached_token
        "Stage11ValidationScenario::deep_continue"
        "state.continue_requested && state.saw_depth_two")
    string(FIND "${stage11_reached_block}" "${required_stage11_reached_token}"
        required_stage11_reached_index)
    if(required_stage11_reached_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 validation completion lacks production predicate: ${required_stage11_reached_token}")
    endif()
endforeach()
if(NOT stage_header_text MATCHES "struct Stage11ValidationState final"
        OR NOT stage_header_text MATCHES
            "Stage10GridRouteState[ \t\r\n]+sweep_grid"
        OR NOT stage_header_text MATCHES
            "Stage10ValidationState[ \t\r\n]+combat_driver")
    message(FATAL_ERROR "Stage 11 validation state definition is missing")
endif()

stage11_unconditional_cpp_surface(
    "${runtime_source_text}" runtime_active runtime_lexical)

set(expected_should_continue_death [=[
bool HostValidationRuntime::should_continue_death(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    const bool stage10_validation_continue =
        impl_->config->stage10_validation
            == Stage10ValidationScenario::player_death;
    const auto scenario = impl_->config->stage11_validation;
    const bool stage11_validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    return pending && (stage10_validation_continue
        || (stage11_validation_continue
            && !impl_->states.stage11.continue_requested));
}
]=])
stage11_require_exact_runtime_definition(
    "Stage 11 runtime should_continue_death owner contract is missing or altered"
    "bool HostValidationRuntime::should_continue_death("
    "${expected_should_continue_death}")

set(expected_observe_death_continue_result [=[
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
stage11_require_exact_runtime_definition(
    "Stage 11 runtime death-result observer contract is missing or altered"
    "void HostValidationRuntime::observe_death_continue_result("
    "${expected_observe_death_continue_result}")

set(expected_fixed_step_movement [=[
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
stage11_require_exact_runtime_definition(
    "Stage 11 runtime fixed-step movement owner contract is missing or altered"
    "combat::MovementInput HostValidationRuntime::fixed_step_movement("
    "${expected_fixed_step_movement}")

set(expected_fixed_step_target_reached [=[
bool HostValidationRuntime::fixed_step_target_reached(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    return host_validation::stage10_validation_reached(
        snapshot, *impl_->config, impl_->states.stage10)
        || host_validation::stage11_validation_reached(
            snapshot, *impl_->config, impl_->states.stage11);
}
]=])
stage11_require_exact_runtime_definition(
    "Stage 11 runtime fixed-step reached owner contract is missing or altered"
    "bool HostValidationRuntime::fixed_step_target_reached("
    "${expected_fixed_step_target_reached}")

set(stage11_presentation_signature
    "PresentationDecision HostValidationRuntime::observe_presented_frame(")
stage11_extract_unique_function("${runtime_active}"
    "${stage11_presentation_signature}"
    "Stage 11 runtime presentation owner is missing or duplicated"
    stage11_presentation_block)
stage11_extract_unique_function("${runtime_lexical}"
    "${stage11_presentation_signature}"
    "Stage 11 runtime lexical presentation owner is missing or duplicated"
    stage11_presentation_lexical_block)
foreach(stage11_presentation_surface IN ITEMS runtime_active runtime_lexical)
    string(FIND "${${stage11_presentation_surface}}"
        "${stage11_presentation_signature}" stage11_presentation_position)
    stage11_brace_depth("${${stage11_presentation_surface}}"
        ${stage11_presentation_position} stage11_presentation_depth)
    if(NOT stage11_presentation_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage 11 runtime presentation owner must be a namespace-level definition")
    endif()
endforeach()
stage11_mask_non_direct_scopes("${stage11_presentation_block}"
    stage11_presentation_direct)
stage11_compact_cpp("${stage11_presentation_direct}"
    stage11_presentation_compact)
foreach(stage11_direct_token IN ITEMS "const bool stage11_target_visible =")
    stage11_count_literal("${stage11_presentation_direct}"
        "${stage11_direct_token}" stage11_direct_token_count)
    string(FIND "${stage11_presentation_direct}"
        "${stage11_direct_token}" stage11_direct_token_position)
    if(NOT stage11_direct_token_count EQUAL 1)
        message(FATAL_ERROR
            "T7C Stage11 presentation branch contract is missing or altered")
    endif()
    stage11_brace_depth("${stage11_presentation_direct}"
        ${stage11_direct_token_position} stage11_direct_token_depth)
    if(NOT stage11_direct_token_depth EQUAL 1)
        message(FATAL_ERROR
            "T7C Stage11 presentation branch contract is missing or altered")
    endif()
endforeach()
foreach(stage11_decision_token IN ITEMS
        "decision.validation_complete =" "return decision;")
    stage11_count_literal("${stage11_presentation_direct}"
        "${stage11_decision_token}" stage11_decision_token_count)
    string(FIND "${stage11_presentation_direct}"
        "${stage11_decision_token}" stage11_decision_token_position)
    if(NOT stage11_decision_token_count EQUAL 1)
        message(FATAL_ERROR
            "T7C Stage11 decision completion contract is missing or altered")
    endif()
    stage11_brace_depth("${stage11_presentation_direct}"
        ${stage11_decision_token_position} stage11_decision_token_depth)
    if(NOT stage11_decision_token_depth EQUAL 1)
        message(FATAL_ERROR
            "T7C Stage11 decision completion contract is missing or altered")
    endif()
endforeach()

set(stage11_presentation_update_contract [=[
const bool stage11_target_visible = host_validation::stage11_validation_reached(
    snapshot, *impl_->config, impl_->states.stage11);
if (stage11_target_visible) {
    ++impl_->states.stage11.target_presented_frames;
} else {
    impl_->states.stage11.target_presented_frames = 0U;
}
]=])
set(stage11_reached_contract [=[
const bool stage11_reached = stage11_target_visible
    && impl_->states.stage11.target_presented_frames >= 4U;
]=])
stage11_compact_cpp("${stage11_presentation_update_contract}"
    stage11_presentation_update_contract_compact)
stage11_compact_cpp("${stage11_reached_contract}"
    stage11_reached_contract_compact)
stage11_count_literal("${stage11_presentation_compact}"
    "${stage11_presentation_update_contract_compact}"
    stage11_presentation_update_contract_count)
stage11_count_literal("${stage11_presentation_compact}"
    "${stage11_reached_contract_compact}"
    stage11_reached_contract_count)
if(NOT stage11_presentation_update_contract_count EQUAL 1
        OR NOT stage11_reached_contract_count EQUAL 1)
    message(FATAL_ERROR
        "T7C Stage11 presentation branch contract is missing or altered")
endif()

set(stage11_completion_contract [=[
decision.validation_complete = stage10_reached || stage11_reached
    || stage11b_reached || stage11c_reached
    || stage11d_reached || stage17_reached;
]=])
stage11_compact_cpp("${stage11_completion_contract}"
    stage11_completion_contract_compact)
stage11_count_literal("${stage11_presentation_compact}"
    "${stage11_completion_contract_compact}"
    stage11_completion_contract_count)
stage11_count_literal("${stage11_presentation_compact}"
    "returndecision;" stage11_decision_return_count)
if(NOT stage11_completion_contract_count EQUAL 1
        OR NOT stage11_decision_return_count EQUAL 1)
    message(FATAL_ERROR
        "T7C Stage11 decision completion contract is missing or altered")
endif()
string(FIND "${stage11_presentation_compact}"
    "${stage11_presentation_update_contract_compact}"
    stage11_presentation_update_contract_position)
string(FIND "${stage11_presentation_compact}"
    "${stage11_reached_contract_compact}"
    stage11_reached_contract_position)
string(FIND "${stage11_presentation_compact}"
    "${stage11_completion_contract_compact}"
    stage11_completion_contract_position)
string(FIND "${stage11_presentation_compact}"
    "returndecision;" stage11_decision_return_position)
if(stage11_presentation_update_contract_position EQUAL -1
        OR stage11_reached_contract_position EQUAL -1
        OR stage11_completion_contract_position EQUAL -1
        OR stage11_decision_return_position EQUAL -1
        OR NOT stage11_presentation_update_contract_position LESS
            stage11_reached_contract_position
        OR NOT stage11_reached_contract_position LESS
            stage11_completion_contract_position
        OR NOT stage11_completion_contract_position LESS
            stage11_decision_return_position)
    message(FATAL_ERROR
        "T7C Stage11 presentation result is missing, discarded, or reordered")
endif()

stage11_count_literal("${runtime_active}" "request_death_continue("
    facade_continue_request_count)
if(NOT facade_continue_request_count EQUAL 0)
    message(FATAL_ERROR
        "Stage 11 validation facade must never submit a death continue request")
endif()

stage11_unconditional_cpp_surface("${host_source}" host_active host_lexical)
stage11_count_literal("${host_active}" "HostExitCode run_raylib_host("
    active_run_host_count)
stage11_count_literal("${host_lexical}" "HostExitCode run_raylib_host("
    lexical_run_host_count)
if(NOT active_run_host_count EQUAL 1 OR NOT lexical_run_host_count EQUAL 1)
    message(FATAL_ERROR
        "Stage 11 formal Host must contain one active run_raylib_host definition")
endif()
stage11_extract_unique_function("${host_active}" "HostExitCode run_raylib_host("
    "Stage 11 formal Host run function is missing" run_host_block)
stage11_mask_non_direct_scopes("${run_host_block}" direct_run_host)

foreach(required_host_token
        "MovementInput" "runtime.fixed_tick" "host_death_input_gate"
        "const PresentationDecision decision ="
        "validation_runtime->observe_presented_frame(")
    string(FIND "${direct_run_host}" "${required_host_token}" required_host_index)
    if(required_host_index EQUAL -1)
        message(FATAL_ERROR
            "Formal host lacks production input/save path: ${required_host_token}")
    endif()
endforeach()
foreach(forbidden_host_stage11_owner IN ITEMS
        "Stage11ValidationState"
        "stage11_validation_state"
        "HostValidationStateAccess::stage11("
        "host_validation::stage11_validation_reached(")
    string(FIND "${direct_run_host}" "${forbidden_host_stage11_owner}"
        forbidden_host_stage11_owner_index)
    if(NOT forbidden_host_stage11_owner_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 Host must not own presentation validation state directly: ${forbidden_host_stage11_owner}")
    endif()
endforeach()
string(FIND "${host_active}" "settings::StableKey::e" stable_e_mapping)
if(stable_e_mapping EQUAL -1)
    message(FATAL_ERROR
        "Formal host lacks production input/save path: settings::StableKey::e")
endif()

set(death_segment_begin_token
    "DeathInputGate death_gate = host_death_input_gate(")
set(death_segment_end_token
    "if (!death_gate.forward_gameplay && window_close_requested")
stage11_count_literal("${direct_run_host}" "${death_segment_begin_token}"
    death_segment_begin_count)
stage11_count_literal("${direct_run_host}" "${death_segment_end_token}"
    death_segment_end_count)
if(NOT death_segment_begin_count EQUAL 1 OR NOT death_segment_end_count EQUAL 1)
    message(FATAL_ERROR "Stage 11 formal death segment boundary is missing")
endif()
string(FIND "${direct_run_host}" "${death_segment_begin_token}"
    death_segment_begin)
string(FIND "${direct_run_host}" "${death_segment_end_token}"
    death_segment_end)
if(NOT death_segment_begin LESS death_segment_end)
    message(FATAL_ERROR "Stage 11 formal death segment boundary is reordered")
endif()
math(EXPR death_segment_length "${death_segment_end} - ${death_segment_begin}")
string(SUBSTRING "${direct_run_host}" ${death_segment_begin}
    ${death_segment_length} death_segment)

foreach(forbidden_death_owner
        "stage11_validation_state" "HostValidationStateAccess::stage11("
        "continue_requested")
    string(FIND "${death_segment}" "${forbidden_death_owner}"
        forbidden_death_owner_index)
    if(NOT forbidden_death_owner_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 Host death segment must not access continue_requested state directly")
    endif()
endforeach()

set(continue_decision_token
    "if (validation_runtime->should_continue_death(current))")
stage11_extract_token_block("${death_segment}" "${continue_decision_token}"
    "Stage 11 Host continue decision must use the runtime facade only"
    continue_decision_block)
stage11_compact_cpp("${continue_decision_block}" continue_decision_compact)
set(expected_continue_decision
    "if(validation_runtime->should_continue_death(current)){death_gate.continue_death=true;}")
if(NOT continue_decision_compact STREQUAL expected_continue_decision)
    message(FATAL_ERROR
        "Stage 11 Host continue decision must use the runtime facade only")
endif()

stage11_count_literal("${direct_run_host}" "runtime.request_death_continue()"
    host_continue_request_count)
if(NOT host_continue_request_count EQUAL 1)
    message(FATAL_ERROR
        "Formal validation continue must use the single death input gate request path")
endif()
stage11_count_literal("${death_segment}" "if (death_gate.continue_death)"
    exact_death_gate_condition_count)
if(NOT exact_death_gate_condition_count EQUAL 1)
    message(FATAL_ERROR
        "Formal death continue condition must be exactly death_gate.continue_death")
endif()
stage11_extract_token_block("${death_segment}"
    "if (death_gate.continue_death)"
    "Stage 11 death observer must follow the real request and snapshot refresh"
    death_gate_block)
stage11_compact_cpp("${death_gate_block}" death_gate_compact)
set(expected_death_gate_block
    "if(death_gate.continue_death){death_continue_result=runtime.request_death_continue();if(death_continue_result!=dungeon::RequestResult::rejected){previous=current;session->snapshot(current);}validation_runtime->observe_death_continue_result(death_continue_result);}")
if(NOT death_gate_compact STREQUAL expected_death_gate_block)
    message(FATAL_ERROR
        "Stage 11 death observer must follow the real request and snapshot refresh")
endif()
stage11_count_literal("${death_segment}"
    "validation_runtime->observe_death_continue_result("
    death_observer_count)
if(NOT death_observer_count EQUAL 1)
    message(FATAL_ERROR
        "Stage 11 death observer must follow the real request and snapshot refresh")
endif()

set(fixed_loop_token
    "for (std::uint32_t step = 0; step < frame.steps; ++step)")
stage11_extract_token_block("${direct_run_host}" "${fixed_loop_token}"
    "Stage 11 direct fixed-step loop is missing" fixed_loop_block)
foreach(forbidden_fixed_owner
        "host_validation::stage11_validation_input("
        "host_validation::stage10_validation_input("
        "host_validation::stage11_validation_reached("
        "host_validation::stage10_validation_reached(")
    string(FIND "${fixed_loop_block}" "${forbidden_fixed_owner}"
        forbidden_fixed_owner_index)
    if(NOT forbidden_fixed_owner_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 direct fixed-step loop must not call old input/reached owners")
    endif()
endforeach()

stage11_extract_token_block("${fixed_loop_block}" "if (!step_death)"
    "Stage 11 fixed-step movement result must feed step_movement under the death gate"
    movement_gate_block)
stage11_compact_cpp("${movement_gate_block}" movement_gate_compact)
set(expected_movement_gate
    "if(!step_death){step_movement=validation_runtime->fixed_step_movement(*session,current,movement);}")
if(NOT movement_gate_compact STREQUAL expected_movement_gate)
    message(FATAL_ERROR
        "Stage 11 fixed-step movement result must feed step_movement under the death gate")
endif()

set(target_gate_token
    "if (validation_runtime->fixed_step_target_reached(current))")
stage11_extract_token_block("${fixed_loop_block}" "${target_gate_token}"
    "Stage 11 fixed-step reached result must directly guard break"
    target_gate_block)
stage11_compact_cpp("${target_gate_block}" target_gate_compact)
set(expected_target_gate
    "if(validation_runtime->fixed_step_target_reached(current)){break;}")
if(NOT target_gate_compact STREQUAL expected_target_gate)
    message(FATAL_ERROR
        "Stage 11 fixed-step reached result must directly guard break")
endif()

message(STATUS "Stage 11 production evidence guard passed")
