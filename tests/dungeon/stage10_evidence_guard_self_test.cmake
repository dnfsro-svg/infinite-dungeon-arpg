foreach(required GUARD_SCRIPT VALID_FIXTURE VALIDATION_GAME_SOURCE FORMAL_SOURCE
        CAPTURE_SCRIPT FORMAL_CAPTURE_SCRIPT STRESS_SOURCE VALID_HOST_HEADER
        VALID_HOST_SOURCE BAD_CAPTURE_ORDER BAD_PRE_CAPTURE_DUMMY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 10 guard self-test missing ${required}")
    endif()
endforeach()

get_filename_component(_stage_directory "${VALID_HOST_HEADER}" DIRECTORY)
set(VALID_STAGE_SOURCE "${_stage_directory}/host_validation_stage10_11.cpp")
if(NOT EXISTS "${VALID_STAGE_SOURCE}")
    message(FATAL_ERROR "Stage 10 guard self-test missing production stage source")
endif()

function(expect_guard_rejection name host expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${host}
            -DSTAGE_SOURCE=${VALID_STAGE_SOURCE}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
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

function(extract_braced_function_block source signature output)
    string(FIND "${source}" "${signature}" function_begin)
    if(function_begin EQUAL -1)
        message(FATAL_ERROR "Stage 10 self-test function is missing: ${signature}")
    endif()
    string(SUBSTRING "${source}" ${function_begin} -1 function_tail)
    string(FIND "${function_tail}" "{" brace_relative)
    math(EXPR brace_open "${function_begin} + ${brace_relative}")
    string(LENGTH "${source}" source_length)
    math(EXPR source_last "${source_length} - 1")
    set(brace_depth 0)
    set(function_end -1)
    foreach(character_index RANGE ${brace_open} ${source_last})
        string(SUBSTRING "${source}" ${character_index} 1 character)
        if(character STREQUAL "{")
            math(EXPR brace_depth "${brace_depth} + 1")
        elseif(character STREQUAL "}")
            math(EXPR brace_depth "${brace_depth} - 1")
            if(brace_depth EQUAL 0)
                set(function_end ${character_index})
                break()
            endif()
        endif()
    endforeach()
    math(EXPR function_length "${function_end} - ${function_begin} + 1")
    string(SUBSTRING "${source}" ${function_begin} ${function_length} function_block)
    set(${output} "${function_block}" PARENT_SCOPE)
endfunction()

function(expect_stage_route_rejection name token)
    file(READ "${VALID_STAGE_SOURCE}" stage_source)
    extract_braced_function_block("${stage_source}"
        "combat::MovementInput stage10_validation_input(" stage10_input)
    string(REPLACE "${token}" "" mutated_input "${stage10_input}")
    if(mutated_input STREQUAL stage10_input)
        message(FATAL_ERROR "${name}: mutation token was not found")
    endif()
    string(REPLACE "${stage10_input}" "${mutated_input}" mutated_source
        "${stage_source}")
    set(mutation_file "${CMAKE_CURRENT_BINARY_DIR}/stage10_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
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
    string(FIND "${combined}" "Stage 10 validation input lacks production route"
        reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: missing Stage 10 route rejection, got: ${combined}")
    endif()
    string(FIND "${combined}" "${token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: missing rejected route token '${token}', got: ${combined}")
    endif()
endfunction()

expect_guard_rejection(capture_before_present "${BAD_CAPTURE_ORDER}"
    "Stage 10 presentation and capture must be owned by one helper")
expect_guard_rejection(capture_before_present_with_dummy
    "${BAD_PRE_CAPTURE_DUMMY}"
    "Stage 10 presentation and capture must be owned by one helper")
expect_stage_route_rejection(missing_descent "session.request_descent(true)")
expect_stage_route_rejection(missing_queue_action
    "session.queue_action(combat::Action::light)")

message(STATUS "Stage 10 guard mutation self-test passed")
