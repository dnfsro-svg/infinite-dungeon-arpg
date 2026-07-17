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

expect_guard_rejection(test_access "${BAD_TEST_ACCESS}" "${VALID_HOST_SOURCE}"
    "Forbidden Stage 11 evidence injection: DungeonSessionTestAccess")
set(public_mutations
    "checkpoint.next_state.death = fabricated_death"
    "checkpoint_ptr->death = fabricated_death"
    "death_snapshot.emplace(fabricated_death)"
    "death_snapshot = fabricated_death"
    "death_checkpoint.emplace(fabricated_death)"
    "death_checkpoint = fabricated_death"
    "auto death = make_death_checkpoint(fabricated_combat, room, target)")
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
expect_guard_rejection(capture_order "${VALID_FIXTURE}" "${BAD_CAPTURE_ORDER}"
    "Capture must occur once after EndDrawing")

message(STATUS "Stage 11 guard mutation self-test passed")
