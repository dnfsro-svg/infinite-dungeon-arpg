foreach(required GUARD_SCRIPT VALID_FIXTURE VALID_FORMAL VALID_CAPTURE
        VALID_HOST_HEADER VALID_HOST_SOURCE BAD_TEST_ACCESS BAD_CAPTURE_ORDER)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 11 guard self-test missing ${required}")
    endif()
endforeach()

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
    "const DeathInputGate death_gate = death_input_gate("
    "static_cast<void>(runtime.request_death_continue());\n            const DeathInputGate death_gate = death_input_gate("
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

message(STATUS "Stage 11 guard mutation self-test passed")
