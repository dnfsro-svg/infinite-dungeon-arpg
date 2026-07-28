include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")

foreach(required GUARD_SCRIPT VALID_FIXTURE VALID_FORMAL VALID_CAPTURE
        VALID_HOST_HEADER VALID_HOST_SOURCE BAD_TEST_ACCESS BAD_CAPTURE_ORDER)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 11 guard self-test missing ${required}")
    endif()
endforeach()

get_filename_component(_stage_directory "${VALID_HOST_HEADER}" DIRECTORY)
set(VALID_STAGE_SOURCE "${_stage_directory}/host_validation_stage10_11.cpp")
if(NOT EXISTS "${VALID_STAGE_SOURCE}")
    message(FATAL_ERROR "Stage 11 guard self-test missing production stage source")
endif()

function(expect_guard_rejection name fixture host expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${fixture}
            -DFORMAL_SOURCE=${VALID_FORMAL}
            -DCAPTURE_SCRIPT=${VALID_CAPTURE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${host}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: mutated evidence was accepted")
    endif()
    string(FIND "${combined}" "${expected}" reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: wrong rejection reason; expected '${expected}', got: ${combined}")
    endif()
endfunction()

function(expect_guard_acceptance name fixture host)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${fixture}
            -DFORMAL_SOURCE=${VALID_FORMAL}
            -DCAPTURE_SCRIPT=${VALID_CAPTURE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${host}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${name}: read-only evidence was rejected: ${output}\n${error}")
    endif()
endfunction()

expect_guard_rejection(test_access "${BAD_TEST_ACCESS}" "${VALID_HOST_SOURCE}"
    "Forbidden Stage 11 evidence injection: DungeonSessionTestAccess")
set(public_mutations
    "checkpoint.next_state.death = fabricated_death"
    "checkpoint_ptr->death = fabricated_death"
    "death_snapshot.emplace(fabricated_death)"
    "death_snapshot = fabricated_death"
    "death_checkpoint.emplace(fabricated_death)"
    "death_checkpoint = fabricated_death"
    "auto death = make_death_checkpoint(fabricated_combat, room, target)"
    "checkpoint.death.lifecycle = pending_continue"
    "checkpoint.death.target_room.seed = 42"
    "checkpoint_ptr->death.final_damage = 0"
    "checkpoint.death.recent_damage[0] += 1"
    "checkpoint.death.final_damage -= 1"
    "checkpoint_ptr->death.target_room.seed |= 1")
set(public_index 0)
foreach(public_mutation IN LISTS public_mutations)
    math(EXPR public_index "${public_index} + 1")
    set(mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage11_bad_public_death_${public_index}.txt")
    file(WRITE "${mutation_file}" "${public_mutation}\n")
    expect_guard_rejection("public_death_${public_index}" "${mutation_file}"
        "${VALID_HOST_SOURCE}" "Forbidden Stage 11 public death injection")
    file(REMOVE "${mutation_file}")
endforeach()

file(READ "${VALID_FIXTURE}" valid_fixture_source)
set(read_only_file
    "${CMAKE_CURRENT_BINARY_DIR}/stage11_read_only_death_comparisons.txt")
file(WRITE "${read_only_file}" "${valid_fixture_source}\n"
    "// checkpoint.death == expected_death\n"
    "// checkpoint.death != other_death\n"
    "// checkpoint.death.lifecycle == pending_continue\n"
    "// checkpoint.death.target_room.seed != expected_seed\n")
expect_guard_acceptance(read_only_death_comparisons "${read_only_file}"
    "${VALID_HOST_SOURCE}")
file(REMOVE "${read_only_file}")

expect_guard_rejection(capture_order "${VALID_FIXTURE}" "${BAD_CAPTURE_ORDER}"
    "Capture must occur once after EndDrawing")

file(READ "${VALID_HOST_SOURCE}" valid_host_source)
set(validation_bypass_file
    "${CMAKE_CURRENT_BINARY_DIR}/stage11_bad_validation_continue_bypass.txt")
string(REPLACE
    "DeathInputGate death_gate = host_death_input_gate("
    "static_cast<void>(runtime.request_death_continue());\n            DeathInputGate death_gate = host_death_input_gate("
    validation_bypass_source "${valid_host_source}")
if(validation_bypass_source STREQUAL valid_host_source)
    message(FATAL_ERROR
        "validation bypass mutation did not find the death input gate")
endif()
file(WRITE "${validation_bypass_file}" "${validation_bypass_source}")
expect_guard_rejection(validation_continue_bypass "${VALID_FIXTURE}"
    "${validation_bypass_file}"
    "Formal validation continue must use the single death input gate")
file(REMOVE "${validation_bypass_file}")

foreach(gate_condition IN ITEMS
        "death_gate.continue_death || validation_continue"
        "validation_continue")
    string(MAKE_C_IDENTIFIER "${gate_condition}" mutation_suffix)
    set(condition_mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage11_bad_gate_${mutation_suffix}.txt")
    string(REPLACE
        "if (death_gate.continue_death) {"
        "if (${gate_condition}) {"
        condition_mutation_source "${valid_host_source}")
    if(condition_mutation_source STREQUAL valid_host_source)
        message(FATAL_ERROR
            "death gate condition mutation did not find the production condition")
    endif()
    file(WRITE "${condition_mutation_file}" "${condition_mutation_source}")
    expect_guard_rejection("death_gate_${mutation_suffix}"
        "${VALID_FIXTURE}" "${condition_mutation_file}"
        "Formal death continue condition must be exactly death_gate.continue_death")
    file(REMOVE "${condition_mutation_file}")
endforeach()

set(validation_scope_mutation_file
    "${CMAKE_CURRENT_BINARY_DIR}/stage11_bad_validation_continue_scope.txt")
string(REPLACE
    "if (death_continue_result\n                        != dungeon::RequestResult::rejected) {"
    "if (validation_continue && death_continue_result\n                        != dungeon::RequestResult::rejected) {"
    validation_scope_mutation_source "${valid_host_source}")
if(validation_scope_mutation_source STREQUAL valid_host_source)
    message(FATAL_ERROR
        "validation scope mutation did not find the request result condition")
endif()
file(WRITE "${validation_scope_mutation_file}"
    "${validation_scope_mutation_source}")
expect_guard_rejection(validation_continue_in_gate_scope "${VALID_FIXTURE}"
    "${validation_scope_mutation_file}"
    "Formal validation_continue must remain outside the death input gate scope")
file(REMOVE "${validation_scope_mutation_file}")

function(expect_stage_route_rejection name token inject_brace_noise)
    file(READ "${VALID_STAGE_SOURCE}" stage_source)
    evidence_find_cpp_function_bounds("${stage_source}"
        "combat::MovementInput stage11_validation_input("
        stage11_begin stage11_open stage11_end)
    math(EXPR stage11_input_length "${stage11_end} - ${stage11_begin} + 1")
    string(SUBSTRING "${stage_source}" ${stage11_begin} ${stage11_input_length}
        stage11_input)
    string(REPLACE "${token}" "" mutated_input "${stage11_input}")
    if(mutated_input STREQUAL stage11_input)
        message(FATAL_ERROR "${name}: mutation token was not found")
    endif()
    if(inject_brace_noise)
        math(EXPR stage11_open_after "${stage11_open} - ${stage11_begin} + 1")
        string(SUBSTRING "${mutated_input}" 0 ${stage11_open_after}
            input_prefix)
        string(SUBSTRING "${mutated_input}" ${stage11_open_after} -1
            input_suffix)
        set(brace_noise "\n    // { line-comment brace\n    /* { block-comment brace } */\n    const char* evidence_brace_string = \"{\\\"}\";\n    const char evidence_brace_character = '{';\n")
        set(mutated_input "${input_prefix}${brace_noise}${input_suffix}")
    endif()
    string(REPLACE "${stage11_input}" "${mutated_input}" mutated_source
        "${stage_source}")
    set(mutation_file "${CMAKE_CURRENT_BINARY_DIR}/stage11_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DFORMAL_SOURCE=${VALID_FORMAL}
            -DCAPTURE_SCRIPT=${VALID_CAPTURE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${VALID_HOST_SOURCE}
            -DSTAGE_SOURCE=${mutation_file}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(REMOVE "${mutation_file}")
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: stage route mutation was accepted")
    endif()
    string(FIND "${combined}" "Stage 11 validation input lacks production route"
        reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: missing Stage 11 route rejection, got: ${combined}")
    endif()
    string(FIND "${combined}" "${token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: missing rejected route token '${token}', got: ${combined}")
    endif()
endfunction()

expect_stage_route_rejection(missing_descent "session.request_descent(true)" FALSE)
expect_stage_route_rejection(missing_queue_action
    "session.queue_action(combat::Action::light)" FALSE)
expect_stage_route_rejection(missing_descent_with_brace_noise
    "session.request_descent(true)" TRUE)
expect_stage_route_rejection(missing_queue_action_with_brace_noise
    "session.queue_action(combat::Action::light)" TRUE)

message(STATUS "Stage 11 guard mutation self-test passed")
